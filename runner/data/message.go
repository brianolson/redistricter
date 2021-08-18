package data

import (
	"encoding/base64"
	"encoding/json"
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
	ConfigURL string `json:"url,omitempty"`

	// where to send results
	PostURL string `json:"post,omitempty"`

	// time.Now().Unix()
	Timestamp int64 `json:"ts,omitempty"`

	// e.g. {"NM":"https://bot.bdistricting.com/2020/NM_1234.tar.gz", ...}
	StateDataURLs map[string]string `json:"durls,omitempty"`

	// Solver crash/fail should not exceed mfn/mfd
	MaxFailuresNumerator   int `json:"mfn,omitempty"`
	MaxFailuresDenominator int `json:"mfd,omitempty"`

	Overlays map[string]interface{} `json:"overlays,omitempty"`
}

func (ac *AllConfig) Normalize() {
	if ac.MaxFailuresDenominator == 0 {
		ac.MaxFailuresNumerator = 5
		ac.MaxFailuresDenominator = 11
	}
}
