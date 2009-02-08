#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <vector>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "Solver.h"
#include "tiger/mmaped.h"
#include "tiger/recordA.h"

using std::vector;

int processRTA(Solver* sov, const char* fname) {
	mmaped rtaf;
	int i;
	i = rtaf.open( fname );
	if ( i < 0 ) {
		return -1;
	}
	recordA ra( (char*)rtaf.data );
	size_t numRTAs = rtaf.sb.st_size / ra.size;
	for ( i = 0; i < (int)numRTAs; i++ ) {
		uint64_t ubid;
		ubid = ra.COUNTYCU_longValue(i);
		ubid *= 1000000;
		ubid += ra.TRACT_longValue(i);
		ubid *= 10000;
		ubid += ra.BLOCK_longValue(i);
		uint32_t lx;
		lx = sov->gd->indexOfUbid( ubid );
		if ( lx == (uint32_t)-1 ) {
			fprintf(
				stderr,
				"ubid %llu (county %ld, tract %ld, block %ld) => %u\n",
				ubid, 
				ra.COUNTYCU_longValue(i), ra.TRACT_longValue(i),
				ra.BLOCK_longValue(i),
				lx);
		} else {
			long cdcu;
			cdcu = ra.CDCU_longValue(i) - 1;
			if ( (cdcu < 0) || (cdcu > POPTYPE_MAX) ){
				fprintf(stderr,"cdcu %ld out of bounds of POPTYPE\n", cdcu );
			} else {
				sov->winner[lx] = cdcu;
			}
		}
	}
	return 0;
}

int main( int argc, char** argv ) {
	Solver sov;
	int i, nargc;
	vector<char*> filenames;
	
	nargc=1;
	
	for ( i = 1; i < argc; i++ ) {
		if ( (! strcmp( argv[i], "-i" )) ) {
			i++;
			filenames.push_back(argv[i]);
		} else if ( strstr(argv[i],".RTA") ) {
			filenames.push_back(argv[i]);
		} else {
			argv[nargc] = argv[i];
			nargc++;
		}
	}
	argv[nargc]=NULL;
	
	if ( filenames.size() == 0 ) {
		fprintf(stderr,"useless rtaToDsz, no inputs\n");
		exit(1);
	}
	sov.handleArgs( nargc, argv );
	if ( sov.dumpname == NULL ) {
		fprintf(stderr,"useless rtaToDsz, null dumpname\n");
		exit(1);
	}
	sov.load();
	sov.initNodes();
	sov.allocSolution();
	for ( i = 0; i < sov.gd->numPoints; i++ ) {
		sov.winner[i] = NODISTRICT;
	}
	for (vector<char*>::iterator fni = filenames.begin();
			fni != filenames.end(); ++fni) {
		if (processRTA(&sov, *fni) < 0) {
			return 1;
		}
	}
	for ( i = 0; i < sov.gd->numPoints; i++ ) {
		if ( sov.winner[i] == NODISTRICT ) {
			fprintf(stderr,"index %d has no district, pop=%d, ubid=%llu\n",
				i, sov.gd->pop[i], sov.gd->ubidOfIndex(i) );
		}
	}
	sov.saveZSolution( sov.dumpname );
	
	return 0;
}
