package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"
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

func (a *Args) Update(ca *ConfigArgs) {
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
}

type ConfigArgs struct {
	Args   []string          `json:"args"`
	Kwargs map[string]string `json:"kwargs"`
}

type Config struct {
	// upper case postal abbreviation e.g. NC
	State string `json:"st"`
	// e.g. NC_Congress
	Name string `json:"name"`

	Solver ConfigArgs `json:"solve,omitempty"`
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
	fmt.Fprintf(os.Stderr, errfmt, params...)
	os.Exit(1)
}

func doFetch(url, path string) error {
	response, err := http.DefaultClient.Get(url)
	if err != nil {
		return fmt.Errorf("could not GET %v, %v", url, err)
	}
	out, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("%s: could not create, %v", path, err)
	}
	_, err = io.Copy(out, response.Body)
	if err != nil {
		return fmt.Errorf("%s: could not create, %v", path, err)
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
}

func (rc *RunContext) run() {
}

func (rc *RunContext) readDataDir() {
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
	configs := make(map[string]Config)
	for _, fi := range infos {
		rc.readDataDirConfig(configs, fi)
	}
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

func (rc *RunContext) runFromAvailableData() {
}

func logerror(format string, params ...interface{}) {
	fmt.Fprintf(os.Stderr, format, params...)
}

func main() {
	var (
		getURL    string = defaultURL
		clientDir string
	)
	var rc RunContext
	pwd, err := os.Getwd()
	maybeFail(err, "pwd: %v", err)
	pwd, err = filepath.Abs(pwd)
	maybeFail(err, "pwd.abs: %v", err)
	flag.StringVar(&getURL, "url", defaultURL, "url to fetch bot config json from")
	flag.StringVar(&clientDir, "dir", pwd, "dir to hold local data")
	flag.StringVar(&rc.dataDir, "data", "", "dir path with datasets")
	flag.StringVar(&rc.workDir, "work", "", "work dir path")
	flag.StringVar(&rc.binDir, "bin", "", "bin dir path")
	flag.IntVar(&rc.threads, "threads", runtime.NumCPU(), "number of districter2 processes to run")
	flag.BoolVar(&rc.local, "local", false, "run from local data")
	// TODO: --diskQuota
	// TODO: --failuresPerSuccessesAllowed=5/2
	// TODO: include/exclude list of what things to run
	nicePath, err := exec.LookPath("nice")
	fmt.Printf("nice=%s err=%v\n", nicePath, err)

	if rc.dataDir == "" {
		rc.dataDir = filepath.Join(clientDir, "data")
	}
	if rc.workDir == "" {
		rc.workDir = filepath.Join(clientDir, "work")
	}
	if rc.binDir == "" {
		rc.binDir = filepath.Join(clientDir, "bin")
	}
}
