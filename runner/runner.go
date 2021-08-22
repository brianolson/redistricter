package main

import (
	"archive/tar"
	"bufio"
	"bytes"
	"compress/gzip"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"io/ioutil"
	"log"
	"math/rand"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/brianolson/redistricter/runner/data"
	"github.com/brianolson/redistricter/runner/version"
)

var logfile = os.Stdout

// args for districter2
var argsWithParam = []string{
	"-P", "-B", "--pngout", "-d", "-i", "-U", "-g", "-o", "-r",
	"--loadSolution", "--distout", "--coordout",
	"--pngW", "--pngH", "--binLog", "-statLog", "--sLog", "--pLog",
	"--popRatioFactor", "--popRatioFactorEnd", "--popRatioFactorPoints",
	"--maxSpreadFraction", "--maxSpreadAbsolute", "--mppb",
	"--runDutySeconds", "--sleepDutySeconds",
}

// arg for running program
// can be standalone "--name" or ("--name" "val")
type arg struct {
	name   string
	val    string
	hasVal bool
}

func marg(name string) arg {
	return arg{name, "", false}
}

func aarg(name, val string) arg {
	return arg{name, val, true}
}

var defaultArgs = []arg{
	marg("--d2"),
	marg("--blankDists"),
	aarg("--popRatioFactorPoints", "0,1.4,30000,1.4,80000,500,100000,50,120000,500"),
	aarg("-g", "150000"),
	aarg("--statLog", "statlog"),
	//aarg("--binLog", "binlog"),
	aarg("--maxSpreadFraction", "0.01"),
	aarg("-o", "final.dsz"),
}

type Args struct {
	args []arg
}

func NewArgs() *Args {
	aa := new(Args)
	aa.args = make([]arg, len(defaultArgs), len(defaultArgs)+10)
	copy(aa.args, defaultArgs)
	return aa
}

func (a *Args) Arg(name, val string) *Args {
	a.args = append(a.args, aarg(name, val))
	return a
}

func (a *Args) Flag(f string) *Args {
	a.args = append(a.args, marg(f))
	return a
}

func (a *Args) Update(ca ConfigArgs) *Args {
	for k, v := range ca.Kwargs {
		found := false
		for i, aa := range a.args {
			if aa.name == k {
				a.args[i] = aarg(k, v)
				found = true
				break
			}
		}
		if !found {
			a.args = append(a.args, aarg(k, v))
		}
	}
	for _, sa := range ca.Args {
		a.args = append(a.args, marg(sa))
	}
	return a
}

func (a *Args) ToArray() []string {
	olen := 0
	for _, xa := range a.args {
		if xa.hasVal {
			olen += 2
		} else {
			olen += 1
		}
	}
	out := make([]string, olen)
	pos := 0
	for _, xa := range a.args {
		out[pos] = xa.name
		pos++
		if xa.hasVal {
			out[pos] = xa.val
			pos++
		}
	}
	return out
}

type ConfigArgs = data.ConfigArgs
type Config = data.Config
type AllConfig = data.AllConfig

func maybeFail(err error, errfmt string, params ...interface{}) {
	if err == nil {
		return
	}
	fmt.Fprintf(logfile, errfmt+"\n", params...)
	os.Exit(1)
}

type fetchError struct {
	url string
	err error
}

func (fe *fetchError) Error() string {
	return fmt.Sprintf("could not GET %v, %v", fe.url, fe.err)
}

func doFetch(url, path string) error {
	debug("%s -> %s", url, path)
	response, err := http.DefaultClient.Get(url)
	if err != nil {
		return &fetchError{url, err}
	}
	out, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("%s: could not create, %v", path, err)
	}
	_, err = io.Copy(out, response.Body)
	if err != nil {
		return fmt.Errorf("%s: could not write, %v", path, err)
	}
	err = out.Close()
	return err
}

func maybeFetch(url, path string, maxage time.Duration) error {
	fi, err := os.Stat(path)
	if err != nil {
		if os.IsNotExist(err) {
			return doFetch(url, path)
		} else {
			return err
		}
	}
	age := time.Now().Sub(fi.ModTime())
	if age > maxage {
		return doFetch(url, path)
	}
	return nil
}

type RunContext struct {
	clientDir string
	dataDir   string
	workDir   string
	binDir    string
	threads   int

	//local bool

	config AllConfig

	enabledConfigs map[string]Config

	weightSum float64

	notnice bool

	// use sync/atomic on this
	gracefulExit uint32

	restart bool // should exec self instead of exit

	ctx context.Context

	// subprocess state
	subpLock  sync.Mutex
	subpCond  *sync.Cond
	children  []*SolverThread
	finishes  []int // 0 = success, 1 = failure
	finishPos int
	wg        sync.WaitGroup

	best *BestDBJsFile
}

func (rc *RunContext) init() {
	rc.ctx = context.Background()
	rc.subpCond = sync.NewCond(&rc.subpLock)
	bestpath := filepath.Join(rc.workDir, "bestdb")
	var err error
	log.Printf("open best db: %v", bestpath)
	rc.best, err = OpenBestDB(bestpath)
	if err != nil {
		logerror("%s: %v", bestpath, err)
	}
	if rc.best == nil {
		panic("no best db")
	}
}

func (rc *RunContext) readDataDir() (configs map[string]Config) {
	ddf, err := os.Open(rc.dataDir)
	if err == os.ErrNotExist {
		err = os.MkdirAll(rc.dataDir, 0766)
		maybeFail(err, "could not mkdir -p dataDir=%v, %v", rc.dataDir, err)
	}
	defer ddf.Close()
	infos, err := ddf.Readdir(-1)
	if err == io.EOF {
		err = nil
	}
	maybeFail(err, "%s: could not read dir, %v", rc.dataDir, err)
	configs = make(map[string]Config)
	for _, fi := range infos {
		rc.readDataDirConfig(configs, fi)
	}
	if verbose {
		cj, _ := json.Marshal(configs)
		fmt.Fprintf(logfile, "found configs:\n%v\n", string(cj))
	}
	return
}

// part of readDataDir
func (rc *RunContext) readDataDirConfig(configs map[string]Config, fi os.FileInfo) {
	if !fi.IsDir() {
		return
	}
	ddConfigPath := filepath.Join(rc.dataDir, fi.Name(), "config")
	ddcdir, err := os.Open(ddConfigPath)
	if err == os.ErrNotExist {
		return
	}
	defer ddcdir.Close()
	cci, err := ddcdir.Readdir(-1)
	if err != nil {
		return
	}
	debug("found %s", ddConfigPath)
	for _, cfi := range cci {
		if !strings.HasSuffix(cfi.Name(), ".json") {
			continue
		}
		jspath := filepath.Join(ddConfigPath, cfi.Name())
		rc.readDataDirConfigJson(configs, jspath)
	}
}

// part of readDataDirConfig which is part of readDataDir
func (rc *RunContext) readDataDirConfigJson(configs map[string]Config, jspath string) {
	jsf, err := os.Open(jspath)
	if err != nil {
		logerror("could not read config json at %s: %v", jspath, err)
		return
	}
	defer jsf.Close()
	dec := json.NewDecoder(jsf)
	var nc Config
	err = dec.Decode(&nc)
	if err != nil {
		logerror("%s: bad json, %v", jspath, err)
		return
	}
	configs[nc.Name] = nc
}

func (rc *RunContext) sumWeights() {
	ws := float64(0.0)
	for _, config := range rc.config.Configs {
		ws += config.GetWeight()
	}
	rc.weightSum = ws
}

var needsEscape = " ?*"

// shell escape
func shescape(x string) string {
	xi := strings.IndexAny(x, needsEscape)
	if xi >= 0 {
		return "'" + x + "'"
	}
	return x
}

func fexists(path string) bool {
	_, err := os.Stat(path)
	// TODO: os.IsNotExist(err) ?
	return err == nil
}

func (rc *RunContext) debugCommandLines() {
	for _, config := range rc.config.Configs {
		rc.runConfig(config, true)
	}
}

func (rc *RunContext) downloadData(config Config) error {
	url := config.DataURL
	if url == "" {
		url = rc.config.StateDataURLs[config.State]
	}
	tarpath := filepath.Join(rc.dataDir, config.State+".tar.gz")
	err := maybeFetch(url, tarpath, 300*24*time.Hour)
	if err != nil {
		logerror("could not download %s from %s: %v", config.State, url, err)
		return err
	}
	fin, err := os.Open(tarpath)
	if err != nil {
		logerror("%s: %v", tarpath, err)
		return err
	}
	gzin, err := gzip.NewReader(fin)
	if err != nil {
		logerror("%s: %v", tarpath, err)
		return err
	}
	tf := tar.NewReader(gzin)
	for {
		th, err := tf.Next()
		if err != nil {
			if err == io.EOF {
				return nil
			}
			logerror("%s: error in tar file, %v", tarpath, err)
			return err
		}
		if th.Typeflag != tar.TypeReg {
			debug("ignore tar element %s", th.Name)
			continue
		}
		outpath := filepath.Join(rc.dataDir, th.Name)
		outdir := filepath.Dir(outpath)
		err = os.MkdirAll(outdir, 0755)
		if err != nil {
			logerror("%s: could not mkdir, %v", outdir, err)
			return err
		}
		debug("%s/%s -> %s", tarpath, th.Name, outpath)
		outf, err := os.OpenFile(outpath, os.O_CREATE|os.O_WRONLY, fs.FileMode(th.Mode))
		_, err = io.Copy(outf, tf)
		if err != nil {
			logerror("%s/%s -> %s: %v", tarpath, th.Name, outpath, err)
			return err
		}
	}
}

func (rc *RunContext) runConfig(config Config, dryrun bool) {
	// if not dryrun it must be called from inside rc.subpLock
	now := time.Now()
	timestamp := now.Format("20060102_150405")
	tmpdirName := fmt.Sprintf("%s_%04d", timestamp, rand.Intn(9999))
	workdir := filepath.Join(rc.workDir, config.Name, tmpdirName)
	stl := strings.ToLower(config.State)
	args := NewArgs().Update(config.Common).Update(config.Solver)
	pbpath := filepath.Join(rc.dataDir, config.State, stl+".pb")
	if !fexists(pbpath) {
		err := rc.downloadData(config)
		if err != nil {
			return
		}
	}
	args.Arg("-P", pbpath)
	if !rc.notnice {
		args.Arg("-nice", "19")
	}
	argStrings := args.ToArray()
	if verbose {
		dcmd := make([]string, len(argStrings))
		for i, p := range argStrings {
			dcmd[i] = shescape(p)
		}
		debug("%s: (mkdir -p %s && cd %s && %s/districter2 %s)", config.Name, workdir, workdir, rc.binDir, strings.Join(argStrings, " "))
	}
	if !dryrun {
		err := os.MkdirAll(workdir, 0755)
		if err != nil {
			rc.error("could not mkdir runner thread workdir %s, %v", workdir, err)
			return
		}
		st, err := NewSolverThread(rc.ctx, logfile, workdir, filepath.Join(rc.binDir, "districter2"), argStrings, config)
		if err != nil {
			rc.error("error starting solver, %v", err)
			return
		}
		rc.subpLock.Lock()
		rc.children = append(rc.children, st)
		debug("%d children active", len(rc.children))
		rc.subpLock.Unlock()
		rc.wg.Add(1)
		go rc.solverWaiter(st)
	}
}

func (rc *RunContext) solverWaiter(st *SolverThread) {
	go st.watchdog(6 * time.Hour)
	err := st.cmd.Wait()
	close(st.done)
	finish := time.Now()
	bestKmpp, statsum, sterr := rc.processStatlog(st)
	if err == nil && sterr != nil {
		logerror("%s/statlog: %s", st.cwd, sterr)
		err = sterr
	} else {
		fmt.Fprintf(logfile, "exited %s\n", st.cwd)
	}
	rc.maybeSendSolution(st, finish, err, bestKmpp, statsum)
	rc.best.Log(st, finish, err == nil, bestKmpp)
	rc.logFinish(st, bestKmpp, err)
	// TODO: remove .png files from old non-best attempts
}

func (rc *RunContext) maybeSendSolution(st *SolverThread, stopTime time.Time, solErr error, bestKmpp StatlogLine, statsum string) {
	// check bestKmpp against local best
	prevBest, err := rc.best.Get(st.config.Name)
	if err == nil && bestKmpp.Kmpp > prevBest.Kmpp {
		// nevermind
		return
	}
	rc.sendSolution(st.start, stopTime, st.config.Name, st.cwd, solErr, bestKmpp, statsum)
}
func (rc *RunContext) sendSolution(startTime, stopTime time.Time, cname, workdir string, solErr error, bestKmpp StatlogLine, statsum string) {
	if rc.config.PostURL == "" {
		debug("no \"post\" url set")
		return
	}
	result := ResultJSON{
		ConfigName: cname,
		Started:    toJTime(startTime),
		Timestamp:  toJTime(stopTime),
		Seconds:    stopTime.Sub(startTime).Seconds(),
		BestKmpp:   bestKmpp,
		Ok:         solErr == nil,
		Statsum:    statsum,
	}

	if solErr == nil {
		dszpath := filepath.Join(workdir, "bestKmpp.dsz")
		dsz, err := ioutil.ReadFile(dszpath)
		if err == nil {
			result.SetSolution(dsz)
		} else {
			logerror("%s: %v", dszpath, err)
			return
		}
	} else if statsum == "" {
		// nothing to send
		debug("%s: error and no statsum, nothing to POST\n", workdir)
		return
	}

	blob, err := json.Marshal(result)
	if err != nil {
		logerror("result json encode error: %v", err)
		return
	}
	br := bytes.NewReader(blob)
	hr, err := http.Post(rc.config.PostURL, "application/json", br)
	if err != nil {
		logerror("result post error: %v", err)
		return
	}
	if hr.StatusCode != 200 {
		msg := make([]byte, 1000)
		n, berr := hr.Body.Read(msg)
		if berr == nil || berr == io.EOF {
			logerror("post status %s, message: %s", hr.Status, string(msg[:n]))
		} else {
			logerror("post status %s", hr.Status)
		}
		return
	}

	err = rc.best.Put(cname, bestKmpp)
	if err != nil {
		logerror("local best db err: %v", err)
	}
}

// read statlog at end
// get best kmpp
// write statlog.gz
// remove original flat statlog
func (rc *RunContext) processStatlog(st *SolverThread) (bestKmpp StatlogLine, statsum string, err error) {
	statlogPath := filepath.Join(st.cwd, "statlog")
	// read, filter for bestkmpp, copy to .gz
	fin, err := os.Open(statlogPath)
	if err != nil {
		err = fmt.Errorf("%s: could not read statlog, %v", statlogPath, err)
		return
	}
	fout, err := os.Create(statlogPath + ".gz")
	if err != nil {
		err = fmt.Errorf("%s.gz: could not write statlog.gz, %v", statlogPath, err)
		return
	}
	gzout := gzip.NewWriter(fout)
	gp := GzipPipe{fin, fout, gzout}
	bestKmpp, statsum, err = statlogSummary(&gp)
	if err == ErrKmppNotFound {
		fe := gp.Finish()
		if fe == nil {
			os.Remove(statlogPath)
		}
		return
	} else if err != nil {
		err = fmt.Errorf("statlog line reading, %v", err)
		return
	}
	debug("%s: best kmpp %f", statlogPath, bestKmpp)
	err = gp.Finish()
	if err != nil {
		err = fmt.Errorf("statlog finish, %v", err)
		return
	}
	os.Remove(statlogPath)
	return
}

func (rc *RunContext) logFinish(st *SolverThread, bestKmpp StatlogLine, err error) {
	rc.subpLock.Lock()
	defer func() {
		// release runLoop to maybe start a new one
		rc.subpCond.Broadcast()
		rc.subpLock.Unlock()
	}()
	fval := 0
	if err != nil {
		fval = 1
	}
	if len(rc.finishes) >= rc.config.MaxFailuresDenominator {
		rc.finishes[rc.finishPos] = fval
	} else {
		rc.finishes = append(rc.finishes, fval)
	}
	if len(rc.finishes) >= rc.config.MaxFailuresDenominator {
		fc := 0
		for _, v := range rc.finishes {
			fc += v
		}
		if fc >= rc.config.MaxFailuresNumerator {
			logerror("too many failures, %d/%d", fc, len(rc.finishes))
			// TODO: _do_ something about too many failures?
		}
	}
	la := len(rc.children)
	for i, stv := range rc.children {
		if stv == st {
			if i != len(rc.children)-1 {
				rc.children[i] = rc.children[len(rc.children)-1]
			}
			rc.children[len(rc.children)-1] = nil // Go GC
			rc.children = rc.children[:len(rc.children)-1]
			rc.wg.Done()
			break
		}
	}
	lb := len(rc.children)
	debug("len(children) %d->%d", la, lb)
	if lb == la {
		logerror("%s exited but len(children) %d->%d", st.cwd, la, lb)
	}
}

func (rc *RunContext) listChildrenCopy() []*SolverThread {
	rc.subpLock.Lock()
	defer rc.subpLock.Unlock()
	out := make([]*SolverThread, len(rc.children))
	copy(out, rc.children)
	return out
}

func (rc *RunContext) randomConfig() Config {
	// weirdly doubly-random, by hash order and by deliberate randomness
	pick := rand.Float64() * rc.weightSum
	for _, config := range rc.enabledConfigs {
		cw := config.GetWeight()
		if pick < cw {
			return config
		}
		pick -= cw
	}
	// paranoid nonsense code in case something was wrong with weight?
	ipick := rand.Intn(len(rc.enabledConfigs))
	i := 0
	for _, config := range rc.enabledConfigs {
		if i < ipick {
			return config
		}
		i++
	}
	for _, config := range rc.enabledConfigs {
		return config
	}
	return Config{}
}

// check a stop file path to exist, return true if found
func (rc *RunContext) maybeStopFile(stopPath string) bool {
	_, err := os.Stat(stopPath)
	if err == nil {
		debug("saw stop file %s, quitting...", stopPath)
		atomic.StoreUint32(&rc.gracefulExit, 1)
		os.Remove(stopPath)
		return true
	}
	return false
}

// return true if we found a stop file
func (rc *RunContext) maybeStop() bool {
	if rc.maybeStopFile(filepath.Join(rc.workDir, "restart")) {
		rc.restart = true
		return true
	}
	if rc.clientDir != "" && rc.maybeStopFile(filepath.Join(rc.workDir, "restart")) {
		rc.restart = true
		return true
	}
	if rc.maybeStopFile(filepath.Join(rc.workDir, "stop")) {
		return true
	}
	if rc.clientDir != "" && rc.maybeStopFile(filepath.Join(rc.workDir, "stop")) {
		return true
	}
	return false
}

func (rc *RunContext) maybeStartSolver() bool {
	if rc.maybeStop() {
		debug("maybeStart no because stopping")
		return false
	}
	rc.subpLock.Lock()
	numChildren := len(rc.children)
	rc.subpLock.Unlock()
	if numChildren >= rc.threads {
		debug("maybeStart no have %d children of %d", numChildren, rc.threads)
		return false
	}
	nextc := rc.randomConfig()
	rc.runConfig(nextc, false)
	return true
}

// start a graceful exit (let running solvers finish)
func (rc *RunContext) quit() {
	atomic.StoreUint32(&rc.gracefulExit, 1)
	rc.subpLock.Lock()
	defer rc.subpLock.Unlock()
	rc.subpCond.Broadcast()
}

func (rc *RunContext) runLoop() {
	for atomic.LoadUint32(&rc.gracefulExit) == 0 {
		didStart := rc.maybeStartSolver()
		if didStart {
			time.Sleep(time.Second)
		} else {
			if atomic.LoadUint32(&rc.gracefulExit) != 0 {
				break
			}
			rc.subpLock.Lock()
			for (len(rc.children) >= rc.threads) && (atomic.LoadUint32(&rc.gracefulExit) == 0) {
				debug("runLoop len(children) = %d, waiting", len(rc.children))
				rc.subpCond.Wait()
			}
			rc.subpLock.Unlock()
		}
	}
	fmt.Fprintf(logfile, "runLoop exiting, winding down\n")
}

func (rc *RunContext) configPath() string {
	return filepath.Join(rc.workDir, "config.json")
}

func (rc *RunContext) loadConfig() {
	configpath := rc.configPath()
	fin, err := os.Open(configpath)
	if errors.Is(err, os.ErrNotExist) {
		return
	}
	maybeFail(err, "%s: could not read, %v", configpath, err)
	defer fin.Close()
	dec := json.NewDecoder(fin)
	err = dec.Decode(&rc.config)
	maybeFail(err, "%s: bad json, %v", configpath, err)
	rc.config.Normalize()
}

func (rc *RunContext) saveConfig() {
	configpath := rc.configPath()
	fout, err := os.Create(configpath)
	maybeFail(err, "%s: could not write, %v", configpath, err)
	defer fout.Close()
	enc := json.NewEncoder(fout)
	err = enc.Encode(rc.config)
	maybeFail(err, "%s: could not write, %v", configpath, err)
}

func (rc *RunContext) readServerConfig(fin io.Reader) error {
	dec := json.NewDecoder(fin)
	err := dec.Decode(&rc.config)
	if err != nil {
		return err
	}
	rc.config.Normalize()
	return nil
}

func (rc *RunContext) error(format string, params ...interface{}) {
	rc.quit()
	logerror(format, params...)
}

type SolverThread struct {
	cmd *exec.Cmd
	cwd string

	pstdout io.ReadCloser
	pstderr io.ReadCloser

	lstdout *bufio.Scanner
	lstderr *bufio.Scanner

	out io.Writer

	config Config

	start time.Time

	done chan int
}

func NewSolverThread(ctx context.Context, out io.Writer, cwd, bin string, args []string, config Config) (st *SolverThread, err error) {
	pcmd := exec.CommandContext(
		ctx,
		bin,
		args...,
	)
	pcmd.Dir = cwd
	pstdout, err := pcmd.StdoutPipe()
	if err != nil {
		err = fmt.Errorf("could not get exec stdout, %v", err)
		return
	}
	pstderr, err := pcmd.StderrPipe()
	if err != nil {
		err = fmt.Errorf("could not get exec stderr, %v", err)
		return
	}

	st = new(SolverThread)
	st.cmd = pcmd
	st.cwd = cwd
	st.pstdout = pstdout
	st.pstderr = pstderr
	st.lstdout = bufio.NewScanner(st.pstdout)
	st.lstderr = bufio.NewScanner(st.pstderr)
	st.out = out
	st.config = config
	st.start = time.Now()
	st.done = make(chan int)

	fmt.Fprintf(st.out, "starting %s\n", cwd)

	go st.prefixThread(st.lstdout, "O")
	go st.prefixThread(st.lstderr, "E")
	err = st.cmd.Start()
	return
}

func (st *SolverThread) prefixThread(lines *bufio.Scanner, prefix string) {
	for lines.Scan() {
		fmt.Fprintf(st.out, "%s %d %s\n", prefix, st.cmd.Process.Pid, lines.Text())
	}
}

func (st *SolverThread) watchdog(timeout time.Duration) {
	tc := time.NewTimer(timeout)
	select {
	case <-st.done:
		if !tc.Stop() {
			<-tc.C
		}
		return
	case <-tc.C:
		st.cmd.Process.Kill()
	}
}

type ResultJSON = data.ResultJSON

var verbose = false

func debug(format string, params ...interface{}) {
	if verbose {
		fmt.Fprintf(logfile, "debug: "+format+"\n", params...)
	}
}

func logerror(format string, params ...interface{}) {
	fmt.Fprintf(logfile, format+"\n", params...)
}

// e.g. io.fs.PathError
type nestedError interface {
	Unwrap() error
}

func main() {
	var (
		printAllCommandLines bool
		showBest             bool
		noRun                bool
		resendBest           bool
		httpAddr             string
		configURL            string
	)
	var rc RunContext
	pwd, err := os.Getwd()
	maybeFail(err, "pwd: %v", err)
	pwd, err = filepath.Abs(pwd)
	maybeFail(err, "pwd.abs: %v", err)
	flag.StringVar(&configURL, "url", "", "url to fetch bot config json from")
	flag.StringVar(&rc.clientDir, "dir", pwd, "dir to hold local data")
	flag.StringVar(&rc.dataDir, "data", "", "dir path with datasets")
	flag.StringVar(&rc.workDir, "work", "", "work dir path")
	flag.StringVar(&rc.binDir, "bin", "", "bin dir path")
	flag.StringVar(&httpAddr, "http", "", "[host]:port to serve on")
	flag.IntVar(&rc.threads, "threads", runtime.NumCPU(), "number of districter2 processes to run")
	//flag.BoolVar(&rc.local, "local", false, "run from local data")
	flag.BoolVar(&verbose, "verbose", false, "show debug log")
	flag.BoolVar(&rc.notnice, "full-prio", false, "run without `nice`")
	flag.BoolVar(&printAllCommandLines, "print-all-commands", false, "debug")
	flag.BoolVar(&showBest, "show-best", false, "show best runs")
	flag.BoolVar(&resendBest, "resend-best", false, "re-send best runs")
	flag.BoolVar(&noRun, "no-run", false, "don't actually run solver")
	// TODO: --diskQuota limit of combined rc.clientDir contents
	// TODO: --failuresPerSuccessesAllowed=5/2
	// TODO: include/exclude list of what things to run
	flag.Parse()

	debug("GOARCH=%s GOOS=%s version=%s", runtime.GOARCH, runtime.GOOS, version.Version)

	if rc.dataDir == "" {
		rc.dataDir = filepath.Join(rc.clientDir, "data")
	}
	if rc.workDir == "" {
		rc.workDir = filepath.Join(rc.clientDir, "work")
	}
	if rc.binDir == "" {
		rc.binDir = filepath.Join(rc.clientDir, "bin")
	}

	err = os.MkdirAll(rc.workDir, 0755)
	maybeFail(err, "%s: could not create work dir, %v", rc.workDir, err)

	rc.init()
	rc.config.Configs = rc.readDataDir()
	rc.loadConfig()

	if showBest {
		bests, err := rc.best.List()
		maybeFail(err, "could not load results from db, %v", err)
		for cname, sll := range bests {
			fmt.Printf("%s\t%f\n", cname, sll.Kmpp)
		}
		return
	}

	if configURL != "" {
		serverConfigPath := filepath.Join(rc.workDir, "server_config.json")
		err := maybeFetch(configURL, serverConfigPath, 23*time.Hour)
		if err != nil {
			if _, ok := err.(*fetchError); !ok {
				maybeFail(err, "could not fetch server config from %s, %v", rc.config.ConfigURL, err)
			}
		}
		fin, err := os.Open(serverConfigPath)
		maybeFail(err, "%s: bad server config, %v", serverConfigPath, err)
		err = rc.readServerConfig(fin)
		maybeFail(err, "%s: bad server config, %v", serverConfigPath, err)
		fin.Close()
	}

	args := flag.Args()
	rc.enabledConfigs = make(map[string]Config, len(rc.config.Configs))
	for name, conf := range rc.config.Configs {
		if conf.Disabled {
			if verbose {
				fmt.Fprintf(logfile, "%s disabled\n", name)
			}
			continue
		}
		if len(args) > 0 {
			hit := false
			for _, arg := range args {
				hit, err = filepath.Match(arg, name)
				if err == nil && hit {
					break
				}
			}
			if !hit {
				if verbose {
					fmt.Fprintf(logfile, "%s not in current run set\n", name)
				}
				continue
			}
		}
		rc.enabledConfigs[name] = conf
		if verbose {
			fmt.Fprintf(logfile, "%s enabled\n", name)
		}
	}
	debug("%d configs enabled of %d\n", len(rc.enabledConfigs), len(rc.config.Configs))
	rc.sumWeights()
	if printAllCommandLines {
		rc.debugCommandLines()
		return
	}

	if resendBest {
		bests, err := rc.best.ListFull()
		maybeFail(err, "could not load results from db, %v", err)
		for cname, flr := range bests {
			debug("post %s\n", flr.WorkDir)
			rc.sendSolution(TimeFromJava(flr.Started), TimeFromJava(flr.Timestamp), cname, flr.WorkDir, nil, flr.BestKmpp, "")
		}
		return
	}

	rc.saveConfig()
	var server http.Server
	if httpAddr != "" {
		sh := runServer{&rc}
		server = http.Server{
			Addr:    httpAddr,
			Handler: &sh,
		}
		go func() {
			log.Printf("serving on %s", httpAddr)
			httpErr := server.ListenAndServe()
			if httpErr != nil {
				fmt.Fprintf(logfile, "runner httpd exited: %v", httpErr)
			}
		}()

	}
	if noRun {
		if httpAddr == "" {
			return
		}
		for {
			time.Sleep(time.Hour)
		}
	}
	rc.runLoop()
	rc.wg.Wait()
	if httpAddr != "" {
		server.Close()
	}
	if rc.restart {
		execSelf()
	}
}

func execSelf() {
	args := os.Args
	selfpath, err := exec.LookPath(args[0])
	if err != nil {
		fmt.Fprintf(logfile, "%s: could not find on PATH, %v", args[0], err)
		return
	}
	env := os.Environ()
	err = syscall.Exec(selfpath, args, env)
	if err != nil {
		fmt.Fprintf(logfile, "%s: could not exec, %v", selfpath, err)
	}
}
