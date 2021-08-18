package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"net/http"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/brianolson/redistricter/runner/data"
)

var (
	serveAddr string
	dataDir   string
	rp        resultPather
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
}

func main() {
	flag.StringVar(&serveAddr, "addr", ":7319", "Server Addr")
	flag.StringVar(&dataDir, "dir", "", "data dir")
	flag.Parse()

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
