package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/brianolson/redistricter/runner/version"
)

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
	aarg("--binLog", "binlog"),
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

type ConfigArgs struct {
	Args   []string          `json:"args,omitempty"`
	Kwargs map[string]string `json:"kwargs,omitempty"`
}

// implement json.Marshaler
// except it seems to not make any difference :-/
func (ca *ConfigArgs) MarshalJSON() ([]byte, error) {
	if len(ca.Args) == 0 && len(ca.Kwargs) == 0 {
		return nil, nil
	}
	return json.Marshal(ca)
}

type Config struct {
	// upper case postal abbreviation e.g. NC
	State string `json:"st"`
	// e.g. NC_Congress
	Name string `json:"name"`

	Solver ConfigArgs `json:"solver,omitempty"`
	Drend  ConfigArgs `json:"drend,omitempty"`
	Common ConfigArgs `json:"common,omitempty"`

	// unset 0.0 becomes 1.0
	// use "dibaled:true" to disable entirely.
	Weight float64 `json:"weight,omitempty"`

	Disabled bool `json:"disabled,omitempty"`

	// SendAnything even invalid results (for distributed debugging)
	SendAnything bool `json:"sendAnything,omitempty"`

	// KmppSendThreshold only send results better than this
	KmppSendThreshold float64 `json:"maxkmpp,omitempty"`

	// SpreadSendThreshold only send results better than this
	SpreadSendThreshold float64 `json:"maxspread,omitempty"`

	// DataURL where to fetch data
	DataURL string `json:"data,omitempty"`
}

func (config *Config) GetWeight() float64 {
	if config.Weight <= 0.000001 {
		return 1.0
	}
	return config.Weight
}

type AllConfig struct {
	// .Configs[config.Name] = config
	Configs map[string]Config `json:"c"`

	// where to get the next version of this config
	ConfigURL string `json:"url"`

	// where to send results
	PostURL string `json:"post"`

	// time.Now().Unix()
	Timestamp int64 `json:"ts"`

	// e.g. {"NM":"https://bot.bdistricting.com/2020/NM_1234.tar.gz", ...}
	StateDataURLs map[string]string
}

const defaultURL = "https://bdistricting.com/bot/2020.json"

func maybeFail(err error, errfmt string, params ...interface{}) {
	if err == nil {
		return
	}
	fmt.Fprintf(os.Stderr, errfmt+"\n", params...)
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
		if err != os.ErrNotExist {
			return err
		} else {
			return doFetch(url, path)
		}
	}
	age := time.Now().Sub(fi.ModTime())
	if age > maxage {
		return doFetch(url, path)
	}
	return nil
}

type RunContext struct {
	dataDir string
	workDir string
	binDir  string
	threads int

	local bool

	config AllConfig

	weightSum float64

	notnice bool
}

func (rc *RunContext) run() {
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
		fmt.Fprintf(os.Stderr, "found configs:\n%v\n", string(cj))
	}
	return
}
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

func (rc *RunContext) debugCommandLines() {
	for _, config := range rc.config.Configs {
		rc.runConfig(config, true)
	}
}

func (rc *RunContext) runConfig(config Config, dryrun bool) {
	now := time.Now()
	timestamp := now.Format("20060102_150405")
	tmpdirName := fmt.Sprintf("%s_%04d", timestamp, rand.Intn(9999))
	workdir := filepath.Join(rc.workDir, config.Name, tmpdirName)
	stl := strings.ToLower(config.State)
	args := NewArgs().Update(config.Common).Update(config.Solver)
	args.Arg("-P", filepath.Join(rc.dataDir, config.State, stl+".pb"))
	if !rc.notnice {
		args.Arg("-nice", "19")
	}
	cmd := args.ToArray()
	if verbose {
		dcmd := make([]string, len(cmd))
		for i, p := range cmd {
			dcmd[i] = shescape(p)
		}
		debug("%s: (mkdir -p %s && cd %s && %s/districter2 %s)", config.Name, workdir, workdir, rc.binDir, strings.Join(dcmd, " "))
	}
	if !dryrun {
		logerror("TODO: actually run the command")
	}
}

func (rc *RunContext) randomConfig() Config {
	// weirdly doubly-random, by hash order and by deliberate randomness
	pick := rand.Float64() * rc.weightSum
	for _, config := range rc.config.Configs {
		cw := config.GetWeight()
		if pick < cw {
			return config
		}
		pick -= cw
	}
	// paranoid nonsense code in case something was wrong with weight?
	ipick := rand.Intn(len(rc.config.Configs))
	i := 0
	for _, config := range rc.config.Configs {
		if i < ipick {
			return config
		}
		i++
	}
	for _, config := range rc.config.Configs {
		return config
	}
	return Config{}
}

func (rc *RunContext) runFromAvailableData() {
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
	return dec.Decode(&rc.config)
}

var verbose = false

func debug(format string, params ...interface{}) {
	if verbose {
		fmt.Fprintf(os.Stderr, "debug: "+format+"\n", params...)
	}
}

func logerror(format string, params ...interface{}) {
	fmt.Fprintf(os.Stderr, format+"\n", params...)
}

// e.g. io.fs.PathError
type nestedError interface {
	Unwrap() error
}

func main() {
	var (
		clientDir string
	)
	var rc RunContext
	pwd, err := os.Getwd()
	maybeFail(err, "pwd: %v", err)
	pwd, err = filepath.Abs(pwd)
	maybeFail(err, "pwd.abs: %v", err)
	flag.StringVar(&rc.config.ConfigURL, "url", defaultURL, "url to fetch bot config json from")
	flag.StringVar(&clientDir, "dir", pwd, "dir to hold local data")
	flag.StringVar(&rc.dataDir, "data", "", "dir path with datasets")
	flag.StringVar(&rc.workDir, "work", "", "work dir path")
	flag.StringVar(&rc.binDir, "bin", "", "bin dir path")
	flag.IntVar(&rc.threads, "threads", runtime.NumCPU(), "number of districter2 processes to run")
	flag.BoolVar(&rc.local, "local", false, "run from local data")
	flag.BoolVar(&verbose, "verbose", false, "show debug log")
	flag.BoolVar(&rc.notnice, "full-prio", false, "run without `nice`")
	// TODO: --diskQuota limit of combined clientDir contents
	// TODO: --failuresPerSuccessesAllowed=5/2
	// TODO: include/exclude list of what things to run
	flag.Parse()

	debug("GOARCH=%s GOOS=%s version=%s", runtime.GOARCH, runtime.GOOS, version.Version)

	if rc.dataDir == "" {
		rc.dataDir = filepath.Join(clientDir, "data")
	}
	if rc.workDir == "" {
		rc.workDir = filepath.Join(clientDir, "work")
	}
	if rc.binDir == "" {
		rc.binDir = filepath.Join(clientDir, "bin")
	}

	err = os.MkdirAll(rc.workDir, 0755)
	maybeFail(err, "%s: could not create work dir, %v", rc.workDir, err)

	rc.config.Configs = rc.readDataDir()
	rc.loadConfig()

	if !rc.local {
		logerror("TODO: get config URL")
		serverConfigPath := filepath.Join(rc.workDir, "server_config.json")
		err := maybeFetch(rc.config.ConfigURL, serverConfigPath, 23*time.Hour)
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
	rc.debugCommandLines()
	rc.saveConfig()
}
