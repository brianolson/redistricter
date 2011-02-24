#include "logging.h"
#include "MapDrawer.h"
#include "Solver.h"
#include "tiger/mmaped.h"
#include "swap.h"
#include "GeoData.h"
#include "renderDistricts.h"
#include "redata.pb.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/gzip_stream.h>

MapDrawer::MapDrawer()
: data(NULL), rows(NULL),
width(-1), height(-1),
px(NULL),
minlat(NAN), minlon(NAN), maxlat(NAN), maxlon(NAN),
bytesPerPixel(4) {}

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
	~pxlist() {
		if (px != NULL) {
			free(px);
		}
	}
};

class pxlistElement {
public:
	pxlist* it;
	pxlistElement* next;
	// index within internal arrays in sf1-geo order
	int index;
	
	pxlistElement(pxlist* list, int index_, pxlistElement* next_) : it(list), next(next_), index(index_) {
		// pass
	}
	
private:
	static int numAllocatedBlocks;
	static pxlistElement** allocatedBlocks;
	static pxlistElement* freeList;
	static const int allocBlockSize = 100;
public:
	void* operator new(size_t) {
		if (freeList == NULL) {
			if (allocatedBlocks == NULL) {
				allocatedBlocks = (pxlistElement**)malloc(sizeof(pxlistElement*));
			} else {
				allocatedBlocks = (pxlistElement**)realloc(allocatedBlocks, sizeof(pxlistElement*) * (numAllocatedBlocks + 1));
			}
			allocatedBlocks[numAllocatedBlocks] = (pxlistElement*)malloc(sizeof(pxlistElement*) * allocBlockSize);
			for (int i = 0; i < allocBlockSize; ++i) {
				allocatedBlocks[numAllocatedBlocks][i].next = freeList;
				freeList = &(allocatedBlocks[numAllocatedBlocks][i]);
			}
			numAllocatedBlocks++;
		}
		pxlistElement* out = freeList;
		freeList = freeList->next;
		return out;
	}
	void operator delete(void* it) {
		pxlistElement* e = (pxlistElement*)it;
		e->next = freeList;
		freeList = e;
	}
};
// static
int pxlistElement::numAllocatedBlocks = 0;
pxlistElement** pxlistElement::allocatedBlocks = NULL;
pxlistElement* pxlistElement::freeList = NULL;

class pxlistGrid {
public:
	int rows;
	int cols;
	int width;
	int height;
	pxlistElement** they;
	
	pxlistGrid() : rows(-1), cols(-1), width(-1), height(-1), they(NULL) {
		// pass
	}
	
	void build(int rows_, int cols_, pxlist* px, int numPoints, int width_, int height_) {
		if (they != NULL) {
			for (int i = 0; i < rows*cols; ++i) {
				pxlistElement* cur = they[i];
				while (cur != NULL) {
					pxlistElement* next = cur->next;
					delete cur;
					cur = next;
				}
			}
			delete [] they;
		}
		rows = rows_;
		cols = cols_;
		width = width_;
		height = height_;
		they = new pxlistElement*[rows * cols];
		// buckets to which this pxlist should be added.
		int* cells = new int[rows*cols];
		int numcells;
		for (int i = 0; i < numPoints; ++i) {
			pxlist* cur = &(px[i]);
			numcells = 0;
			for (int p = 0; p < cur->numpx; ++p) {
				int pcell = pointToBucket(cur->px[p*2], cur->px[p*2 + 1]);
				for (int c = 0; c < numcells; ++c) {
					if (pcell == cells[c]) {
						pcell = -1;
						break;
					}
				}
				if (pcell >= 0) {
					cells[numcells] = pcell;
					numcells++;
				}
			}
			for (int c = 0; c < numcells; ++c) {
				they[cells[c]] = new pxlistElement(cur, i, they[cells[c]]);
			}
		}
	}
	
	inline int pointToBucket(int x, int y) {
		return ((y / height) * cols) + (x / width);
	}
	
#if 0
	// TODO DELETE
	inline pxlistElement* rc(int x, int y) {
		assert(x >= 0);
		assert(x < rows);
		assert(y >= 0);
		assert(y <= cols);
		return they[(y*rows) + x];
	}
#endif
	
	int pointToIndex(int x, int y) {
		pxlistElement* cur = they[pointToBucket(x, y)];
		while (cur != NULL) {
			for (int i = 0; i < cur->it->numpx; ++i) {
				if ((cur->it->px[i*2] == x) && (cur->it->px[i*2 + 1] == y)) {
					return cur->index;
				}
			}
			cur = cur->next;
		}
		return -1;
	}
};

bool MapDrawer::readUPix( const Solver* sov, const char* upfname ) {
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
			assert(0);
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
	return true;
}

bool MapDrawer::readMapRasterization( const Solver* sov, const char* mppb_path ) {
	int fd = open(mppb_path, O_RDONLY);
	if (fd < 0) {
		perror(mppb_path);
		return false;
	}	
	google::protobuf::io::FileInputStream pbfin(fd);
	google::protobuf::io::GzipInputStream zin(&pbfin);
	MapRasterization map;
	bool ok = map.ParseFromZeroCopyStream(&zin);
	if (!ok) {
		fprintf(stderr, "%s: error: failed to parse MapRasterization\n", mppb_path);
		return false;
	}
	pbfin.Close();
	
	width = map.sizex();
	assert(width > 0);
	height = map.sizey();
	assert(height > 0);
	
	if (px != NULL) {
		delete [] px;
		px = NULL;
	}
	if (px == NULL) {
		px = new pxlist[sov->gd->numPoints];
	}
	for (int i = 0; i < map.block_size(); ++i) {
		const MapRasterization::Block& b = map.block(i);
		if (!b.has_ubid()) {
			fprintf(stderr, "%s: error: block %d without ubid\n", mppb_path, i);
			return false;
		}
		uint64_t tubid = b.ubid();
		uint32_t index = sov->gd->indexOfUbid(tubid);
		if ( index != (uint32_t)-1 ) {
			pxlist* cpx;
			int blockpoints = b.xy_size() / 2;
			cpx = px + index;
			int nexti = cpx->numpx * 2;
			if ( cpx->px != NULL ) {
				cpx->px = (uint16_t*)realloc( cpx->px, sizeof(uint16_t)*((cpx->numpx + blockpoints)*2) );
				assert( cpx->px != NULL );
				cpx->numpx += blockpoints;
			} else {
				cpx->px = (uint16_t*)malloc( sizeof(uint16_t)*blockpoints*2 );
				cpx->numpx = blockpoints;
			}
			for (int pi = 0; pi < b.xy_size(); ++pi) {
				assert(b.xy(pi) >= 0);
				//assert(b.xy(pi) <= 65535);
				if (pi & 1) {
					assert(b.xy(pi) <= height);
				} else {
					assert(b.xy(pi) <= width);
				}
				cpx->px[nexti + pi] = b.xy(pi);
				if (pi & 1) {
					assert(cpx->px[pi] <= height);
				} else {
					assert(cpx->px[pi] <= width);
				}
			}
			for (int xxxi = 0; xxxi < cpx->numpx; xxxi += 2) {
				assert(cpx->px[xxxi] <= width);
				assert(cpx->px[xxxi+1] <= height);
			}
		} else {
			fprintf(stderr, "%013llu no index!\n", tubid );
		}
	}
	return true;
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
		} else if ( mystrmatch( cf + pos, "--mppb" ) ) {
			GETARG();
			readMapRasterization( &sov, tmp );
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
			if (bytesPerPixel == 4) {
				px[3] = 0; // 0% alpha
			}
		}
	}
}

#define DPRSET( dim, src ) if ( ! isnan( src ) ) {\
		l##dim = (int)(1000000.0 * (src) );	  \
	} else { \
		l##dim = sov->dim;\
	}

void MapDrawer::paintPoints( Solver* sov ) {
	int lminx, lminy, lmaxx, lmaxy;

	DPRSET( minx, minlon );
	DPRSET( maxx, maxlon );
	DPRSET( miny, minlat );
	DPRSET( maxy, maxlat );

	/* setup transformation */
	double ym = 0.999 * height / (lmaxy - lminy);
	double xm = 0.999 * width / (lmaxx - lminx);
        debugprintf(
            "min lat,lon=(%d, %d), max (%d, %d)\n",
            lminx, lminy, lmaxx, lmaxy);
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
		setPoint(x, y, color[0], color[1], color[2]);
	}
}

double _tpd(int pop, unsigned long long area) {
	double tpd = (1.0 * pop) / area;
	// could be logarithmic, but linear is working for now.
	tpd = log(tpd + 1.0);
	return tpd;
}

bool MapDrawer::getPopulationDensityRenderParams( Solver* sov, double* minpdP, double* pdrangeP ) {
	if ( px == NULL ) {
		return false;
	}
	int numPoints = sov->gd->numPoints;
	bool first = true;
	double minpd = 9999999;
	double maxpd = 0;
	// Find minimum and maximums so we can scale color gradient.
	for ( int i = 0; i < numPoints; i++ ) {
		pxlist* cpx = px + i;
		if ( cpx->numpx <= 0 ) {
			continue;
		}
		double tpd = _tpd(sov->gd->pop[i], sov->gd->area[i]);
		//fprintf(stderr, "%d: pop=%d area=%llu tpd=%g\n", i, sov->gd->pop[i], sov->gd->area[i], tpd);
		if (first) {
			minpd = maxpd = tpd;
			first = false;
		} else if (tpd < minpd) {
			minpd = tpd;
		} else if (tpd > maxpd) {
			maxpd = tpd;
		}
	}
	fprintf(stderr, "min density %g max density %g\n", minpd, maxpd);
	double pdrange = maxpd - minpd;
	int ghist[256];
	// TODO: shame on me, rewrite flow control away from goto.
tryagain:
	memset(ghist, 0, sizeof(ghist));
	for ( int i = 0; i < numPoints; i++ ) {
		pxlist* cpx = px + i;
		if ( cpx->numpx <= 0 ) {
			continue;
		}
		double tpd = _tpd(sov->gd->pop[i], sov->gd->area[i]);
		int grey = (int)(255.0 * (tpd - minpd) / pdrange);
		//fprintf(stderr, "tpd %g -> grey %d\n", tpd, grey);
		if (grey < 0) {
			fprintf(stderr, "gpdrp grey %d tpd=%g\n", grey, tpd);
			grey = 0;
		} else if (grey > 255) {
			//fprintf(stderr, "gpdrp grey %d tpd=%g\n", grey, tpd);
			grey = 255;
		}
		ghist[grey]++;
	}
	int newHighGrey = 255;
	// TODO: make clamp fraction configurable.
	// clamp top 5% to white (smooshes outliers)
	int pointsClamped = 0;
	while (pointsClamped < (numPoints / 40)) {
		pointsClamped += ghist[newHighGrey];
		//fprintf(stderr, "drop %d points at grey=%d\n", ghist[newHighGrey], newHighGrey);
		newHighGrey--;
		if (newHighGrey == 0) {
			fprintf(stderr, "too squashed, expand, try again\n");
			pdrange *= 1/255.0;
			goto tryagain;
		}
		assert(newHighGrey >= 0);
	}
	fprintf(stderr, "pdrange %g newHighGrey %d\n", pdrange, newHighGrey);
	pdrange *= (newHighGrey / 255.0);
	*minpdP = minpd;
	*pdrangeP = pdrange;
	return true;
}

// Piant population density (pop/area)
void MapDrawer::paintPopulation( Solver* sov ) {
	if ( px == NULL ) {
		fprintf(stderr, "not painting population without --px rendering\n");
		return;
	}
	int numPoints = sov->gd->numPoints;
	double minpd = 0.0;
	double pdrange;
	/*bool ok =*/ getPopulationDensityRenderParams(sov, &minpd, &pdrange);
	fprintf(stderr, "minpd %g range %g\n", minpd, pdrange);
	int ghist[256];
	memset(ghist, 0, sizeof(ghist));
	int blocksSkipped = 0;
	for ( int i = 0; i < numPoints; i++ ) {
		pxlist* cpx = px + i;
		if ( cpx->numpx <= 0 ) {
			blocksSkipped++;
			continue;
		}
		double tpd = _tpd(sov->gd->pop[i], sov->gd->area[i]);
		int grey = (int)(255.0 * (tpd - minpd) / pdrange);
		//fprintf(stderr, "tpd %g -> grey %d\n", tpd, grey);
		if (grey < 0) {
			fprintf(stderr, "grey %d tpd=%g\n", grey, tpd);
			grey = 0;
		} else if (grey > 255) {
			//fprintf(stderr, "grey %d tpd=%g\n", grey, tpd);
			grey = 255;
		}
		ghist[grey]++;
		uint8_t alpha = abs(grey - 128) / 4;
		for ( int j = 0; j < cpx->numpx; j++ ) {
			int x, y;
			x = cpx->px[j*2];
			if ( x < 0 || x > width ) {
				fprintf(stderr,"index %d[%d] x=%d out of bounds (0<=x<=%d)\n", i, j, x, width );
				continue;
			}
			y = cpx->px[j*2 + 1];
			if ( y < 0 || y > height ) {
				fprintf(stderr,"index %d y (%d) out of bounds (0<=y<=%d)\n", i, y, height );
				continue;
			}
			setPoint(x, y, grey, grey, grey, alpha);
		}
	}
#if 0
	for (int i = 0; i < 256; ++i) {
		fprintf(stderr, "ghist[%d] %d\n", i, ghist[i]);
	}
#endif
	fprintf(stderr, "%d blocks skipped due to being represented by no pixels\n", blocksSkipped);
}
void MapDrawer::paintPixels( Solver* sov ) {
	if ( px == NULL ) {
		paintPoints( sov );
		return;
	}
	int lminx, lminy, lmaxx, lmaxy;

	DPRSET( minx, minlon );
	DPRSET( maxx, maxlon );
	DPRSET( miny, minlat );
	DPRSET( maxy, maxlat );
	
	POPTYPE* winner = sov->winner;
	int numPoints = sov->gd->numPoints;
	int blocksSkipped = 0;
	double minpd = 9999999;
	double pdrange;
	// TODO: make a command line flag to turn this off.
	bool doPopDensityShading = getPopulationDensityRenderParams(sov, &minpd, &pdrange);
	
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
			blocksSkipped++;
			continue;
		}
		double alpha = 1.0;
		double oma = 0.0;
		int grey;
		if (doPopDensityShading) {
			double tpd = _tpd(sov->gd->pop[i], sov->gd->area[i]);
			grey = (int)(255.0 * (tpd - minpd) / pdrange);
			if (grey > 255) {
				grey = 255;
			} else if (grey < 0) {
				grey = 0;
			}
			alpha = abs(grey - 128) / (3.0 * 255);
			oma = 1.0 - alpha;
			grey = grey * alpha;
		}
		for ( int j = 0; j < cpx->numpx; j++ ) {
			int x, y;
			x = cpx->px[j*2];
			if ( x < 0 || x > width ) {
				fprintf(stderr,"index %d[%d] x=%d out of bounds (0<=x<=%d)\n", i, j, x, width );
				continue;
			}
			y = cpx->px[j*2 + 1];
			if ( y < 0 || y > height ) {
				fprintf(stderr,"index %d y (%d) out of bounds (0<=y<=%d)\n", i, y, height );
				continue;
			}
			if (doPopDensityShading) {
				setPoint(x, y,
					(color[0] * oma) + grey,
					(color[1] * oma) + grey,
					(color[2] * oma) + grey);
			} else {
				setPoint(x, y, color[0], color[1], color[2]);
			}
		}
	}
	fprintf(stderr, "%d blocks skipped due to being represented by no pixels\n", blocksSkipped);
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
				fprintf(stderr,"index %d x (%d) out of bounds (0<=x<=%d)\n", index, x, width );
				return;
			}
			y = cpx->px[j*2 + 1];
			if ( y < 0 || y > height ) {
				fprintf(stderr,"index %d y (%d) out of bounds (0<=y<=%d)\n", index, y, height );
				return;
			}
			setPoint(x, y, red, green, blue);
		}
	} else {
		int lminx, lminy, lmaxx, lmaxy;
		
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
