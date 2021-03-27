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

type statlogLine struct {
	kmpp   float64
	spread float64
	std    float64
}

var bestKmppRe *regexp.Regexp

func init() {
	bestKmppRe = regexp.MustCompile("Best Km/p: Km/p=([0-9.]+)\\s+spread=([0-9.]+)\\s+std=([0-9.]+)")
}

var ErrKmppNotFound error = errors.New("best Km/p not found")

// find log line:
// #Best Km/p: Km/p={fake_kmpp} spread=1535.000000 std=424.780999 gen=50983

func statlogBestKmpp(fin io.Reader) (bestKmpp statlogLine, err error) {
	scanner := bufio.NewScanner(fin)
	for scanner.Scan() {
		line := scanner.Text()
		parts := bestKmppRe.FindStringSubmatch(line)
		if parts != nil {
			bestKmpp.kmpp, err = strconv.ParseFloat(parts[1], 64)
			if err != nil {
				return
			}
			bestKmpp.spread, err = strconv.ParseFloat(parts[2], 64)
			if err != nil {
				return
			}
			bestKmpp.std, err = strconv.ParseFloat(parts[3], 64)
			return
		}
	}
	err = scanner.Err()
	if err == nil {
		err = ErrKmppNotFound
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
