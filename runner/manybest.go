package main

import (
	"fmt"

	"encoding/json"

	"go.etcd.io/bbolt"
	bolt "go.etcd.io/bbolt"
)

type BestDB struct {
	path string
	db   *bolt.DB
}

func OpenBestDB(path string) (bdb *BestDB, err error) {
	db, err := bolt.Open(path, 0600, nil)
	if err != nil {
		return nil, err
	}
	err = db.Update(func(tx *bbolt.Tx) error {
		_, err := tx.CreateBucketIfNotExists(bestkey)
		return err
	})
	if err != nil {
		return nil, fmt.Errorf("could not init http cache db, %v", err)
	}
	return &BestDB{path, db}, nil
}

var bestkey = []byte("b")

func (best *BestDB) Put(configName string, stat StatlogLine) error {
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

func (best *BestDB) Get(configName string) (stat StatlogLine, err error) {
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

func (best *BestDB) Close() {
	// TODO: flush to disk?
}
