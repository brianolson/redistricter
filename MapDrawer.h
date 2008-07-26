#ifndef MAP_DRAWER_H
#define MAP_DRAWER_H

#include <stdint.h>

class Solver;
namespace {
class pxlist;
}

class MapDrawer {
public:
unsigned char* data; // = NULL;
unsigned char** rows; // = NULL;
int width; // pixels
int height; // pixels
pxlist* px;
double minlat;
double minlon;
double maxlat;
double maxlon;
int bytesPerPixel;  // defaults to 3 for png output. set to 4 for screen.

MapDrawer();

void maybeClearDataAndRows();
void initDataAndRows();
void readUPix( const Solver* sov, const char* upfname );
inline void setSize(int width, int height) {
	this->width = width;
	this->height = height;
}

void clearToBlack();
void clearToBackgroundColor();
// Use rendered pixel list from makepolys loaded by readUPix()
void paintPixels( Solver* sov );
// Dot per block
void paintPoints( Solver* sov );
void doPNG_r( Solver* sov, const char* pngname );
void runDrendCommandFile( Solver& sov, const char* commandFileName );
};


extern uint8_t backgroundColor[];

#endif /* MAP_DRAWER_H */
