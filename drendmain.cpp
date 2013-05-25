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

#include "arghandler.h"
#include "Solver.h"
#include "tiger/mmaped.h"
#include "MapDrawer.h"
#include "renderDistricts.h"


void runBasic( Solver& sov, MapDrawer& mr, const char* colorFileIn, const char* colorFileOut );
void runDrendCommandFile( Solver& sov );

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

int main( int argc, const char** argv ) {
	Solver sov;
	int nargc;
	const char* upxfname = NULL;
	const char* mppbfname = NULL;
	const char* popdensityname = NULL;

	const char* commandFileName = NULL;

	const char* colorFileIn = NULL;
	const char* colorFileOut = NULL;

	MapDrawer mr;

	nargc=1;
	sov.districtSetFactory = District2SetFactory;

	double minlatd = NAN;
	double minlond = NAN;
	double maxlatd = NAN;
	double maxlond = NAN;

	int argi = 1;
	while (argi < argc) {
#if 1
	    DoubleArg("minlat", &mr.minlat);
	    DoubleArg("minlatd", &minlatd);
	    DoubleArg("minlon", &mr.minlon);
	    DoubleArg("minlond", &minlond);
	    DoubleArg("maxlat", &mr.maxlat);
	    DoubleArg("maxlatd", &maxlatd);
	    DoubleArg("maxlon", &mr.maxlon);
	    DoubleArg("maxlond", &maxlond);
	    StringArg("f", &commandFileName);
	    StringArg("px", &upxfname);
	    StringArg("mppb", &mppbfname);
	    StringArg("colorsIn", &colorFileIn);
	    StringArg("colorsOut", &colorFileOut);
	    StringArg("density", &popdensityname);

	    // default:
	    argv[nargc] = argv[argi];
	    nargc++;
	    argi++;
#else
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
		} else if ( ! strcmp( argv[i], "--mppb" ) ) {
			i++;
			mppbfname = argv[i];
		} else if ( ! strcmp( argv[i], "--colorsIn" ) ) {
			i++;
			colorFileIn = argv[i];
		} else if ( ! strcmp( argv[i], "--colorsOut" ) ) {
			i++;
			colorFileOut = argv[i];
		} else if ( ! strcmp( argv[i], "--density" ) ) {
			i++;
			popdensityname = argv[i];
		} else {
			argv[nargc] = argv[i];
			nargc++;
		}
#endif
	}

	if (!isnan(minlatd)) {
	    mr.minlat = mr.maxlat + minlatd;
	}
	if (!isnan(maxlatd)) {
	    mr.maxlat = mr.minlat + maxlatd;
	}
	if (!isnan(minlond)) {
	    mr.minlon = mr.maxlon + minlond;
	}
	if (!isnan(maxlond)) {
	    mr.maxlon = mr.minlon + maxlond;
	}


	argv[nargc]=NULL;
	int argcout = sov.handleArgs( nargc, argv );
	if (argcout != 1) {
		fprintf( stderr, "%s: bogus arg \"%s\"\n", argv[0], argv[1] );
		fputs( usage, stderr );
		fputs( Solver::argHelp, stderr );
		exit(1);
		return 1;
	}
	
	if ( (!sov.hasSolutionToLoad()) && commandFileName == NULL ) {
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
	if ( sov.hasSolutionToLoad() ) {
		fprintf( stdout, "loading \"%s\"\n", sov.getSolutionFilename() );
		if ( sov.loadSolution() < 0 ) {
			return 1;
		}
		char* statstr = new char[10000];
		sov.getDistrictStats(statstr, 10000);
		fputs( statstr, stdout );
		delete [] statstr;
	}
	//sov.init();
	
	bool ok = true;
	if ( upxfname != NULL ) {
		mr.readUPix( &sov, upxfname );
	} else if ( mppbfname != NULL ) {
		ok = mr.readMapRasterization( &sov, mppbfname );
	} else {
		mr.setSize( sov.pngWidth, sov.pngHeight );
	}
	if (!ok) {
		return 1;
	}
	
	// init PNG image data area
	mr.initDataAndRows();

	if ( commandFileName != NULL ) {
		mr.runDrendCommandFile( sov, commandFileName );
	} else {
	    runBasic( sov, mr, colorFileIn, colorFileOut );
	}
	if (popdensityname != NULL) {
		mr.clearToBackgroundColor();
		mr.paintPopulation(&sov);
		myDoPNG( popdensityname, mr.rows, mr.height, mr.width );
	}

	return 0;
}

void runBasic( Solver& sov, MapDrawer& mr, const char* colorFileIn, const char* colorFileOut ) {
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
