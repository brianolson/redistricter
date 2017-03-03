UNAME:=$(shell uname)

-include makeopts/${UNAME}.pre
-include localvars.make

ROOTDIR=${PWD}

all:	districter2 linkfixup drend rta2dsz analyze dumpBinLog
jall:	java/org/bolson/redistricter/Redata.java tools.jar

THINGSTOCLEAN:=districter2 linkfixup drend gbin rta2dsz analyze tools.jar


# Enable clang
#CC=/Users/bolson/psrc/llvm/llvm/Debug+Asserts/bin/clang -emit-llvm
#CXX=/Users/bolson/psrc/llvm/llvm/Debug+Asserts/bin/clang -emit-llvm
#LDFLAGS+=-L/usr/lib/gcc/i686-apple-darwin10/4.2.1 -lstdc++
#CXXLD=/Users/bolson/psrc/llvm/llvm/Debug+Asserts/bin/llvm-ld

# Not clang
CXXLD?=${CXX} ${CXXFLAGS}

#OG:=-O2 -DNDEBUG=1
OG?=-g
#OG:=-g -pg
# can't have -ansi -pedantic because C++ standard as implemented in GCC I've
# tried (up to 4.0.1) throw a bunch of warnings on draft 64 bit stuff.
#CCOMMONFLAGS:=-Wall -Itiger -MMD -ansi -pedantic
CCOMMONFLAGS+=-Wall -Itiger -DHAVE_PROTOBUF -Iinclude
# -MMD is incompatible with some Apple compile modes
#CCOMMONFLAGS+=-Wall -Itiger -MMD
#CCOMMONFLAGS+=-MMD
CXXFLAGS+=${OG} ${CCOMMONFLAGS} -std=c++11
CFLAGS+=${OG} ${CCOMMONFLAGS}

JAVAC?=javac

#CXXFLAGS+=-DNEAREST_NEIGHBOR_MULTITHREAD=1


#TODO: this is getting rediculous, might be time for autoconf. ew. autoconf.
LDPNG?=-lpng
STATICPNG?=${LDPNG}
LDFLAGS+=${LDPNG} -Llib -L/usr/local/lib -lz -lprotobuf

COREOBJS:=fileio.o Bitmap.o tiger/mmaped.o Solver.o District2.o
COREOBJS+=PreThread.o renderDistricts.o LinearInterpolate.o
COREOBJS+=GrabIntermediateStorage.o AbstractDistrict.o DistrictSet.o
COREOBJS+=NearestNeighborDistrictSet.o protoio.o StatThing.o
COREOBJS+=uf1.o logging.o redata.pb.o BinaryStatLogger.o

CORESRCS:=fileio.cpp Bitmap.cpp tiger/mmaped.cpp Solver.cpp District2.cpp
CORESRCS+=PreThread.cpp renderDistricts.cpp LinearInterpolate.cpp
CORESRCS+=GrabIntermediateStorage.cpp AbstractDistrict.cpp DistrictSet.cpp
CORESRCS+=NearestNeighborDistrictSet.cpp protoio.cpp StatThing.cpp
CORESRCS+=uf1.cpp logging.cpp redata.pb.cc BinaryStatLogger.cpp

D2OBJS:=${COREOBJS} nonguimain.o
D2SOURCES:=District2.cpp fileio.cpp nonguimain.cpp renderDistricts.cpp Solver.cpp tiger/mmaped.cpp PreThread.cpp LinearInterpolate.cpp GrabIntermediateStorage.cpp

THINGSTOCLEAN+=${D2OBJS}
districter2:	$(D2OBJS)
	$(CXXLD) $(D2OBJS) $(LDFLAGS) -o districter2

# TODO: switch to libprotobuf-lite.a
# TODO: maybe use local static libpng.a
districter2_staticproto:	${D2OBJS}
	${CXXLD} ${D2OBJS} -lpthread ${STATICPNG} -lz /usr/local/lib/libprotobuf.a -o districter2_staticproto
	strip districter2_staticproto

# compile all the sources together in case there's any cross-sourcefile optimization to be done
#districter2:	 District2.cpp fileio.cpp nonguimain.cpp renderDistricts.cpp Solver.cpp tiger/mmaped.cpp
#	${CXX} -o districter2 ${CXXFLAGS} District2.cpp fileio.cpp nonguimain.cpp renderDistricts.cpp Solver.cpp tiger/mmaped.cpp ${LDFLAGS}

d2prof:	${CORESRCS} nonguimain.cpp
	${CXX} -m64 -o districter2 -g -pg ${CCOMMONFLAGS} ${CORESRCS} nonguimain.cpp ${LDFLAGS}

d2debug:	 ${D2SOURCES}
	${CXX} -o districter2 -g -Wall -Itiger -MMD ${D2SOURCES} ${LDPNG} -lz

d2nopng:	 ${D2SOURCES}
	${CXX} -o d2nopng -DNOPNG -O2 -Wall -Itiger -MMD ${D2SOURCES} -lz

d2nopngtri:	${D2SOURCES}
	${CXX} -o d2nopng -DNOPNG -O2 -Wall -Itiger -MMD ${D2SOURCES} -lz

#d2_32:	${D2SOURCES}
#	${CXX} -o $@ -O2 ${CCOMMONFLAGS} ${D2SOURCES} -lpng -lz -DINTRA_GRAB_MULTITHREAD=0

#d2_64:	${D2SOURCES}
#	${CXX} -o $@ -O2 ${CCOMMONFLAGS} -m64 ${D2SOURCES} -lpng -lz -DINTRA_GRAB_MULTITHREAD=0

#d2_64_smp:	${D2SOURCES}
#	${CXX} -o $@ -O2 ${CCOMMONFLAGS} -m64 ${D2SOURCES} -lpng -lz -DINTRA_GRAB_MULTITHREAD=1

LFUOBJS:=${COREOBJS} linkfixup.o
THINGSTOCLEAN+=${LFUOBJS}

linkfixup:	$(LFUOBJS)
	$(CXX) ${CXXFLAGS} $(LFUOBJS) $(LDFLAGS) -lproj -o linkfixup

DRENDOBJS:=${COREOBJS} drendmain.o MapDrawer.o

drend:	${DRENDOBJS}
	$(CXX) ${CXXFLAGS} $(DRENDOBJS) $(LDFLAGS) -o drend

drend_static:	${DRENDOBJS}
	$(CXX) ${CXXFLAGS} $(DRENDOBJS) $(STATICPNG) -lpthread -lz /usr/local/lib/libprotobuf.a -o drend_static
	strip drend_static

ANALYZEOBJS:=${COREOBJS} analyze.o placefile.o popSSD.o

analyze:	${ANALYZEOBJS}
	$(CXX) ${CXXFLAGS} $(ANALYZEOBJS) $(LDFLAGS) -o analyze

RTADSZOBJS:=${COREOBJS} rtaToDsz.o tiger/recordA.o

THINGSTOCLEAN+=${DRENDOBJS} ${GBINOBJS} ${RTADSZOBJS} ${ANALYZEOBJS} *.d */*.d

rta2dsz:	${RTADSZOBJS}
	$(CXX) ${CXXFLAGS} $(RTADSZOBJS) $(LDFLAGS) -o rta2dsz

rtaToDsz.o:	tiger/recordA.h

redata_pb2.py:	redata.proto
	protoc redata.proto --python_out=.

#UFONEDATAOBJS:=uf1.o uf1data.o tiger/mmaped.o
#THINGSTOCLEAN+=${UFONEDATAOBJS}
#uf1data:	${UFONEDATAOBJS}
#	$(CXX) ${CXXFLAGS} $(UFONEDATAOBJS) $(LDFLAGS) -o uf1data

run:	districter2
	./districter2 -U build/input.uf1 -g 100 --pngout fl_test.png --pngW 400 --pngH 400 --oldCDs -d -10 -o fl_test_oz

pw:	pw.cpp
	$(CXX) ${CXXFLAGS} -Wall ${LDPNG} pw.cpp -lz -o pw -g

dumpBinLog:	dumpBinLog.cpp redata.pb.cc redata.pb.h
	$(CXX) ${CXXFLAGS} dumpBinLog.cpp redata.pb.cc -lz -lprotobuf -lpthread $(LDFLAGS) -o dumpBinLog

xcode:
	xcodebuild -alltargets -project guidistricter.xcodeproj

# make clean && make -j 2 OG='-O2 -DNDEBUG' clientdist
clientdist:	.FORCE districter2_staticproto drend_static
	./makedist.py

proj/configure:
	git clone https://github.com/OSGeo/proj.4.git proj
	cd proj && /bin/sh autogen.sh

proj/Makefile:	proj/configure
	cd proj && ./configure --prefix=${ROOTDIR}

lib/libproj.a include/proj_api.h:	proj/Makefile
	cd proj && make && make install


.FORCE:

include tiger/tiger.make

-include *.d */*.d

-include data/*/.make

-include localtail.make

%.pb.cc %.pb.h : %.proto
	protoc $< --cpp_out=$(@D)

java/org/bolson/redistricter/Redata.java:	redata.proto
	protoc $< --java_out=java

jcompile:
	mvn package

tools.jar:	jcompile
	ln -s `ls -t target/redistricter*.jar | head -1` tools.jar

THINGSTOCLEAN+=java/org/bolson/redistricter/Redata.java tools.jar


clean:
	rm -f ${THINGSTOCLEAN} *.pb.cc *.pb.h java/org/bolson/redistricter/Redata.java

protoio.o:	redata.pb.h
PBPointOutput.o:	redata.pb.h
linkfixup.o:	include/proj_api.h
