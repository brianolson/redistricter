#ifndef GEODATA_H
#define GEODATA_H

#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>

#include "config.h"

// TODO: cleanup all the unused options and remove #if around the used ones.
class GeoData {
public:
	int fd;
	void* data;
	struct stat sb;
	size_t mmapsize;

	int32_t numPoints;
#if COUNT_DISTRICTS
	int congressionalDistricts;
#endif

#if READ_INT_POS
	int32_t* pos;
	int minx, maxx, miny, maxy;

	inline void allocPoints() {
		pos = new int32_t[numPoints*2];
	}
	inline int32_t lon(int x) {
		return pos[(x*2)];
	}
	inline int32_t lat(int x) {
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
#endif
#if READ_DOUBLE_POS
	double* pos;
	double minx, maxx, miny, maxy;

	inline void allocPoints() {
		pos = new double[numPoints*2];
	}
	inline double lon(int x) {
		return pos[(x*2)];
	}
	inline double lat(int x) {
		return pos[(x*2) + 1];
	}
	inline void set_lon(int x, int32_t microdegrees) {
		pos[(x*2)] = microdegrees / 1000000.0;
		if (pos[(x*2)] < minx) minx = pos[(x*2)];
		if (pos[(x*2)] > maxx) maxx = pos[(x*2)];
	}
	inline void set_lon(int x, double degrees) {
		pos[(x*2)] = degrees;
		if (pos[(x*2)] < minx) minx = pos[(x*2)];
		if (pos[(x*2)] > maxx) maxx = pos[(x*2)];
	}
	inline void set_lat(int x, int32_t microdegrees) {
		pos[(x*2) + 1] = microdegrees / 1000000.0;
		if (pos[(x*2) + 1] < miny) miny = pos[(x*2) + 1];
		if (pos[(x*2) + 1] > maxy) maxy = pos[(x*2) + 1];
	}
	inline void set_lat(int x, double degrees) {
		pos[(x*2) + 1] = degrees;
		if (pos[(x*2) + 1] < miny) miny = pos[(x*2) + 1];
		if (pos[(x*2) + 1] > maxy) maxy = pos[(x*2) + 1];
	}
#endif

#if READ_INT_AREA
	uint32_t* area;
#endif

#if READ_INT_POP
	int32_t* pop;
	int totalpop;
	int maxpop;
#endif
#if READ_UBIDS
	// Ubid Sort Thing
	class UST {
public:
		uint64_t ubid;
		uint32_t index;
	};
	UST* ubids;
	/* binary search, fastish */
	uint32_t indexOfUbid( uint64_t u );
	/* linear search, sloooow */
	uint64_t ubidOfIndex( uint32_t index );
#endif

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
	uint32_t recnoOfIndex( uint32_t index ) {
		assert(index >= 0);
		assert(index < ((uint32_t)numPoints));
		return recnos[index];
	}
	
	GeoData();
	int open( char* inputname );
	virtual int load() = 0;
	virtual int numDistricts();
	
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
	int writeBin( int fd, const char* fname = NULL );
	int readBin( int fd, const char* fname = NULL );
	int writeBin( const char* fname );
	int readBin( const char* fname );
	int close();
	virtual ~GeoData();
};

class ZCTA : public GeoData {
	virtual int load();
	//virtual int numDistricts();
};

#define sizeof_GeoUf1 402
#define copyGeoUf1Field( dst, src, i, start, stop ) memcpy( dst, (void*)((unsigned long)src + i*sizeof_GeoUf1 + start ), stop - start ); ((unsigned char*)dst)[stop-start] = '\0';

class Uf1 : public GeoData {
	virtual int load();
	virtual int numDistricts();
	
	// get Unique Block ID, the ull interpretation of county tract and block concatenated
	uint64_t ubid( int index );
	
	// get "Logical Record Number" which links to deeper census data.
	uint32_t logrecno( int index );
	
	int countDistricts();

public:
	POPTYPE oldDist( int index );
};


class GeoBin : public GeoData {
	virtual int load();
	//virtual int numDistricts();
};


GeoData* openZCTA( char* inputname );
GeoData* openUf1( char* inputname );
GeoData* openBin( char* inputname );
GeoData* protobufGeoDataTag( char* inputname );

#endif /* GEODATA_H */
