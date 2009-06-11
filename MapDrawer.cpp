#include "logging.h"
#include "MapDrawer.h"
#include "Solver.h"
#include "tiger/mmaped.h"
#include "swap.h"
#include "GeoData.h"
#include "renderDistricts.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

MapDrawer::MapDrawer()
: data(NULL), rows(NULL),
width(-1), height(-1),
px(NULL),
minlat(NAN), minlon(NAN), maxlat(NAN), maxlon(NAN),
bytesPerPixel(3) {}

void MapDrawer::maybeClearDataAndRows() {
	if ( data != NULL ) {
		free( data );
		data = NULL;
	}
	if ( rows != NULL ) {
		free( rows );
		rows = NULL;
	}
}
void MapDrawer::initDataAndRows() {
	maybeClearDataAndRows();
	assert(width > 0);
	assert(height > 0);
	data = (unsigned char*)malloc(width*height*bytesPerPixel*sizeof(unsigned char) );
	rows = (unsigned char**)malloc(height*sizeof(unsigned char*) );
	assert(data != NULL);
	assert(rows != NULL);
	
	for ( int y = 0; y < height; y++ ) {
		rows[y] = data + (y*width*bytesPerPixel);
	}
}

class pxlist {
public:
	int numpx;
	uint16_t* px;
	pxlist() : numpx( 0 ), px( NULL ) {};
};

void MapDrawer::readUPix( const Solver* sov, const char* upfname ) {
	mmaped mf;
	uintptr_t filemem;
	mf.open( upfname );
	int endianness = 0;
	if (px != NULL) {
		delete [] px;
		px = NULL;
	}
	if (px == NULL) {
		px = new pxlist[sov->gd->numPoints];
	}
	
	filemem = (uintptr_t)mf.data;
	// int32_t vers, x, y;
	{
		uint32_t vers;
		vers = *((uint32_t*)filemem);
		if ( swap32( vers ) == 1 ) {
			endianness = 1;
		} else if ( vers != 1 ) {
			printf("unkown upix version %u (0x%x)\n", vers, vers );
			exit(1);
		}
	}
	{
		int32_t xpx, ypx;
		xpx = *((int32_t*)(filemem + 4));
		ypx = *((int32_t*)(filemem + 8));
		if ( endianness ) {
			width = swap32( xpx );
			height = swap32( ypx );
		} else {
			width = xpx;
			height = ypx;
		}
	}
	off_t pos = 12;
	while ( pos < mf.sb.st_size ) {
		uint64_t tubid;
		off_t ep;
		uint32_t index;
		int newpoints;
		
		memcpy( &tubid, (void*)(filemem + pos), 8 );
		if ( endianness ) {
			tubid = swap64( tubid );
		}
		pos += 8;
		ep = pos;
		while ( ep < mf.sb.st_size && *((uint32_t*)(filemem + ep)) != 0xffffffff ) {
			ep += 4;
		}
		newpoints = (ep - pos) / 4;
		index = sov->gd->indexOfUbid(tubid);
		if ( index != (uint32_t)-1 ) {
			pxlist* cpx;
			uint16_t* dest;
			uint16_t* src;
			//printf("%013llu\n", tubid );
			cpx = px + index;
			if ( cpx->px != NULL ) {
				cpx->px = (uint16_t*)realloc( cpx->px, sizeof(uint16_t)*((cpx->numpx + newpoints)*2) );
				assert( cpx->px != NULL );
				dest = cpx->px + (cpx->numpx * 2);
				src = (uint16_t*)(filemem + pos);
				cpx->numpx += newpoints;
			} else {
				cpx->px = (uint16_t*)malloc( sizeof(uint16_t)*newpoints*2 );
				dest = cpx->px;
				src = (uint16_t*)(filemem + pos);
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

void MapDrawer::runDrendCommandFile(
		Solver& sov, const char* commandFileName) {
	Adjacency ta;
	mmaped cfm;
	cfm.open( commandFileName );
	char* cf = (char*)cfm.data;
	int pos = 0;
	int p2;
	char* tmp = NULL;
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
				initDataAndRows();
			}
			doPNG_r( &sov, tmp );
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
			width = strtol( tmp, NULL, 10 );
			maybeClearDataAndRows();
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "--pngH" ) ) {
			GETARG();
			height = strtol( tmp, NULL, 10 );
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
			readUPix( &sov, tmp );
			free( tmp ); tmp = NULL;
		} else if ( mystrmatch( cf + pos, "clearpx" ) ) {
			SKIPCMD();
			delete [] px;
			px = NULL;
#if 0
		// template entry
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
	cfm.close();
}

extern const unsigned char* colors;
extern int numColors;

uint8_t backgroundColor[3] = { 0xd0,0xd0,0xd0 };

void MapDrawer::doPNG_r( Solver* sov, const char* pngname ) {
	if ( px ) {
		clearToBackgroundColor();
		paintPixels( sov );
	} else {
		clearToBlack();
		paintPoints( sov );
	}
	myDoPNG( pngname, rows, height, width );
}

void MapDrawer::clearToBlack() {
	memset( data, 0x0, width*height*bytesPerPixel );
}
void MapDrawer::clearToBackgroundColor() {
	for ( int y = 0; y < height; y++ ) {
		unsigned char* row;
		row = rows[y];
		for ( int x = 0; x < width; x++ ) {
			unsigned char* px;
			px = row + (x*bytesPerPixel);
			px[0] = backgroundColor[0];
			px[1] = backgroundColor[1];
			px[2] = backgroundColor[2];
		}
	}
}

#if READ_INT_POS
#define DPRSET( dim, src ) if ( ! isnan( src ) ) {\
		l##dim = (int)(1000000.0 * (src) );	  \
	} else { \
		l##dim = sov->dim;\
	}
#elif READ_DOUBLE_POS
#define DPRSET( dim, src ) if ( ! isnan( src ) ) {\
		l##dim = ( src ); \
	} else { \
		l##dim = sov->dim;\
	}
#else
#error "what type is pos?"
#endif

void MapDrawer::paintPoints( Solver* sov ) {
#if READ_INT_POS
	int lminx, lminy, lmaxx, lmaxy;
#elif READ_DOUBLE_POS
	double lminx, lminy, lmaxx, lmaxy;
#else
#error "what type is pos?"
#endif

	DPRSET( minx, minlon );
	DPRSET( maxx, maxlon );
	DPRSET( miny, minlat );
	DPRSET( maxy, maxlat );

	/* setup transformation */
	double ym = 0.999 * height / (lmaxy - lminy);
	double xm = 0.999 * width / (lmaxx - lminx);
#if READ_INT_POS
        debugprintf(
            "min lat,lon=(%d, %d), max (%d, %d)\n",
            lminx, lminy, lmaxx, lmaxy);
#elif READ_DOUBLE_POS
        debugprintf(
            "min lat,lon=(%f, %f), max (%f, %f)\n",
            lminx, lminy, lmaxx, lmaxy);
#endif
        debugprintf("scale x,y=(%f, %f)\n", xm, ym);
	
	GeoData* gd = sov->gd;
	POPTYPE* winner = sov->winner;
	
	for ( int i = 0; i < gd->numPoints; i++ ) {
		double ox, oy;
		const unsigned char* color;
		if ( winner[i] == NODISTRICT ) {
			static const unsigned char colorNODISTRICT[3] = { 0,0,0 };
			color = colorNODISTRICT;
		} else {
			color = colors + ((winner[i] % numColors) * 3);
		}
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
		x = (int)ox;
#if 1
		setPoint(x, y, color[0], color[1], color[2]);
#else
		unsigned char* row;
		row = data + (y*width*bytesPerPixel);
		x *= bytesPerPixel;
		row[x  ] = color[0];
		row[x+1] = color[1];
		row[x+2] = color[2];
#endif
	}
}

void MapDrawer::paintPixels( Solver* sov ) {
	if ( px == NULL ) {
		paintPoints( sov );
		return;
	}
#if READ_INT_POS
	int lminx, lminy, lmaxx, lmaxy;
#elif READ_DOUBLE_POS
	double lminx, lminy, lmaxx, lmaxy;
#else
#error "what type is pos?"
#endif

	DPRSET( minx, minlon );
	DPRSET( maxx, maxlon );
	DPRSET( miny, minlat );
	DPRSET( maxy, maxlat );
	
	POPTYPE* winner = sov->winner;
	int numPoints = sov->gd->numPoints;
	
	for ( int i = 0; i < numPoints; i++ ) {
		const unsigned char* color;
		if ( winner[i] == NODISTRICT ) {
			static const unsigned char colorNODISTRICT[3] = { 0,0,0 };
			color = colorNODISTRICT;
		} else {
			color = colors + ((winner[i] % numColors) * 3);
		}
		pxlist* cpx = px + i;
		if ( cpx->numpx <= 0 ) {
			continue;
		}
		for ( int j = 0; j < cpx->numpx; j++ ) {
			int x, y;
			x = cpx->px[j*2];
			if ( x < 0 || x > width ) {
				fprintf(stderr,"index %d x (%d) out of bounds\n", i, x );
				continue;
			}
			y = cpx->px[j*2 + 1];
			if ( y < 0 || y > height ) {
				fprintf(stderr,"index %d y (%d) out of bounds\n", i, y );
				continue;
			}
#if 1
			setPoint(x, y, color[0], color[1], color[2]);
#else
			unsigned char* row;
			row = data + (y*width*bytesPerPixel);
			x *= bytesPerPixel;
			row[x  ] = color[0];
			row[x+1] = color[1];
			row[x+2] = color[2];
#endif
		}
	}
}

void MapDrawer::setIndexColor(Solver* sov, int index, uint8_t red, uint8_t green, uint8_t blue) {
	if (px != NULL) {
		pxlist* cpx = px + index;
		if ( cpx->numpx <= 0 ) {
			return;
		}
		for ( int j = 0; j < cpx->numpx; j++ ) {
			int x, y;
			x = cpx->px[j*2];
			if ( x < 0 || x > width ) {
				fprintf(stderr,"index %d x (%d) out of bounds\n", index, x );
				return;
			}
			y = cpx->px[j*2 + 1];
			if ( y < 0 || y > height ) {
				fprintf(stderr,"index %d y (%d) out of bounds\n", index, y );
				return;
			}
			setPoint(x, y, red, green, blue);
		}
	} else {
#if READ_INT_POS
		int lminx, lminy, lmaxx, lmaxy;
#elif READ_DOUBLE_POS
		double lminx, lminy, lmaxx, lmaxy;
#else
#error "what type is pos?"
#endif
		
		DPRSET( minx, minlon );
		DPRSET( maxx, maxlon );
		DPRSET( miny, minlat );
		DPRSET( maxy, maxlat );
		
		/* setup transformation */
		double ym = 0.999 * height / (lmaxy - lminy);
		double xm = 0.999 * width / (lmaxx - lminx);
		
		GeoData* gd = sov->gd;
		double ox, oy;
		if ( gd->pos[index*2] < lminx ) {
			return;
		}
		if ( gd->pos[index*2] > lmaxx ) {
			return;
		}
		if ( gd->pos[index*2+1] < lminy ) {
			return;
		}
		if ( gd->pos[index*2+1] > lmaxy ) {
			return;
		}
		oy = (lmaxy - gd->pos[index*2+1]) * ym;
		ox = (gd->pos[index*2  ] - lminx) * xm;
		int y, x;
		y = (int)oy;
		x = (int)ox;
		setPoint(x, y, red, green, blue);
	}
}
