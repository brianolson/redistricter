package data

import (
	"encoding/base64"
)

type ResultJSON struct {
	ConfigName string      `json:"n"`
	Started    int64       `json:"s"`
	Timestamp  int64       `json:"t"` // finish time
	Seconds    float64     `json:"r"` // run time
	BestKmpp   StatlogLine `json:"b"`
	Ok         bool        `json:"ok"`

	SolutionB64 string `json:"bestKmpp.dsz"`
	Statsum     string `json:"statsum"`

	BinlogB64 string `json:"binlog"` // deprecated
}

func (r *ResultJSON) SetSolution(dsz []byte) {
	r.SolutionB64 = b64(dsz)
}

func (r *ResultJSON) SetBinlog(binlog []byte) {
	r.BinlogB64 = b64(binlog)
}

func b64(blob []byte) string {
	return base64.StdEncoding.EncodeToString(blob)
}

type StatlogLine struct {
	Kmpp   float64
	Spread float64
	Std    float64
}
