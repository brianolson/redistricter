UNAME:=$(shell uname)

-include makeopts/${UNAME}.pre
-include localvars.make

all:	districter2 linkfixup drend rta2dsz analyze

THINGSTOCLEAN:=districter2 linkfixup drend gbin rta2dsz analyze


#OG:=-O2 -DNDEBUG=1
OG?=-g
#OG:=-g -pg
# can't have -ansi -pedantic because C++ standard as implemented in GCC I've
# tried (up to 4.0.1) throw a bunch of warnings on draft 64 bit stuff.
#CCOMMONFLAGS:=-Wall -Itiger -MMD -ansi -pedantic
CCOMMONFLAGS+=-Wall -Itiger -DHAVE_PROTOBUF
# -MMD is incompatible with some Apple compile modes
#CCOMMONFLAGS+=-Wall -Itiger -MMD
#CCOMMONFLAGS+=-MMD
CXXFLAGS+=${OG} ${CCOMMONFLAGS}
CFLAGS+=${OG} ${CCOMMONFLAGS}

#CXXFLAGS+=-DNEAREST_NEIGHBOR_MULTITHREAD=1


LDPNG=-lpng12
LDFLAGS+=${LDPNG} -lz -lprotobuf

COREOBJS:=fileio.o Bitmap.o tiger/mmaped.o Solver.o District2.o
COREOBJS+=PreThread.o renderDistricts.o LinearInterpolate.o
COREOBJS+=GrabIntermediateStorage.o AbstractDistrict.o DistrictSet.o
COREOBJS+=NearestNeighborDistrictSet.o protoio.o StatThing.o
COREOBJS+=uf1.o logging.o redata.pb.o

CORESRCS:=fileio.cpp Bitmap.cpp tiger/mmaped.cpp Solver.cpp District2.cpp
CORESRCS+=PreThread.cpp renderDistricts.cpp LinearInterpolate.cpp
CORESRCS+=GrabIntermediateStorage.cpp AbstractDistrict.cpp DistrictSet.cpp
CORESRCS+=NearestNeighborDistrictSet.cpp protoio.cpp StatThing.cpp
CORESRCS+=uf1.cpp

D2OBJS:=${COREOBJS} nonguimain.o
D2SOURCES:=District2.cpp fileio.cpp nonguimain.cpp renderDistricts.cpp Solver.cpp tiger/mmaped.cpp PreThread.cpp LinearInterpolate.cpp GrabIntermediateStorage.cpp

THINGSTOCLEAN+=${D2OBJS}
districter2:	$(D2OBJS)
	$(CXX) $(D2OBJS) $(LDFLAGS) ${CXXFLAGS} -o districter2
# compile all the sources together in case there's any cross-sourcefile optimization to be done
#districter2:	 District2.cpp fileio.cpp nonguimain.cpp renderDistricts.cpp Solver.cpp tiger/mmaped.cpp
#	${CXX} -o districter2 ${CXXFLAGS} District2.cpp fileio.cpp nonguimain.cpp renderDistricts.cpp Solver.cpp tiger/mmaped.cpp  -lpng12 -lz

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
	$(CXX) ${CXXFLAGS} $(LFUOBJS) $(LDFLAGS) -o linkfixup

DRENDOBJS:=${COREOBJS} drendmain.o MapDrawer.o

drend:	${DRENDOBJS}
	$(CXX) ${CXXFLAGS} $(DRENDOBJS) $(LDFLAGS) -o drend

ANALYZEOBJS:=${COREOBJS} analyze.o

analyze:	${ANALYZEOBJS}
	$(CXX) ${CXXFLAGS} $(ANALYZEOBJS) $(LDFLAGS) -o analyze

RTADSZOBJS:=${COREOBJS} rtaToDsz.o tiger/recordA.o

THINGSTOCLEAN+=${DRENDOBJS} ${GBINOBJS} ${RTADSZOBJS} ${ANALYZEOBJS} *.d */*.d

rta2dsz:	${RTADSZOBJS}
	$(CXX) ${CXXFLAGS} $(RTADSZOBJS) $(LDFLAGS) -o rta2dsz

rtaToDsz.o:	tiger/recordA.h

tabledesc/redata_pb2.py:	redata.proto
	protoc redata.proto --python_out=tabledesc

#UFONEDATAOBJS:=uf1.o uf1data.o tiger/mmaped.o
#THINGSTOCLEAN+=${UFONEDATAOBJS}
#uf1data:	${UFONEDATAOBJS}
#	$(CXX) ${CXXFLAGS} $(UFONEDATAOBJS) $(LDFLAGS) -o uf1data

clean:
	rm -f ${THINGSTOCLEAN} *.pb.cc *.pb.h

run:	districter2
	./districter2 -U build/input.uf1 -g 100 --pngout fl_test.png --pngW 400 --pngH 400 --oldCDs -d -10 -o fl_test_oz

pw:	pw.cpp
	g++ -Wall ${LDPNG} pw.cpp -lz -o pw -g

xcode:
	xcodebuild -alltargets -project guidistricter.xcodeproj

.FORCE:

include tiger/tiger.make

-include *.d */*.d

-include data/*/.make

-include localtail.make

%.pb.cc %.pb.h : %.proto
	protoc $< --cpp_out=$(@D)

java/org/bolson/redistricter/Redata.java:	redata.proto
	protoc $< --java_out=java

tools.jar:	java/org/bolson/redistricter/Redata.java java/org/bolson/redistricter/*.java jars/protobuf.jar
	mkdir -p classes
	cd java && find . -name \*.java | xargs javac -cp ../jars/protobuf.jar -g -d ../classes
	cd classes && jar cvf ../tools.jar org

protoio.o:	redata.pb.h
PBPointOutput.o:	redata.pb.h
