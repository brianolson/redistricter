DATESTAMP=$(shell date +%Y%m%d_%H%M%S)

${GTSRC}/runner:	${GTSRC}/*.go
	cd "${GTSRC}" && CGO_ENABLED=0 go build -ldflags="-X github.com/brianolson/redistricter/runner/version.Version=${DATESTAMP}"

${GTSRC}/receiver/receiver:	${GTSRC}/receiver/*.go
	cd "${GTSRC}/receiver" && CGO_ENABLED=0 go build -ldflags="-X github.com/brianolson/redistricter/runner/version.Version=${DATESTAMP}"
