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
