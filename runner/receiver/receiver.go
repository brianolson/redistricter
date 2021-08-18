package main

import (
	"bufio"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"math/rand"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/brianolson/forwarded"
	"github.com/brianolson/redistricter/runner/data"
)

var (
	serveAddr string
	dataDir   string
	rp        resultPather
	hs        *hashSet
)

type ErrJSON struct {
	Error string `json:"error"`
}

func jsonResponse(out http.ResponseWriter, code int, v interface{}) {
	out.Header().Set("Content-Type", "application/json")
	out.WriteHeader(code)
	enc := json.NewEncoder(out)
	enc.Encode(v) // TODO: log err?
}

func maybeErrJson(err error, out http.ResponseWriter, code int, format string, args ...interface{}) bool {
	if err == nil {
		return false
	}
	msg := fmt.Sprintf(format, args...)
	jsonResponse(out, code, ErrJSON{msg})
	return true
}

func configHandler(out http.ResponseWriter, request *http.Request) {
	confPath := filepath.Join(dataDir, "config.json")
	jsbytes, err := ioutil.ReadFile(confPath)
	var config data.AllConfig
	err = json.Unmarshal(jsbytes, &config)
	if maybeErrJson(err, out, 500, "bad config.json: %v", err) {
		return
	}
	sconfPath := filepath.Join(dataDir, "server.json")
	jsbytes, err = ioutil.ReadFile(sconfPath)
	var sconf data.AllConfig
	err = json.Unmarshal(jsbytes, &sconf)
	if maybeErrJson(err, out, 500, "bad server.json: %v", err) {
		return
	}

	if sconf.ConfigURL != "" {
		config.ConfigURL = sconf.ConfigURL
	}
	if sconf.PostURL != "" {
		config.PostURL = sconf.PostURL
	}
	if len(sconf.StateDataURLs) != 0 {
		config.StateDataURLs = sconf.StateDataURLs
	}

	outConfBlob, err := json.Marshal(config)
	if maybeErrJson(err, out, 500, "config -> json: %v", err) {
		return
	}
	if len(sconf.Overlays) != 0 {
		var cd map[string]interface{}
		err = json.Unmarshal(outConfBlob, &cd)
		if maybeErrJson(err, out, 500, "config json -> map: %v", err) {
			return
		}
		for pathstr, v := range sconf.Overlays {
			path := strings.Split(pathstr, ".")
			d := cd
			for i := 0; i < len(path)-1; i++ {
				pe := path[i]
				ndi, ok := d[pe]
				if !ok {
					nd := make(map[string]interface{})
					d[pe] = nd
					d = nd
				} else {
					d = ndi.(map[string]interface{})
				}
			}
			d[path[len(path)-1]] = v
		}
		outConfBlob, err = json.Marshal(cd)
		if maybeErrJson(err, out, 500, "config map -> json: %v", err) {
			return
		}
	}
	out.WriteHeader(http.StatusOK)
	out.Write(outConfBlob)
}

func putHandler(out http.ResponseWriter, request *http.Request) {
	if request.Method != "POST" {
		jsonResponse(out, 400, "not POST")
		return
	}
	if request.Header.Get("Content-Type") != "application/json" {
		jsonResponse(out, 400, "wrong Content-Type")
		return
	}
	var req data.ResultJSON
	fin := http.MaxBytesReader(out, request.Body, 10000000)
	raw, err := io.ReadAll(fin)
	if err != nil {
		return
	}
	err = json.Unmarshal(raw, &req)
	if maybeErrJson(err, out, 400, "bad result json: %v", err) {
		return
	}
	request.Body.Close()
	value := req.SolutionB64
	if value == "" {
		value = req.Statsum
	}
	if value == "" {
		jsonResponse(out, 400, "empty result")
		return
	}
	vhash := sha256.Sum256([]byte(value))
	collision := hs.insertOrRejectPending(vhash[:])
	if collision {
		jsonResponse(out, 200, "already received")
		return
	}
	defer hs.rollback(vhash[:]) // nop if committed
	rhost := forwarded.FirstForwardedFor(forwarded.ParseHeaders(request.Header))
	if rhost == "" {
		rhost = request.RemoteAddr
	}
	pos := strings.LastIndexByte(rhost, ':')
	if pos != -1 {
		rhost = rhost[:pos]
	}
	outpath := rp.makeEventId(rhost)
	outpath = filepath.Join(dataDir, outpath)
	outdir := filepath.Dir(outpath)
	err = os.MkdirAll(outdir, 0755)
	if maybeErrJson(err, out, 500, "%s: could not make upload dir: %v", outdir, err) {
		return
	}
	err = ioutil.WriteFile(outpath, raw, 0444)
	if maybeErrJson(err, out, 500, "%s: could not write upload: %v", outpath, err) {
		return
	}
	hs.commit(vhash[:])
	jsonResponse(out, 200, "ok")
}

func main() {
	flag.StringVar(&serveAddr, "addr", ":7319", "Server Addr")
	flag.StringVar(&dataDir, "dir", "", "data dir")
	flag.Parse()

	hspath := filepath.Join(dataDir, "seen")
	var err error
	hs, err = openHashSet(hspath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "%s: could not open hash set, %v", hspath, err)
		os.Exit(1)
		return
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/config.json", configHandler)
	mux.HandleFunc("/put", putHandler)

	server := &http.Server{
		Addr:    serveAddr,
		Handler: mux,
	}
	log.Print("serving on ", serveAddr)
	log.Fatal(server.ListenAndServe())
}

type resultPather struct {
	dayn       int
	day        string
	group      string
	groupCount int

	l sync.Mutex
}

const suffixLetters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0213456789"

func (rp *resultPather) makeEventId(ipAddr string) string {
	rp.l.Lock()
	defer rp.l.Unlock()

	now := time.Now()
	if rp.day == "" || (rp.dayn != now.Day()) {
		rp.day = now.Format("20060102")
		rp.dayn = now.Day()
	}
	hms := now.Format("150405")
	if rp.group == "" || (rp.groupCount >= 1000) {
		rp.group = hms
		rp.groupCount = 0
	}
	rp.groupCount++
	var rc3 [3]byte
	for i := 0; i < len(rc3); i++ {
		rc3[i] = suffixLetters[rand.Intn(len(suffixLetters))]
	}
	fname := fmt.Sprintf("%s_%s_%s", hms, ipAddr, string(rc3[:]))
	return filepath.Join(rp.day, rp.group, fname)
}

type hashSet struct {
	path string
	out  io.WriteCloser

	pending map[string]bool
	they    map[string]bool

	l sync.Mutex
}

func openHashSet(path string) (*hashSet, error) {
	hs := new(hashSet)
	hs.path = path
	fin, err := os.Open(path)
	doread := true
	if err != nil {
		doread = !os.IsNotExist(err)
		if doread {
			return nil, err
		}
	}
	hs.they = make(map[string]bool, 1000)
	hs.pending = make(map[string]bool, 10)
	if doread {
		scanner := bufio.NewScanner(fin)
		for scanner.Scan() {
			line := scanner.Text()
			h, err := base64.StdEncoding.DecodeString(line)
			if err != nil {
				return nil, err
			}
			hh := string(h)
			hs.they[hh] = true
		}
	}
	hs.out, err = os.OpenFile(path, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0644)
	if err != nil {
		return nil, err
	}
	return hs, nil
}

func (hs *hashSet) insertOrRejectPending(h []byte) bool {
	hh := string(h)
	hs.l.Lock()
	defer hs.l.Unlock()
	if hs.they[hh] {
		return true
	}
	if hs.pending[hh] {
		return true
	}
	hs.pending[hh] = true
	return false
}

// return true on collision
// otherwise return false and insert into the set
func (hs *hashSet) commit(h []byte) {
	hh := string(h)
	hs.l.Lock()
	defer hs.l.Unlock()
	delete(hs.pending, hh)
	hos := base64.StdEncoding.EncodeToString(h)
	fmt.Fprintf(hs.out, "%s\n", hos)
	hs.they[hh] = true
}

func (hs *hashSet) rollback(h []byte) {
	hh := string(h)
	hs.l.Lock()
	defer hs.l.Unlock()
	delete(hs.pending, hh)
}
