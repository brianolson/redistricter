ROOTDIR=${PWD}
SRCDIR=${ROOTDIR}/cpp
UNAME:=$(shell uname)
-include cpp/makeopts/${UNAME}.pre
-include cpp/localvars.make

all:	districter2 linkfixup drend analyze dumpBinLog jall

include cpp/cpp.make

jall:	java/org/bolson/redistricter/Redata.java tools.jar

java/org/bolson/redistricter/Redata.java:	redata.proto
	protoc $< --java_out=java

jcompile:	java/org/bolson/redistricter/Redata.java
	mvn package

tools.jar:	java/org/bolson/redistricter/Redata.java
	mvn package
	ln -s `ls -t target/redistricter*.jar | head -1` tools.jar

python/bdistricting/redata_pb2.py:
	protoc redata.proto --python_out=python/bdistricting

GCSRC=runner
include runner/gotools.make

deploy.tar:	runner/runner districter2
	rm -f deploy.tar
	tar -c -f deploy.tar districter2
	tar -r -f deploy.tar -C runner runner

deploy.tar.gz:	deploy.tar
	gzip < deploy.tar > deploy.tar.gz
