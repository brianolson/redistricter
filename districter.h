#ifndef DISTRICTER_H
#define DISTRICTER_H

#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// storage floating type
#ifndef SFTYPE
#define SFTYPE double
#endif
// just has to be big enough to identify a district number. unsigned char good up through 255 districts
#ifndef POPTYPE
#define POPTYPE uint8_t
#define POPTYPE_MAX 0xfc
#endif

#ifndef NODISTRICT
#define NODISTRICT 0xff
#endif

#ifndef ERRDISTRICT
#define ERRDISTRICT 0xfe
#endif

#ifndef WITH_PNG
#if NOPNG
#define WITH_PNG 0
#else
#define WITH_PNG 1
#endif
#endif

class LatLong {
public:
	SFTYPE lat, lon;
};

class PosPop {
public:
	LatLong pos;
	SFTYPE pop;
};

class Edge {
public:
	int index;
	Edge* next;
};

#include "GeoData.h"

#if WITH_PNG
void myDoPNG( const char* outname, unsigned char** rows, int height, int width ) __attribute__ (( weak ));
#pragma weak myDoPNG
void renderDistricts( int numZips, PosPop* zipd, double* rs, POPTYPE* winner, char* outname, int height = 1000, int width = 1000 );
void recolorDists( POPTYPE* adjacency, int adjlen, int numd, POPTYPE* renumber = NULL ) __attribute__ (( weak ));
#pragma weak recolorDists
//void renderDistricts( int numZips, double* xy, double* popRadii, POPTYPE* winner, char* outname, int height = 1000, int width = 1000 );
void printColoring( FILE* fout );
void readColoring( FILE* fin );
#endif

class Node;
Node* initNodesFromLinksFile( GeoData* gd, const char* inputname );

static inline uint16_t swap16( uint16_t v ) {
	return ((v >> 8) &  0x00ff) |
	((v << 8) & 0xff00);
}
static inline uint32_t swap32( uint32_t v ) {
	return ((v >> 24) & 0xff) |
	((v >> 8) & 0xff00) |
	((v & 0xff00) << 8) |
	((v & 0xff) << 24);
}
static inline uint64_t swap64( uint64_t v ) {
	return
	((v & 0xff00000000000000ULL) >> 56) |
	((v & 0x00ff000000000000ULL) >> 40) |
	((v & 0x0000ff0000000000ULL) >> 24) |
	((v & 0x000000ff00000000ULL) >>  8) |
	((v & 0x00000000ff000000ULL) <<  8) |
	((v & 0x0000000000ff0000ULL) << 24) |
	((v & 0x000000000000ff00ULL) << 40) |
	((v & 0x00000000000000ffULL) << 56);
}

#endif
