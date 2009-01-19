#ifndef MAP_DRAWER_H
#define MAP_DRAWER_H

#include <stdint.h>

class Solver;
class pxlist;

class MapDrawer {
public:
uint8_t* data; // = NULL;
uint8_t** rows; // = NULL;
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
	
	void setIndexColor(Solver* sov, int index, uint8_t red, uint8_t green, uint8_t blue);
	inline void setPoint(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
		uint8_t* row;
		row = data + (y*width*bytesPerPixel);
		x *= bytesPerPixel;
		row[x  ] = red;
		row[x+1] = green;
		row[x+2] = blue;
	}
};


extern uint8_t backgroundColor[];

#endif /* MAP_DRAWER_H */
