#ifndef RASTERIZE_TIGER_H
#define RASTERIZE_TIGER_H

#include <stdint.h>

class rtip;
class rt2;
class rtaPolyUbid;
class point;
class PointOutput;

class PolyGroup {
public:

	char* ifrootname;
	char* nameA;
	char* nameI;
	char* name1;
	char* name2;

	int numi, numa, num1, num2;

	rtip* rtippool;

	void buildRTI();
	rtip* rtipForTLID( uint32_t tlid );
	
	rt2* shapes;
	void buildShapes();
	int shapeIndexForTLID( uint32_t tlid );

	rtaPolyUbid* rtapu;
	rtaPolyUbid* rtapuForUbid( uint64_t ubid );

	void addpointstortapu( rtaPolyUbid* rp, int reverse, int si, uint32_t tlid,
		uint32_t startlon, int32_t startlat, int32_t endlon, int32_t endlat,
		uint32_t tzidstart, uint32_t tzidend );
	void setRootName( char* n );
	void processR1();

	inline void reconcile( PointOutput* fout ) {
		reconcile( fout, &PolyGroup::rasterizePointList );
	}
	void reconcile( PointOutput* fout,
		int (PolyGroup::* processPointList)(PointOutput*,uint64_t,point*) );

	double minlat;
	double minlon;
	double maxlat;
	double maxlon;
	int minlatset;
	int minlonset;
	int maxlatset;
	int maxlonset;
	int32_t xpx;
	int32_t ypx;

	uint8_t* maskpx;

	double pixelHeight;
	double pixelWidth;

	uint8_t maskgrey;

/*
	 _____________ maxlat
	 |   |   |   |
	 -------------
	 |   |   |   |
	 ------------- minlat
  minlon       maxlon
 
 every pixel should be in exactly one triangle, based on the center of the pixel.
 The outer edges of the pixel image will be at the min/max points.
 */
	inline double calcPixelHeight() {
		return (maxlat - minlat) / ypx;
	}
	inline double calcPixelWidth() {
		return (maxlon - minlon) / xpx;
	}
	/* for some y, what is the next pixel center below that? */
	inline int pcenterBelow(double somey) {
		double ty = maxlat - somey;
		return (int)floor( (ty / pixelHeight) + 0.5 );
	}
	/* for some x, what is the next pixel center to the right? */
	inline int pcenterRight(double somex) {
		double tx = somex - minlon;
		return (int)floor( (tx / pixelWidth) + 0.5 );
	}
	inline double pcenterY( int py ) {
		return maxlat - ((py + 0.5) * pixelHeight);
	}
	inline double pcenterX( int px ) {
		return minlon + ((px + 0.5) * pixelWidth);
	}

	int rasterizePointList( PointOutput* fout, uint64_t ubid, point* plist );
	int printPointList( PointOutput* fout, uint64_t ubid, point* plist );
	int pointListNOP( PointOutput* fout, uint64_t ubid, point* plist );
	void reconcileRTAPUChunks( rtaPolyUbid* rp, PointOutput* fout,
		int (PolyGroup::* processPointList)(PointOutput*,uint64_t,point*) );

	void updatePixelSize();
	void updatePixelSize2();

	PolyGroup();
	void clear();
};

class PointOutput {
public:
	virtual bool writePoint(uint64_t ubid, int32_t x, int32_t y) = 0;
	virtual bool flush() = 0;
	virtual bool close() = 0;
	virtual ~PointOutput();
};

template<class T>
class FILEPointOutput : public PointOutput {
private:
	FILE* fout;
	uint64_t lastUbid;
	bool firstout;
	void setEnd(T* end);
public:
	virtual bool writePoint(uint64_t ubid, int px, int py);
	virtual bool flush();
	virtual bool close();
	FILEPointOutput(FILE* out);
	virtual ~FILEPointOutput();
};
#include "FILEPointOutput.h"

// png logic copied from districter.h
#ifndef WITH_PNG
#if NOPNG
#define WITH_PNG 0
#else
#define WITH_PNG 1
#endif
#endif

#if WITH_PNG
void doMaskPNG( char* outname, uint8_t* px, int width, int height );
#else
inline void doMaskPNG( char* outname, uint8_t* px, int width, int height ){}
#endif

#endif /* RASTERIZE_TIGER_H */
