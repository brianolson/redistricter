TYPE_LETTERS := 1 2 4 5 6 7 8 A B C E H I M P R S T U Z
#TYPE_SOURCES := record1_auto.cpp
TYPE_SOURCES := $(foreach let,$(TYPE_LETTERS),record$(let).cpp)
TYPE_OBJS := $(foreach let,$(TYPE_LETTERS),record$(let).o)
TYPE_OBJS += mmaped.o

CXXFLAGS += -g -Wall -MMD

all:	mergeRT1 makepolys

makeLinks:	makeLinks.cpp record1.o
	g++ -g -o makeLinks makeLinks.cpp -Wall record1.o mmaped.o

mergeRT1:	mergeRT1.cpp records.a
	g++ -g -o mergeRT1 mergeRT1.cpp -Wall records.a

records.a:	$(TYPE_OBJS)
	ar -r -c records.a $(TYPE_OBJS)
	ranlib records.a

POLYOBJS := makepolys.o recordA.o recordI.o record1.o record2.o mmaped.o

makepolys:	${POLYOBJS}
	${CXX} ${CXXFLAGS} ${LDFLAGS} ${POLYOBJS} -o makepolys

mprun:	makepolys
	./makepolys TGR06003 > mprun 2> mperr
	wc mprun

run:	mergeRT1
	./mergeRT1 -o rt1.RT1 CA/raw/TGR06047.RT1 CA/raw/TGR06099.RT1 > run.spew 2>&1
#	./mergeRT1 -o rt1.RT1 onehundred.RT1
#	./mergeRT1 -o rt1.RT1 CA/raw/TGR0600*RT1

include *.make

clean:
	rm -r -f $(TYPE_OBJS) $(TYPE_SOURCES) records.a mergeRT1

%.cpp %.h:	%.txt createRecordClass.pl
#	echo createRecordClass.pl -t $*
	./createRecordClass.pl -t $*

.SECONDARY:	$(TYPE_SOURCES)

include *.d
