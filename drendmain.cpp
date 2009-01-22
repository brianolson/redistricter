#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>

#include "Solver.h"
#include "tiger/mmaped.h"
#include "MapDrawer.h"

char* commandFileName = NULL;

char* colorFileIn = NULL;
char* colorFileOut = NULL;

void runBasic( Solver& sov );
void runDrendCommandFile( Solver& sov );

MapDrawer mr;

const char usage[] = 
"usage: drend [--minlat 000.000000 | --minlatd delta from maxlat]\n"
"  [--minlon 000.000000 | --minlond delta from maxlon]\n"
"  [--maxlat 000.000000 | --maxlatd delta from minlat]\n"
"  [--maxlon 000.000000 | --maxlond delta from minlon]\n"
"  [-f command file][-px pixel_map.mpout]\n"
"  [--colorsIn color file][--colorsOut color file]\n"
"  [-U file.uf1 | -B file.gbin][-d num districts]\n"
"  [-r|--loadSolution .gsz]\n"
"  [--pngW pixels wide][--pngH pixels high][--pngout out.png]\n"
;

int main( int argc, char** argv ) {
	Solver sov;
	int i, nargc;
	char* upxfname = NULL;
	
	nargc=1;
	sov.districtSetFactory = District2SetFactory;

	for ( i = 1; i < argc; i++ ) {
		if ( ! strcmp( argv[i], "--minlat" ) ) {
			i++;
			mr.minlat = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--minlatd" ) ) {
			i++;
			mr.minlat = strtod( argv[i], NULL ) + mr.maxlat;
		} else if ( ! strcmp( argv[i], "--minlon" ) ) {
			i++;
			mr.minlon = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--minlond" ) ) {
			i++;
			mr.minlon = strtod( argv[i], NULL ) + mr.maxlon;
		} else if ( ! strcmp( argv[i], "--maxlon" ) ) {
			i++;
			mr.maxlon = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--maxlond" ) ) {
			i++;
			mr.maxlon = strtod( argv[i], NULL ) + mr.minlon;
		} else if ( ! strcmp( argv[i], "--maxlat" ) ) {
			i++;
			mr.maxlat = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--maxlatd" ) ) {
			i++;
			mr.maxlat = strtod( argv[i], NULL ) + mr.minlat;
		} else if ( ! strcmp( argv[i], "-f" ) ) {
			i++;
			commandFileName = argv[i];
		} else if ( ! strcmp( argv[i], "-px" ) ) {
			i++;
			upxfname = argv[i];
		} else if ( ! strcmp( argv[i], "--colorsIn" ) ) {
			i++;
			colorFileIn = argv[i];
		} else if ( ! strcmp( argv[i], "--colorsOut" ) ) {
			i++;
			colorFileOut = argv[i];
		} else {
			argv[nargc] = argv[i];
			nargc++;
		}
	}
	argv[nargc]=NULL;
	sov.handleArgs( nargc, argv );
	
	if ( sov.loadname == NULL && commandFileName == NULL ) {
		fprintf(stderr,"useless drend, null loadname and null command file name\n");
		exit(1);
	}
	if ( sov.pngname == NULL && commandFileName == NULL ) {
		fprintf(stderr,"useless drend, null pngname and null command file name\n");
		exit(1);
	}
	sov.load();

	sov.initNodes();
	sov.allocSolution();
	if ( sov.loadname != NULL ) {
		fprintf( stdout, "loading \"%s\"\n", sov.loadname );
		if ( sov.loadZSolution( sov.loadname ) < 0 ) {
			return 1;
		}
		char* statstr = new char[10000];
		sov.getDistrictStats(statstr, 10000);
		fputs( statstr, stdout );
		delete statstr;
	}
	//sov.init();
	
	if ( upxfname != NULL ) {
		mr.readUPix( &sov, upxfname );
	} else {
		mr.setSize( sov.pngWidth, sov.pngHeight );
	}
	
	// init PNG image data area
	mr.initDataAndRows();

	if ( commandFileName != NULL ) {
		mr.runDrendCommandFile( sov, commandFileName );
	} else {
		runBasic( sov );
	}

	return 0;
}

void runBasic( Solver& sov ) {
	if ( colorFileIn != NULL ) {
		FILE* cfi = fopen( colorFileIn, "r" );
		if ( cfi == NULL ) {
			perror(colorFileIn);
			exit(1);
		}
		readColoring( cfi );
		fclose( cfi );
	} else {
		Adjacency ta;
		sov.calculateAdjacency(&ta);
		recolorDists( ta.adjacency, ta.adjlen, sov.districts );
	}
	mr.doPNG_r( &sov, sov.pngname );
	
	if ( colorFileOut != NULL ) {
		FILE* cfo = fopen( colorFileOut, "w" );
		if ( cfo == NULL ) {
			perror(colorFileIn);
			exit(1);
		}
		printColoring( cfo );
		fclose( cfo );
	}
}
