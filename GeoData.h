#ifndef GEODATA_H
#define GEODATA_H

// TODO: cleanup all the unused options and remove #if around the used ones.
class GeoData {
public:
	int fd;
	void* data;
	struct stat sb;
	size_t mmapsize;

	int32_t numPoints;
#ifndef COUNT_DISTRICTS
#define COUNT_DISTRICTS 1
#endif
#if COUNT_DISTRICTS
	int congressionalDistricts;
#endif

#ifndef READ_INT_POS
#define READ_INT_POS 1
#endif
#if READ_INT_POS
	int32_t* pos;
	int minx, maxx, miny, maxy;
#endif
#ifndef READ_DOUBLE_POS
#define READ_DOUBLE_POS 0
#endif
#if READ_DOUBLE_POS
	double* pos;
	double minx, maxx, miny, maxy;
#endif

#ifndef READ_INT_AREA
#define READ_INT_AREA 1
#endif
#if READ_INT_AREA
	uint32_t* area;
#endif

#ifndef READ_INT_POP
#define READ_INT_POP 1
#endif
#if READ_INT_POP
	int32_t* pop;
	int totalpop;
	int maxpop;
#endif
#ifndef READ_UBIDS
#define READ_UBIDS 1
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
#ifndef READ_RECNOS
// TODO: not done yet, implement recno merging of other data
#define READ_RECNOS 0
#endif
#if READ_RECNOS
	// Map "Logical Record Number" to internal index so that we can map in 
	// other Census data files.
	class RecnoNode {
	public:
		uint32_t recno;
		uint32_t index;
	};
	RecnoNode* recnos;
	/* binary search, fastish */
	uint32_t indexOfRecno( uint32_t u );
	/* linear search, sloooow */
	uint32_t recnoOfIndex( uint32_t index );
#endif
	
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
public:
	POPTYPE oldDist( int index );
};


class GeoBin : public GeoData {
	virtual int load();
	//virtual int numDistricts();
};


/*int readZCTA( char* inputname, PosPop** zipdP );
int readGeoUf1( char* inputname, PosPop** zipdP );*/
GeoData* openZCTA( char* inputname );
GeoData* openUf1( char* inputname );
GeoData* openBin( char* inputname );

#endif /* GEODATA_H */
