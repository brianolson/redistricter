SRCDIR?=${PWD}
ROOTDIR?=${PWD}/..

#OG:=-O2 -DNDEBUG=1
OG?=-g
#OG:=-g -pg
CCOMMONFLAGS+=-Wall -I${SRCDIR}/include -MMD
CXXFLAGS+=${OG} ${CCOMMONFLAGS} -std=c++11
CFLAGS+=${OG} ${CCOMMONFLAGS}

LDPNG?=-lpng
STATICPNG?=${LDPNG}
LDFLAGS+=${LDPNG} -L${SRCDIR}/lib -L/usr/local/lib -lz -lprotobuf

CORESRCS:=${SRCDIR}/fileio.cpp
CORESRCS+=${SRCDIR}/Bitmap.cpp
CORESRCS+=${SRCDIR}/mmaped.cpp
CORESRCS+=${SRCDIR}/Solver.cpp
CORESRCS+=${SRCDIR}/District2.cpp
CORESRCS+=${SRCDIR}/PreThread.cpp
CORESRCS+=${SRCDIR}/renderDistricts.cpp
CORESRCS+=${SRCDIR}/LinearInterpolate.cpp
CORESRCS+=${SRCDIR}/GrabIntermediateStorage.cpp
CORESRCS+=${SRCDIR}/AbstractDistrict.cpp
CORESRCS+=${SRCDIR}/DistrictSet.cpp
CORESRCS+=${SRCDIR}/NearestNeighborDistrictSet.cpp
CORESRCS+=${SRCDIR}/protoio.cpp
CORESRCS+=${SRCDIR}/StatThing.cpp
CORESRCS+=${SRCDIR}/uf1.cpp
CORESRCS+=${SRCDIR}/logging.cpp
CORESRCS+=${SRCDIR}/redata.pb.cc
CORESRCS+=${SRCDIR}/BinaryStatLogger.cpp
CORESRCS+=${SRCDIR}/CountyCityDistricter.cpp

D2SOURCES:=${CORESRCS}
D2SOURCES+=${SRCDIR}/nonguimain.cpp

COREOBJS=$(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(CORESRCS)))
D2OBJS=$(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(D2SOURCES)))

cppall:	districter2 linkfixup drend analyze dumpBinLog

districter2:	${D2OBJS}
	$(CXX) -static ${CXXFLAGS} $(D2OBJS) $(LDFLAGS) -o districter2

LFUSRCS:=${CORESRCS} ${SRCDIR}/linkfixup.cpp
LFUOBJS=$(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(LFUSRCS)))

linkfixup:	$(LFUOBJS)
	$(CXX) -static ${CXXFLAGS} $(LFUOBJS) ${SRCDIR}/lib/libproj.a $(LDFLAGS) -o linkfixup
#	$(CXX) -static ${CXXFLAGS} $(LFUOBJS) $(LDFLAGS) -lproj -o linkfixup

DRENDSRCS:=${CORESRCS} ${SRCDIR}/drendmain.cpp ${SRCDIR}/MapDrawer.cpp
DRENDOBJS=$(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(DRENDSRCS)))

drend:	${DRENDOBJS}
	$(CXX) -static ${CXXFLAGS} $(DRENDOBJS) $(LDFLAGS) -o drend


ANALYZESRCS:=${CORESRCS}
ANALYZESRCS+=${SRCDIR}/analyze.cpp
ANALYZESRCS+=${SRCDIR}/placefile.cpp
ANALYZESRCS+=${SRCDIR}/popSSD.cpp
ANALYZEOBJS=$(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(ANALYZESRCS)))

analyze:	${ANALYZEOBJS}
	$(CXX) -static ${CXXFLAGS} $(ANALYZEOBJS) $(LDFLAGS) -o analyze

DBLSRCS:=${SRCDIR}/dumpBinLog.cpp ${SRCDIR}/redata.pb.cc

dumpBinLog:	${DBLSRCS}
	$(CXX) -static ${CXXFLAGS} ${DBLSRCS} -lz -lprotobuf -lpthread $(LDFLAGS) -o dumpBinLog


${SRCDIR}/%.pb.cc ${SRCDIR}/%.pb.h : ${ROOTDIR}/%.proto
	protoc --proto_path=${ROOTDIR} $< --cpp_out=${SRCDIR}

# Download and build `proj` geographic projection library
${SRCDIR}/proj/configure:
	git clone https://github.com/OSGeo/proj.4.git ${SRCDIR}/proj
	cd ${SRCDIR}/proj && /bin/sh autogen.sh

${SRCDIR}/proj/Makefile:	${SRCDIR}/proj/configure
	cd ${SRCDIR}/proj && ./configure --prefix=${SRCDIR}

${SRCDIR}/lib/libproj.a ${SRCDIR}/include/proj_api.h:	${SRCDIR}/proj/Makefile
	cd ${SRCDIR}/proj && make && make install


clean:
	rm -f ${SRCDIR}/*.o ${SRCDIR}/*.pb.cc ${SRCDIR}/*.pb.h ${SRCDIR}/*.d districter2 linkfixup drend analyze dumpBinLog

${SRCDIR}/protoio.o:	${SRCDIR}/redata.pb.h
${SRCDIR}/linkfixup.o:	${SRCDIR}/include/proj_api.h
