DATESTAMP=$(shell date +%Y%m%d_%H%M%S)

runner:	runner.go data/redata.pb.go
	CGO_ENABLED=0 go build -ldflags="-X github.com/brianolson/redistricter/runner/version.Version=${DATESTAMP}"

data/redata.pb.go:	../redata.proto
	protoc ../redata.proto --go_out=data --proto_path=../
