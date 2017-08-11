#ifndef GEODATA_H
#define GEODATA_H

#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"

// TODO: cleanup all the unused options and remove #if around the used ones.
class GeoData {
public:
	int fd;
	void* data;
	struct stat sb;
	size_t mmapsize;

	int32_t numPoints;
	int congressionalDistricts;

	// (x,y) meters or (lon,lat) integer microdegrees
	int32_t* pos;
	int minx, maxx, miny, maxy;

	// square meters
	uint64_t* area;

	int32_t* pop;
	int totalpop;
	int maxpop;
	// Ubid Sort Thing
	class UST {
public:
		uint64_t ubid;
		uint32_t index;
	};
	UST* ubids;
	/* binary search, fastish, return INVALID_INDEX on failure */
	uint32_t indexOfUbid( uint64_t u );
	/* linear search, sloooow */
	uint64_t ubidOfIndex( uint32_t index );
	/* allocate new uint64_t[] look-up-table, fill it, pass-out min and max index for it. */
	uint64_t* makeUbidLUT(uint32_t* minIndex, uint32_t* maxIndex);

	// Map "Logical Record Number" to internal index so that we can map in 
	// other Census data files.
	class RecnoNode {
	public:
		uint32_t recno;
		uint32_t index;
	};
	// May be NULL
	RecnoNode* recno_map;
	uint32_t* recnos;
	/* binary search, fastish. ((uint32_t)-1) on failure. */
	uint32_t indexOfRecno( uint32_t u );
	/* lookup table, fast! ((uint32_t)-1) on failure. */
	inline uint32_t recnoOfIndex( uint32_t index ) const {
		assert(index >= 0);
		assert(index < ((uint32_t)numPoints));
		return recnos[index];
	}

	// uint32_t[numPoints] {0:None, else place id}
	// Census 'place', filtered to be towns/cities/municipalities
	uint32_t* place;
	
	GeoData();
	int open( const char* inputname );
	virtual int load() = 0;
	virtual int numDistricts();
	//virtual uint64_t ubid( int index ) = 0;
	// get "Logical Record Number" which links to deeper census data.
	//virtual uint32_t logrecno( int index ) = 0;
	
	/** Binary Format:
	int32_t endianness; Should read as 1 in the correct endianness.
	                    May become a version number in the future.
	int32_t numPoints;
	int32_t position[numPoints*2];
	int32_t population[numPoints];
	uint32_t area[numPoints];
	(Combine to form UST. Stored sorted by ubid, index points into arrays above.
	uint32_t index[numPoints];
	uint64_t ubid[numPoints];
	)
	*/
	// deprecated, use protobufs
	int writeBin( int fd, const char* fname = ((const char*)0) );
	int readBin( int fd, const char* fname = ((const char*)0) );
	int writeBin( const char* fname );
	int readBin( const char* fname );
	int close();
	virtual ~GeoData();

	inline void allocPoints() {
		pos = new int32_t[numPoints*2];
	}
	inline int32_t lon(int x) const {
		return pos[(x*2)];
	}
	inline int32_t lat(int x) const {
		return pos[(x*2) + 1];
	}
	inline void set_lon(int x, int32_t microdegrees) {
		pos[(x*2)] = microdegrees;
		if (pos[(x*2)] < minx) minx = pos[(x*2)];
		if (pos[(x*2)] > maxx) maxx = pos[(x*2)];
	}
	inline void set_lon(int x, double degrees) {
		pos[(x*2)] = degrees * 1000000.0;
		if (pos[(x*2)] < minx) minx = pos[(x*2)];
		if (pos[(x*2)] > maxx) maxx = pos[(x*2)];
	}
	inline void set_lat(int x, int32_t microdegrees) {
		pos[(x*2) + 1] = microdegrees;
		if (pos[(x*2) + 1] < miny) miny = pos[(x*2) + 1];
		if (pos[(x*2) + 1] > maxy) maxy = pos[(x*2) + 1];
	}
	inline void set_lat(int x, double degrees) {
		pos[(x*2) + 1] = degrees * 1000000.0;
		if (pos[(x*2) + 1] < miny) miny = pos[(x*2) + 1];
		if (pos[(x*2) + 1] > maxy) maxy = pos[(x*2) + 1];
	}
};

static const uint32_t INVALID_INDEX = ((uint32_t)-1);

#if 0
// deprecated
class ZCTA : public GeoData {
	virtual int load();
	//virtual int numDistricts();
};
#endif

#define sizeof_GeoUf1 402
#define copyGeoUf1Field( dst, src, i, start, stop ) memcpy( dst, (void*)((unsigned long)src + i*sizeof_GeoUf1 + start ), stop - start ); ((unsigned char*)dst)[stop-start] = '\0';

class Uf1 : public GeoData {
public:
	virtual int load();
	virtual int numDistricts();

	virtual POPTYPE oldDist( int index );

private:	
	// Get Unique Block ID, the ull interpretation of concatenated
	// decimal digits {state, county, tract, block}: SSCCCTTTTTTBBBB
	// Ubid links to geometry data which tags blocks by this string.
	// This is eqivalent to Cenus 2000 tiger shapefile BLOCKID.
	// TODO: 2010 shapefile has an extra digit appended.
	uint64_t ubid( int index );
	
	// get "Logical Record Number" which links to deeper census data.
	uint32_t logrecno( int index );
	
	int countDistricts();
};

class PlGeo : public GeoData {
public:
	PlGeo();
	virtual ~PlGeo();

	virtual int load();
	virtual int numDistricts();
	
	// Get Unique Block ID, the ull interpretation of concatenated
	// decimal digits {state, county, tract, block}: SSCCCTTTTTTBBBB
	// Ubid links to geometry data which tags blocks by this string.
	// This is eqivalent to Cenus 2000 tiger shapefile BLOCKID.
	// TODO: 2010 shapefile has an extra digit appended.
	//virtual uint64_t ubid( int index );
	
	// get "Logical Record Number" which links to deeper census data.
	//virtual uint32_t logrecno( int index );
		
	POPTYPE oldDist( int index );
};


inline uint32_t ubidBlock(uint64_t ubid) {
    //  SSCCCTTTTTTBBBB
    //             9999
    return ubid % 10000;
}
inline uint32_t ubidTract(uint64_t ubid) {
    //  SSCCCTTTTTTBBBB
    //       999999
    return (ubid / 10000) % 1000000;
}
inline uint32_t ubidCounty(uint64_t ubid) {
    return (ubid / 10000000000) % 1000;
}
inline uint32_t ubidState(uint64_t ubid) {
    return (ubid / 10000000000000) % 100;
}

//GeoData* openZCTA( const char* inputname );
GeoData* openUf1( const char* inputname );
GeoData* openPlGeo( const char* inputname );
GeoData* protobufGeoDataTag( const char* inputname );

#endif /* GEODATA_H */
