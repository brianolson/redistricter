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

double minlat = NAN, minlon = NAN, maxlat = NAN, maxlon = NAN;
char* commandFileName = NULL;

char* colorFileIn = NULL;
char* colorFileOut = NULL;

unsigned char* data = NULL;
unsigned char** rows = NULL;

void runBasic( Solver& sov );
void runDrendCommandFile( Solver& sov );

inline void maybeClearDataAndRows() {
	if ( data != NULL ) {
		free( data );
		data = NULL;
	}
	if ( rows != NULL ) {
		free( rows );
		rows = NULL;
	}
}
void initDataAndRows( int pngWidth, int pngHeight ) {
	maybeClearDataAndRows();
	data = (unsigned char*)malloc(pngWidth*pngHeight*3*sizeof(unsigned char) );
	rows = (unsigned char**)malloc(pngHeight*sizeof(unsigned char*) );
	assert(data != NULL);
	assert(rows != NULL);
	
	for ( int y = 0; y < pngHeight; y++ ) {
		rows[y] = data + (y*pngWidth*3);
	}
}

class pxlist {
public:
	int numpx;
	uint16_t* px;
	pxlist() : numpx( 0 ), px( NULL ) {};
};

pxlist* globalpxlist = NULL;

void clearUPix( pxlist* px, int len ) {
	for ( int i = 0; i < len; i++ ) {
		if ( px[i].px != NULL ) {
			free( px[i].px );
			px[i].px = NULL;
		}
		px[i].numpx = 0;
	}
}
void readUPix( Solver* sov, char* upfname, pxlist* px ) {
	mmaped mf;
	uintptr_t data;
	mf.open( upfname );
	int endianness = 0;
	
	data = (uintptr_t)mf.data;
	// int32_t vers, x, y;
	{
		uint32_t vers;
		vers = *((uint32_t*)data);
		if ( swap32( vers ) == 1 ) {
			endianness = 1;
		} else if ( vers != 1 ) {
			printf("unkown upix version %u (0x%x)\n", vers, vers );
			exit(1);
		}
	}
	{
		int32_t xpx, ypx;
		xpx = *((int32_t*)(data + 4));
		ypx = *((int32_t*)(data + 8));
		if ( endianness ) {
			sov->pngWidth = swap32( xpx );
			sov->pngHeight = swap32( ypx );
		} else {
			sov->pngWidth = xpx;
			sov->pngHeight = ypx;
		}
	}
	off_t pos = 12;
	while ( pos < mf.sb.st_size ) {
		uint64_t tubid;
		off_t ep;
		int index;
		int newpoints;
		
		memcpy( &tubid, (void*)(data + pos), 8 );
		if ( endianness ) {
			tubid = swap64( tubid );
		}
		pos += 8;
		ep = pos;
		while ( ep < mf.sb.st_size && *((uint32_t*)(data + ep)) != 0xffffffff ) {
			ep += 4;
		}
		newpoints = (ep - pos) / 4;
		index = sov->gd->indexOfUbid(tubid);
		if ( index >= 0 ) {
			pxlist* cpx;
			uint16_t* dest;
			uint16_t* src;
			//printf("%013llu\n", tubid );
			cpx = px + index;
			if ( cpx->px != NULL ) {
				cpx->px = (uint16_t*)realloc( cpx->px, sizeof(uint16_t)*((cpx->numpx + newpoints)*2) );
				assert( cpx->px != NULL );
				dest = cpx->px + (cpx->numpx * 2);
				src = (uint16_t*)(data + pos);
				cpx->numpx += newpoints;
			} else {
				cpx->px = (uint16_t*)malloc( sizeof(uint16_t)*newpoints*2 );
				dest = cpx->px;
				src = (uint16_t*)(data + pos);
				cpx->numpx = newpoints;
			}
			if ( endianness ) {
				int pxi;
				for ( pxi = 0; pxi < newpoints*2; pxi++ ) {
					dest[pxi] = swap16( src[pxi] );
				}
			} else {
				memcpy( dest, src, sizeof(uint16_t)*newpoints*2 );
			}
		} else {
			printf("%013llu no index!\n", tubid );
		}
		pos = ep + 4;
	}
	
	mf.close();
}

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
			minlat = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--minlatd" ) ) {
			i++;
			minlat = strtod( argv[i], NULL ) + maxlat;
		} else if ( ! strcmp( argv[i], "--minlon" ) ) {
			i++;
			minlon = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--minlond" ) ) {
			i++;
			minlon = strtod( argv[i], NULL ) + maxlon;
		} else if ( ! strcmp( argv[i], "--maxlon" ) ) {
			i++;
			maxlon = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--maxlond" ) ) {
			i++;
			maxlon = strtod( argv[i], NULL ) + minlon;
		} else if ( ! strcmp( argv[i], "--maxlat" ) ) {
			i++;
			maxlat = strtod( argv[i], NULL );
		} else if ( ! strcmp( argv[i], "--maxlatd" ) ) {
			i++;
			maxlat = strtod( argv[i], NULL ) + minlat;
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
	}
	//sov.init();
	
	if ( upxfname != NULL ) {
		globalpxlist = new pxlist[sov.gd->numPoints];
		readUPix( &sov, upxfname, globalpxlist );
	}
	
	// init PNG image data area
	initDataAndRows( sov.pngWidth, sov.pngHeight );

	if ( commandFileName != NULL ) {
		runDrendCommandFile( sov );
	} else {
		runBasic( sov );
	}

	//free( rows );
	//free( data );

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
	sov.doPNG_r( data, rows, sov.pngWidth, sov.pngHeight, sov.pngname );
	
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
inline int mystrmatch( const char* src, const char* key ) {
	while ( *key != '\0' ) {
		if ( *src != *key ) {
			return 0;
		}
		src++; key++;
	}
	return 1;
}
#define SKIPCMD() while( (pos < cfm.sb.st_size) && (!isspace(cf[pos])) ){pos++;}\
while( (pos < cfm.sb.st_size) && (isspace(cf[pos])) ){pos++;}
#define GETARG() while( (pos < cfm.sb.st_size) && (!isspace(cf[pos])) ){pos++;}\
while( (pos < cfm.sb.st_size) && (isspace(cf[pos])) ){pos++;}\
p2=pos+1;\
while( (p2 < cfm.sb.st_size) && (!isspace(cf[p2])) ){p2++;}\
if ( (pos < cfm.sb.st_size) && (p2 <= cfm.sb.st_size) ) {\
	tmp = (char*)malloc( p2-pos + 1 ); assert(tmp != NULL);\
	memcpy( tmp, cf + pos, p2-pos );\
	tmp[p2-pos]='\0'; pos = p2;\
} else { fprintf(stderr,"error getting arg, pos=%d p2=%d\n", pos,p2);exit(1); }

void runDrendCommandFile( Solver& sov ) {
	Adjacency ta;
	mmaped cfm;
	cfm.open( commandFileName );
	char* cf = (char*)cfm.data;
	int pos = 0;
	int p2;
	char* tmp = NULL;
	int pngWidth = sov.pngWidth;
	int pngHeight = sov.pngHeight;
#if 0
	char* pngname = NULL;
	if ( sov.pngname != NULL ) {
		pngname = strdup( sov.pngname );
	}
#endif
	while( (pos < cfm.sb.st_size) && (isspace(cf[pos])) ){pos++;}
	while ( pos < cfm.sb.st_size ) {
		if ( cf[pos] == '#' || (cf[pos] == '/' && cf[pos+1] == '/') ) {
			while( (pos < cfm.sb.st_size) && (cf[pos] != '\n') ){pos++;}
			while( (pos < cfm.sb.st_size) && (isspace(cf[pos])) ){pos++;}
			continue;
		}
		if ( mystrmatch( cf + pos, "--loadSolution" ) ) {
			GETARG();
			printf("loading \"%s\"\n", tmp );
			sov.loadZSolution( tmp );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "done" ) ) {
			return;
		} else if ( mystrmatch( cf + pos, "renumber" ) ) {
			SKIPCMD();
			sov.californiaRenumber();
			sov.calculateAdjacency(&ta);
			recolorDists( ta.adjacency, ta.adjlen, sov.districts, sov.renumber );
		} else if ( mystrmatch( cf + pos, "recolor" ) ) {
			SKIPCMD();
			sov.calculateAdjacency(&ta);
			recolorDists( ta.adjacency, ta.adjlen, sov.districts );
		} else if ( mystrmatch( cf + pos, "--pngout" ) ) {
			GETARG();
			printf("writing \"%s\"\n", tmp );
			if ( data == NULL || rows == NULL ) {
				initDataAndRows( pngWidth, pngHeight );
			}
			sov.doPNG_r( data, rows, pngWidth, pngHeight, tmp );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--colorsIn" ) ) {
			GETARG();
			FILE* cfi = fopen( tmp, "r" );
			if ( cfi == NULL ) {
				perror(tmp);
				exit(1);
			}
			readColoring( cfi );
			fclose( cfi );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--colorsOut" ) ) {
			GETARG();
			FILE* cfo = fopen( tmp, "w" );
			if ( cfo == NULL ) {
				perror(tmp);
				exit(1);
			}
			printColoring( cfo );
			fclose( cfo );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--pngW" ) ) {
			GETARG();
			pngWidth = strtol( tmp, NULL, 10 );
			maybeClearDataAndRows();
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--pngH" ) ) {
			GETARG();
			pngHeight = strtol( tmp, NULL, 10 );
			maybeClearDataAndRows();
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--minlond" ) ) {
			GETARG();
			minlon = strtod( tmp, NULL ) + maxlon;
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--minlon" ) ) {
			GETARG();
			minlon = strtod( tmp, NULL );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--maxlond" ) ) {
			GETARG();
			maxlon = strtod( tmp, NULL ) + minlon;
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--maxlon" ) ) {
			GETARG();
			maxlon = strtod( tmp, NULL );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--minlatd" ) ) {
			GETARG();
			minlat = strtod( tmp, NULL ) + maxlat;
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--minlat" ) ) {
			GETARG();
			minlat = strtod( tmp, NULL );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--maxlatd" ) ) {
			GETARG();
			maxlat = strtod( tmp, NULL ) + minlat;
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--maxlat" ) ) {
			GETARG();
			maxlat = strtod( tmp, NULL );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "-px" ) ) {
			GETARG();
			globalpxlist = new pxlist[sov.gd->numPoints];
			readUPix( &sov, tmp, globalpxlist );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "clearpx" ) ) {
			SKIPCMD();
			delete [] globalpxlist;
			globalpxlist = NULL;
#if 0
		} else if ( mystrmatch( cf + pos, "" ) ) {
			GETARG();
			free( tmp ); tmp = NULL;
#endif
		} else {
			fprintf(stderr,"bogus command \"%10s\"...\n", cf + pos );
			exit(1);
		}
		while( (pos < cfm.sb.st_size) && (isspace(cf[pos])) ){pos++;}
	}
}


extern const unsigned char* colors;
extern int numColors;

uint8_t backgroundColor[3] = { 0xd0,0xd0,0xd0 };

void Solver::doPNG_r( unsigned char* data, unsigned char** rows, int pngWidth, int pngHeight, const char* pngname ) {
	if ( globalpxlist != NULL ) {
		// use blackground color
		for ( int y = 0; y < pngHeight; y++ ) {
			unsigned char* row;
			row = rows[y];
			for ( int x = 0; x < pngWidth; x++ ) {
				unsigned char* px;
				px = row + (x*3);
				px[0] = backgroundColor[0];
				px[1] = backgroundColor[1];
				px[2] = backgroundColor[2];
			}
		}
	} else {
		// points on black background
		memset( data, 0x0, pngWidth*pngHeight*3*sizeof(unsigned char) );
	}
	
#if READ_INT_POS
	int lminx, lminy, lmaxx, lmaxy;
	//#define SRC_TO_LM( x ) ((x) * 1000000.0)
#define DPRSET( dim, src ) if ( ! isnan( src ) ) {\
		l##dim = (int)(1000000.0 * (src) );	  \
	} else { \
		l##dim = dim;\
	}
#elif READ_DOUBLE_POS
	double lminx, lminy, lmaxx, lmaxy;
	//#define SRC_TO_LM( x ) (x)
#define DPRSET( dim, src ) if ( ! isnan( src ) ) {\
		l##dim = ( src ); \
	} else { \
		l##dim = dim;\
	}
#else
#error "what type is pos?"
#endif

	DPRSET( minx, minlon );
	DPRSET( maxx, maxlon );
	DPRSET( miny, minlat );
	DPRSET( maxy, maxlat );

//fprintf(stderr,"minx=%10d miny=%10d maxx=%10d maxy=%10d w=%d h=%d\n", minx, miny, maxx, maxy, pngWidth, pngHeight );
	/* setup transformation */
	double ym = 0.999 * pngHeight / (lmaxy - lminy);
	double xm = 0.999 * pngWidth / (lmaxx - lminx);
	
	for ( int i = 0; i < gd->numPoints; i++ ) {
		double ox, oy;
		const unsigned char* color;
		if ( winner[i] == NODISTRICT ) {
			static const unsigned char colorNODISTRICT[3] = { 0,0,0 };
			color = colorNODISTRICT;
		} else {
			color = colors + ((winner[i] % numColors) * 3);
		}
		if ( globalpxlist != NULL ) {
			pxlist* cpx = globalpxlist + i;
			if ( cpx->numpx <= 0 ) {
				continue;
			}
			for ( int j = 0; j < cpx->numpx; j++ ) {
				int x, y;
				x = cpx->px[j*2];
				if ( x < 0 || x > pngWidth ) {
					fprintf(stderr,"index %d ubid %013llu x (%d) out of bounds\n", i, gd->ubidOfIndex(i), x );
					continue;
				}
				y = cpx->px[j*2 + 1];
				if ( y < 0 || y > pngHeight ) {
					fprintf(stderr,"index %d ubid %013llu y (%d) out of bounds\n", i, gd->ubidOfIndex(i), y );
					continue;
				}
				unsigned char* row;
				row = data + (y*pngWidth*3);
				x *= 3;
				row[x  ] = color[0];
				row[x+1] = color[1];
				row[x+2] = color[2];
			}
		} else {
			if ( gd->pos[i*2] < lminx ) {
				continue;
			}
			if ( gd->pos[i*2] > lmaxx ) {
				continue;
			}
			if ( gd->pos[i*2+1] < lminy ) {
				continue;
			}
			if ( gd->pos[i*2+1] > lmaxy ) {
				continue;
			}
			oy = (lmaxy - gd->pos[i*2+1]) * ym;
			ox = (gd->pos[i*2  ] - lminx) * xm;
			int y, x;
			y = (int)oy;
			unsigned char* row;
			row = data + (y*pngWidth*3);
			x = (int)ox;
			x *= 3;
			row[x  ] = color[0];
			row[x+1] = color[1];
			row[x+2] = color[2];
		}
	}
#if USE_EDGE_LOOP && 0
	for ( POPTYPE d = 0; d < districts; d++ ) {
		District2* cd;
		const unsigned char* color;
		cd = dists + d;
		color = colors + ((d % numColors) * 3);
		for ( District::EdgeNode* en = cd->edgelistRoot; en != cd->edgelistRoot; en = en->next ) {
			int pi;
			pi = en->nodeIndex;//cd->edgelist[i].nodeIndex;
				double ox, oy;
				oy = (maxy - gd->pos[pi*2+1]) * ym;
				ox = (gd->pos[pi*2  ] - minx) * xm;
				int y, x;
				y = (int)oy;
				unsigned char* row;
				row = data + (y*pngWidth*3);
				x = (int)ox;
				x *= 3;
				row[x] = color[0];
				row[x+1] = color[1];
				row[x+2] = color[2];
		}
	}
#endif
	
	myDoPNG( pngname, rows, pngHeight, pngWidth );
}
