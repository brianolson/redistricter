package main

import (
	"flag"
	"os"
	"strings"
	"testing"
)

var (
	testStatlogPath string
)

func TestMain(m *testing.M) {
	flag.StringVar(&testStatlogPath, "statlogpath", "", "path to statlog or statlog.gz")
	flag.Parse()
	os.Exit(m.Run())
}

func TestStatlogBestKmppFromFile(t *testing.T) {
	if testStatlogPath == "" {
		t.Skip("no -statlogpath")
		return
	}
	fin, err := OpenAny(testStatlogPath)
	maybeFatalf(t, err, "%s: could not open, %v", testStatlogPath, err)
	_, err = statlogBestKmpp(fin)
	maybeFatalf(t, err, "%s: bad statlog, %v", testStatlogPath, err)
}

func TestStatlogBestKmpp(t *testing.T) {
	const raw = `#Best Km/p: Km/p=41.261278 spread=1535.000000 std=424.780999 gen=50983
`
	fin := strings.NewReader(raw)
	_, err := statlogBestKmpp(fin)
	maybeFatalf(t, err, "%s: bad statlog, %v", testStatlogPath, err)
}

func maybeFatalf(t *testing.T, err error, format string, args ...interface{}) {
	if err == nil {
		return
	}
	t.Fatalf(format, args...)
}
