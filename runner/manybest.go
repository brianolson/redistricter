package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"time"

	"encoding/json"
)

type BestDB interface {
	Put(configName string, stat StatlogLine) error
	Get(configName string) (stat StatlogLine, err error)
	List() (bests map[string]StatlogLine, err error)
	Log(st *SolverThread, stopTime time.Time, ok bool, stat StatlogLine) error
	Close()
}

type BestDBJsFile struct {
	path string

	out io.WriteCloser

	bests map[string]FinishLogRecord
}

func OpenBestDB(path string) (bdb *BestDBJsFile, err error) {
	best := new(BestDBJsFile)
	best.path = path
	best.bests = make(map[string]FinishLogRecord, 100)
	err = best.read()
	if err != nil {
		return nil, err
	}
	best.out, err = os.OpenFile(best.path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		return nil, err
	}
	return best, nil
}

func (best *BestDBJsFile) read() error {
	fin, err := os.Open(best.path)
	if err != nil {
		// probably just not there.
		return nil
	}
	lines := bufio.NewScanner(fin)
	lineno := 0
	for lines.Scan() {
		lineno++
		line := lines.Text()
		var rec FinishLogRecord
		err = json.Unmarshal([]byte(line), &rec)
		if err != nil {
			return fmt.Errorf("%s:%d bad json, %v", best.path, lineno, err)
		}
		if !rec.Ok {
			continue
		}
		if rec.BestKmpp.Kmpp < 0.000001 {
			continue
		}
		prev, any := best.bests[rec.ConfigName]
		if (!any) || (prev.BestKmpp.Kmpp > rec.BestKmpp.Kmpp) {
			best.bests[rec.ConfigName] = rec
		}
	}
	return nil
}

func (best *BestDBJsFile) Put(configName string, stat StatlogLine) error {
	// nop, wait for full record added at Log()
	//best.bests[configName] = stat
	return nil
}

func (best *BestDBJsFile) Get(configName string) (stat StatlogLine, err error) {
	stat = best.bests[configName].BestKmpp
	return
}

func (best *BestDBJsFile) List() (bests map[string]StatlogLine, err error) {
	bests = make(map[string]StatlogLine, len(best.bests))
	for k, v := range best.bests {
		bests[k] = v.BestKmpp
	}
	return bests, nil
}

func (best *BestDBJsFile) ListFull() (bests map[string]FinishLogRecord, err error) {
	bests = make(map[string]FinishLogRecord, len(best.bests))
	for k, v := range best.bests {
		bests[k] = v
	}
	return bests, nil
}

type FinishLogRecord struct {
	ConfigName string      `json:"n"`
	Started    int64       `json:"s"`
	Timestamp  int64       `json:"t"` // finish time
	Seconds    float64     `json:"r"` // run time
	WorkDir    string      `json:"d"`
	BestKmpp   StatlogLine `json:"b"`
	Ok         bool        `json:"ok"`
}

func (best *BestDBJsFile) Log(st *SolverThread, stopTime time.Time, ok bool, stat StatlogLine) error {
	rec := FinishLogRecord{
		ConfigName: st.config.Name,
		Started:    toJTime(st.start),
		Timestamp:  toJTime(stopTime),
		Seconds:    stopTime.Sub(st.start).Seconds(),
		WorkDir:    st.cwd,
		BestKmpp:   stat,
		Ok:         ok,
	}
	if ok {
		prev, any := best.bests[rec.ConfigName]
		if (!any) || ((rec.BestKmpp.Kmpp > 0.000001) && (prev.BestKmpp.Kmpp > rec.BestKmpp.Kmpp)) {
			best.bests[rec.ConfigName] = rec
		}
	}
	blob, err := json.Marshal(rec)
	if err != nil {
		return err
	}
	blob = append(blob, '\n')
	_, err = best.out.Write(blob)
	if err != nil {
		return err
	}
	return nil
}

func (best *BestDBJsFile) Close() {
	if best.out != nil {
		best.out.Close()
		best.out = nil
	}
}

/*
type BestDBBolt struct {
	path string
	db   *bolt.DB
}

func OpenBestDBBolt(path string) (bdb BestDB, err error) {
	db, err := bolt.Open(path, 0600, nil)
	if err != nil {
		return nil, err
	}
	log.Printf("opened manybest: %v", path)
	err = db.Update(func(tx *bbolt.Tx) error {
		_, err := tx.CreateBucketIfNotExists(bestkey)
		if err != nil {
			return err
		}
		_, err = tx.CreateBucketIfNotExists(logkey)
		return err
	})
	if err != nil {
		return nil, fmt.Errorf("could not bests db, %v", err)
	}
	return &BestDBBolt{path, db}, nil
}

var bestkey = []byte("b")
var logkey = []byte("l")

func (best *BestDBBolt) Put(configName string, stat StatlogLine) error {
	key := []byte(configName)
	blob, err := json.Marshal(stat)
	if err != nil {
		return err
	}
	err = best.db.Update(func(tx *bbolt.Tx) error {
		bu := tx.Bucket(bestkey)
		bu.Put(key, blob)
		return nil
	})
	return err
}

func (best *BestDBBolt) Get(configName string) (stat StatlogLine, err error) {
	key := []byte(configName)
	var data []byte
	err = best.db.View(func(tx *bbolt.Tx) error {
		bu := tx.Bucket(bestkey)
		data = bu.Get(key)
		return nil
	})
	if err != nil {
		return
	}
	err = json.Unmarshal(data, &stat)
	return
}

func (best *BestDBBolt) List() (bests map[string]StatlogLine, err error) {
	bests = make(map[string]StatlogLine, 100)
	err = best.db.View(func(tx *bbolt.Tx) error {
		bu := tx.Bucket(bestkey)
		cur := bu.Cursor()
		for k, v := cur.First(); k != nil; k, v = cur.Next() {
			var stat StatlogLine
			err = json.Unmarshal(v, &stat)
			if err != nil {
				return err
			}
			bests[string(k)] = stat
		}
		return nil
	})
	if err != nil {
		return
	}
	return
}

// Log records something that happened, successful or not, good or not, for local stats and process
func (best *BestDBBolt) Log(workdir, configName string, ok bool, stat StatlogLine) error {
	rec := WorkLog{configName, stat, ok, JavaTime()}
	key := []byte(workdir)
	blob, err := json.Marshal(rec)
	if err != nil {
		return err
	}
	err = best.db.Update(func(tx *bbolt.Tx) error {
		bu := tx.Bucket(logkey)
		bu.Put(key, blob)
		return nil
	})
	return err
}

func (best *BestDBBolt) Close() {
	// TODO: flush to disk?
}

*/
type WorkLog struct {
	ConfigName string      `json:"n"`
	BestKmpp   StatlogLine `json:"b"`
	Ok         bool        `json:"ok"`
	Timestamp  int64       `json:"t"`
}

func JavaTime() int64 {
	now := time.Now()
	return (now.Unix() * 1000) + int64(now.Nanosecond()/1000000)
}

func toJTime(now time.Time) int64 {
	return (now.Unix() * 1000) + int64(now.Nanosecond()/1000000)
}

func TimeFromJava(jtms int64) time.Time {
	return time.Unix(jtms/1000, (jtms%1000)*1000000)
}
