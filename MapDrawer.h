#ifndef MAP_RASTERIZER_H
#define MAP_RASTERIZER_H

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

MapDrawer();

void maybeClearDataAndRows();
void initDataAndRows();
void readUPix( const Solver* sov, char* upfname );
inline void setSize(int width, int height) {
	this->width = width;
	this->height = height;
}

void doPNG_r( Solver* sov, const char* pngname );
void runDrendCommandFile( Solver& sov, const char* commandFileName );
};

#endif /* MAP_RASTERIZER_H */
