package main

import (
	"embed"
	"encoding/json"
	"fmt"
	"html/template"
	"log"
	"net/http"
	"strings"
	"time"
)

//go:embed templates
var tfs embed.FS

var templates *template.Template

func init() {
	nt, err := template.ParseFS(tfs, "templates/*.html")
	if err != nil {
		panic(fmt.Sprintf("could not load templates, %v", err))
	}
	t := nt.Lookup("index.html")
	if t == nil {
		panic("no index.html template")
	}
	templates = nt
}

func textResponse(code int, response http.ResponseWriter, format string, args ...interface{}) {
	response.Header().Set("Content-Type", "text/plain")
	response.WriteHeader(code)
	fmt.Fprintf(response, format, args...)
}

// return true if Handler should exit
func httpErr(err error, code int, response http.ResponseWriter, format string, args ...interface{}) bool {
	if err == nil {
		return false
	}
	textResponse(code, response, format, args...)
	return true
}

type runServer struct {
	rc *RunContext
}

func (rs *runServer) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	log.Print(request.URL.Path)
	if request.URL.Path == "/best" {
		rs.showBest(response, request)
		return
	}
	if request.URL.Path == "/running" {
		rs.showRunning(response, request)
		return
	}
	textResponse(404, response, "nope")
}
func (rs *runServer) showBest(response http.ResponseWriter, request *http.Request) {
	sb := strings.Builder{}
	bests, err := rs.rc.best.List()
	if httpErr(err, 500, response, "could not load list, %v", err) {
		return
	}
	//maybeFail(err, "%s: could not load results from db, %v", rs.rc.best.path, err)
	if true {
		blob, err := json.Marshal(bests)
		if httpErr(err, 500, response, "could not json, %v", err) {
			return
		}
		response.Header().Set("Content-Type", "application/json")
		response.WriteHeader(http.StatusOK)
		response.Write(blob)
	} else {
		for cname, sll := range bests {
			fmt.Fprintf(&sb, "%s\t%f\n", cname, sll.Kmpp)
		}
		response.Header().Set("Content-Type", "text/plain")
		response.WriteHeader(http.StatusOK)
		response.Write([]byte(sb.String()))
	}
}

func (rs *runServer) showIndex(response http.ResponseWriter, request *http.Request) {
	t := templates.Lookup("index.html")
	if t == nil {
		panic("no index.html template")
	}
}

type RunningRecord struct {
	Cwd      string `json:"cwd"`
	Start    string `json:"s"`
	StartInt int64  `json:"sn"`
	Config   Config `json:"cfg"`
}

func (rs *runServer) showRunning(response http.ResponseWriter, request *http.Request) {
	children := rs.rc.listChildrenCopy()

	out := make([]RunningRecord, len(children))
	for i, ch := range children {
		out[i].Cwd = ch.cwd
		out[i].StartInt = ch.start.Unix()
		out[i].Start = ch.start.Format(time.RFC3339)
		out[i].Config = ch.config
	}
	blob, err := json.Marshal(out)
	if httpErr(err, 500, response, "could not json, %v", err) {
		return
	}
	response.Header().Set("Content-Type", "application/json")
	response.WriteHeader(http.StatusOK)
	response.Write(blob)
}
