package main

import (
	"compress/gzip"
	"fmt"
	"io"
)

type GzipPipe struct {
	source io.Reader
	fout   io.Writer
	gzdest *gzip.Writer
}

// implement io.Reader
func (gp *GzipPipe) Read(p []byte) (n int, err error) {
	n, err = gp.source.Read(p)
	if err == nil && n > 0 {
		_, werr := gp.gzdest.Write(p[:n])
		if werr != nil {
			err = fmt.Errorf("gzip copy dest, %v", werr)
		}
	}
	return
}

func (gp *GzipPipe) Close() (err error) {
	if rc, ok := gp.source.(io.Closer); ok {
		rc.Close() // throw away source Close() err; don't care
	}
	err = gp.gzdest.Close()
	if gp.fout != nil {
		if wc, ok := gp.fout.(io.Closer); ok {
			return wc.Close()
		}
	}
	return err
}

// Finish consumes the rest of the source and copies it to gzdest
// Calls Close() on source and dest if appropriate.
func (gp *GzipPipe) Finish() (err error) {
	defer func() {
		ce := gp.Close()
		if err == nil {
			err = ce
		}
	}()
	buf := make([]byte, 64*1024)
	for {
		var n int
		n, err = gp.source.Read(buf)
		if err != nil && err != io.EOF {
			return err
		}
		if n > 0 {
			_, werr := gp.gzdest.Write(buf[:n])
			if werr != nil {
				return fmt.Errorf("gzip copy dest, %v", werr)
			}
		}
		if err == io.EOF {
			err = nil
			return
		}
	}
}
