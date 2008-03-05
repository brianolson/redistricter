#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "mmaped.h"
#include "recordA.h"
#include "recordI.h"
#include "record1.h"
#include "record2.h"

#include "rasterizeTiger.h"

class bstrlist {
public:
	char* str;
	bstrlist* next;
	bstrlist( char* si, bstrlist* ni ) : str( si ), next( ni ) {}
};

bstrlist* froots = NULL;

int checkRoot( char* arg ) {
	struct stat sb;
	int err;
	int arglen = strlen( arg );
	char* root;
	char* longer;
	int toret = 1;
	
	root = strdup( arg );
	assert(root != NULL);
	if ( root[arglen-4] == '.' && root[arglen-3] == 'R' && root[arglen-2] == 'T' ) {
		root[arglen-4] = '\0';
		arglen -= 4;
	}
	longer = (char*)malloc( arglen+5 );
	assert(longer != NULL);
	strcpy( longer, root );
	longer[arglen] = '.';
	longer[arglen+1] = 'R';
	longer[arglen+2] = 'T';
	longer[arglen+3] = '1';
	longer[arglen+4] = '\0';
	err = stat( longer, &sb );
	if ( err != 0 ) {
		perror( longer );
		goto die;
	}
	longer[arglen+3] = '2';
	err = stat( longer, &sb );
	if ( err != 0 ) {
		perror( longer );
		goto die;
	}
	longer[arglen+3] = 'A';
	err = stat( longer, &sb );
	if ( err != 0 ) {
		perror( longer );
		goto die;
	}
	longer[arglen+3] = 'I';
	err = stat( longer, &sb );
	if ( err != 0 ) {
		perror( longer );
		goto die;
	}
	
	froots = new bstrlist( root, froots );
	goto done;

die:
	toret = 0;
	free( root );

done:
	free( longer );
	return toret;
}

int main( int argc, char** argv ) {
	int i;
	//char* ifrootname = NULL;
	PolyGroup pg;
	char* oname = NULL;
	char* maskOutName = NULL;
	bool printPointsPreRasterize = false;
	bool pointNOP = false;

	for ( i = 1; i < argc; i++ ) {
		if ( ! strcmp( argv[i], "--minlat" ) ) {
			i++;
			pg.minlat = strtod( argv[i], NULL ); pg.minlatset = 1;
		} else if ( ! strcmp( argv[i], "--minlatd" ) ) {
			i++;
			pg.minlat = strtod( argv[i], NULL ) + pg.maxlat; pg.minlatset = 1;
		} else if ( ! strcmp( argv[i], "--minlon" ) ) {
			i++;
			pg.minlon = strtod( argv[i], NULL ); pg.minlonset = 1;
		} else if ( ! strcmp( argv[i], "--minlond" ) ) {
			i++;
			pg.minlon = strtod( argv[i], NULL ) + pg.maxlon; pg.minlonset = 1;
		} else if ( ! strcmp( argv[i], "--maxlon" ) ) {
			i++;
			pg.maxlon = strtod( argv[i], NULL ); pg.maxlonset = 1;
		} else if ( ! strcmp( argv[i], "--maxlond" ) ) {
			i++;
			pg.maxlon = strtod( argv[i], NULL ) + pg.minlon; pg.maxlonset = 1;
		} else if ( ! strcmp( argv[i], "--maxlat" ) ) {
			i++;
			pg.maxlat = strtod( argv[i], NULL ); pg.maxlatset = 1;
		} else if ( ! strcmp( argv[i], "--maxlatd" ) ) {
			i++;
			pg.maxlat = strtod( argv[i], NULL ) + pg.minlat; pg.maxlatset = 1;
		} else if ( ! strcmp( argv[i], "--pngW" ) ) {
			i++;
			pg.xpx = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "--pngH" ) ) {
			i++;
			pg.ypx = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "-o" ) ) {
			i++;
			oname = argv[i];
		} else if ( ! strcmp( argv[i], "--maskout" ) ) {
			i++;
			maskOutName = argv[i];
		} else if ( ! strcmp( argv[i], "--prerast" ) ) {
			printPointsPreRasterize = true;
		} else if ( ! strcmp( argv[i], "--pointnop" ) ) {
			pointNOP = true;
		} else if ( checkRoot( argv[i] ) ) {
			//ifrootname = argv[i];
		} else {
			fprintf(stderr,"bogus arg \"%s\"\n", argv[i] );
			exit(1);
		}
	}
	
	if ( froots == NULL ) {
		fprintf(stderr,"need root name\n");
		exit(1);
	}
	//upix = (uint64_t*)malloc( sizeof(uint64_t[xpx*ypx]) );
	FILE* fout = NULL;
	if ( oname != NULL ) {
		fout = fopen( oname, "wb" );
		if ( fout == NULL ) {
			perror( oname );
			exit(1);
		}
	} else {
		fout = stdout;
	}
	if ( ! printPointsPreRasterize ) {
		uint32_t vers = 1;
		fwrite(&vers,sizeof(uint32_t),1,fout);
		fwrite(&pg.xpx,sizeof(int32_t),1,fout);
		fwrite(&pg.ypx,sizeof(int32_t),1,fout);
	}
	if ( (maskOutName != NULL) && (!printPointsPreRasterize) ) {
	    pg.maskpx = (uint8_t*)malloc( sizeof(uint8_t)*pg.xpx*pg.ypx );
	}

	PointOutput* pout;
	if ( printPointsPreRasterize ) {
		pout = new FILEPointOutput<uint32_t>(fout);
	} else {
		pout = new FILEPointOutput<uint16_t>(fout);
	}
	pg.updatePixelSize();
	bstrlist* cf = froots;
	while ( cf != NULL ) {
		pg.setRootName( cf->str );
		fprintf(stderr,"processing %s\n", cf->str );
		
		pg.buildRTI();
		pg.buildShapes();
		//builtRTAPU();
		
		pg.processR1();
		
		pg.updatePixelSize2();

		if ( printPointsPreRasterize ) {
			pg.reconcile( pout, &PolyGroup::printPointList );
		} else if ( pointNOP ) {
			pg.reconcile( pout, &PolyGroup::pointListNOP );
		} else {
			pg.reconcile( pout );
		}
		pg.clear();
		cf = cf->next;
	}

#if 01
	pout->close();
#else
	fflush(fout);
	fclose(fout);
#endif
	
	if ( maskOutName != NULL ) {
		doMaskPNG( maskOutName, pg.maskpx, pg.xpx, pg.ypx );
	}
#if 0
	if ( upix != NULL ) {
		int fdo = open( "upix", O_WRONLY|O_CREAT, 0777 );
		if ( fdo < 0 ) {
			perror("upix");
			exit(1);
		}
		uint32_t vers = 1;
		write(fdo,&vers,sizeof(uint32_t));
		write(fdo,&xpx,sizeof(int32_t));
		write(fdo,&ypx,sizeof(int32_t));
		write(fdo,&vers,sizeof(uint32_t));// pad keep upix 8 byte aligned
		write(fdo,upix,sizeof(uint64_t[ypx*xpx]));
		close( fdo );
	}
#endif
	return 0;
}

