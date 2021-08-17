package main

import (
	"bufio"
	"compress/gzip"
	"errors"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"
)

type StatlogLine struct {
	Kmpp   float64
	Spread float64
	Std    float64
}

var bestKmppRe *regexp.Regexp

func init() {
	bestKmppRe = regexp.MustCompile("Best Km/p: Km/p=([0-9.]+)\\s+spread=([0-9.]+)\\s+std=([0-9.]+)")
}

var ErrKmppNotFound error = errors.New("best Km/p not found")

// gather lines at end of log prefixed with '#'
// parse log line:
// #Best Km/p: Km/p={fake_kmpp} spread=1535.000000 std=424.780999 gen=50983
func statlogSummary(fin io.Reader) (bestKmpp StatlogLine, statsum string, err error) {
	tail := newTail(50)
	sumlines := make([]string, 0, 10)
	scanner := bufio.NewScanner(fin)
	hit := false
	for scanner.Scan() {
		line := scanner.Text()
		tail.Push(line)
		if len(line) > 0 && line[0] == '#' {
			sumlines = append(sumlines, line)
		}
		parts := bestKmppRe.FindStringSubmatch(line)
		if parts != nil {
			bestKmpp.Kmpp, err = strconv.ParseFloat(parts[1], 64)
			if err != nil {
				return
			}
			bestKmpp.Spread, err = strconv.ParseFloat(parts[2], 64)
			if err != nil {
				return
			}
			bestKmpp.Std, err = strconv.ParseFloat(parts[3], 64)
			if err != nil {
				return
			}
			hit = true
			return
		}
	}
	statsum = strings.Join(sumlines, "\n")
	err = scanner.Err()
	if err != nil {
		return
	}
	if !hit {
		err = ErrKmppNotFound
		statsum = strings.Join(tail.Tail(), "\n")
		return
	}
	return
}

/*
early log (some in no district)

gen 600: 267839 in no district (pop=9284963) 9.0261786208 Km/person
population avg=19271 std=10140.9895
max=36703 (dist# 1)  min=5089 (dist# 5)  median=15747 (dist# 12)
kmpp var per 601=1084.556321, spread var per 601=0.042117

later log

generation 107300: 41.529453028 Km/person
population avg=733499 std=266.437417
max=733885 (dist# 10)  min=732896 (dist# 4)  median=733589 (dist# 12)
kmpp var per 5000=0.001357, spread var per 5000=0.002679
*/

type subReadCloser struct {
	cur io.ReadCloser
	sub io.ReadCloser
}

func (src *subReadCloser) Read(b []byte) (n int, err error) {
	return src.cur.Read(b)
}

func (src *subReadCloser) Close() error {
	src.cur.Close()
	return src.sub.Close()
}

func OpenAny(path string) (io.ReadCloser, error) {
	if strings.HasSuffix(path, ".gz") {
		fin, err := os.Open(path)
		if err != nil {
			return nil, err
		}
		gzin, err := gzip.NewReader(fin)
		if err != nil {
			return nil, err
		}
		return &subReadCloser{gzin, fin}, nil
	}
	return os.Open(path)
}

// circular buffer to keep last N lines
type linebuf struct {
	lines  []string
	lineno int
}

func newTail(lines int) linebuf {
	return linebuf{lines: make([]string, lines)}
}

func (tail *linebuf) Push(line string) {
	tail.lines[tail.lineno%len(tail.lines)] = line
	tail.lineno++
}

func (tail *linebuf) Tail() []string {
	if tail.lineno < len(tail.lines) {
		// never wrapped
		return tail.lines[:tail.lineno]
	}
	out := make([]string, len(tail.lines))
	outpos := 0
	pos := (tail.lineno + 1) % len(tail.lines)
	end := (tail.lineno) % len(tail.lines)
	for {
		out[outpos] = tail.lines[pos]
		if pos == end {
			break
		}
		outpos++
		pos = (pos + 1) % len(tail.lines)
	}
	return out
}
