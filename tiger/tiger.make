TYPE_LETTERS := 1 2 4 5 6 7 8 A B C E H I M P R S T U Z
#TYPE_SOURCES := record1_auto.cpp
TYPE_SOURCES := $(foreach let,$(TYPE_LETTERS),tiger/record$(let).cpp)
TYPE_HEADERS := $(foreach let,$(TYPE_LETTERS),tiger/record$(let).h)
TYPE_OBJS := $(foreach let,$(TYPE_LETTERS),tiger/record$(let).o)
TYPE_OBJS += tiger/mmaped.o

#CXXFLAGS += -g -Wall -MMD

all:	tiger/mergeRT1 tiger/makepolys tiger/makeLinks

THINGSTOCLEAN+=tiger/mergeRT1 tiger/makepolys tiger/makeLinks ${TYPE_OBJS} ${TYPE_SOURCES} ${TYPE_HEADERS}

tiger/makeLinks:	tiger/makeLinks.cpp tiger/record1.o tiger/mmaped.o
	g++ ${CXXFLAGS} -g -o tiger/makeLinks tiger/makeLinks.cpp -Wall tiger/record1.o tiger/mmaped.o

tiger/mergeRT1:	tiger/mergeRT1.cpp tiger/record1.o tiger/mmaped.o
	g++ ${CXXFLAGS} -g -o tiger/mergeRT1 tiger/mergeRT1.cpp -Wall tiger/record1.o tiger/mmaped.o

#records.a:	$(TYPE_OBJS)
#	ar -r -c records.a $(TYPE_OBJS)
#	ranlib records.a

POLYOBJS:=tiger/makepolys.o tiger/recordA.o tiger/recordI.o
POLYOBJS+=tiger/record1.o tiger/record2.o tiger/mmaped.o
POLYOBJS+=tiger/rasterizeTiger.o
THINGSTOCLEAN+=${POLYOBJS}

tiger/makepolys.o:	tiger/recordI.h tiger/record2.h

tiger/makepolys:	${POLYOBJS}
	${CXX} ${CXXFLAGS} ${LDFLAGS} ${POLYOBJS} -o tiger/makepolys

mprun mpout mperr:	tiger/makepolys
	tiger/makepolys -o mpout --pngW 828 --pngH 960 --minlon -124.413732 --maxlon -114.13534 --minlat 32.539886 --maxlat 42.007832 data/CA/raw/TGR06083 --maskout mpmask.png > mprun 2> mperr
	wc mprun

%.cpp %.h:	%.txt tiger/createRecordClass.pl
#	echo createRecordClass.pl -t $*
	tiger/createRecordClass.pl -t $*

.SECONDARY:	$(TYPE_SOURCES)

#include tiger/*.d
