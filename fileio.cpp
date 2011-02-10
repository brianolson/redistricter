#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include "place2k.h"
#include "Node.h"

#include "swap.h"
#include "GeoData.h"

#if 01
//extern FILE* blaf;
#elif __GNUC__
extern FILE* blaf __attribute__ ((weak));
#else
#pragma weak blaf
extern FILE* blaf;
#endif

int GeoData::open( char* inputname ) {
	int err;
	
	fd = ::open( inputname, O_RDONLY );
	if ( fd <= 0 ) {
		perror(inputname);
		return -1;
	}
	err = fstat( fd, &sb );
	if ( err == -1 ) {
		perror("fstat");
		return -1;
	}
	{
		int pagesize = getpagesize();
		mmapsize = (sb.st_size + pagesize - 1) & (~(pagesize - 1));
		//printf("mmap( NULL, mmapsize = 0x%x, PROT_READ, MAP_FILE, fd=%d, 0 );\n", mmapsize, fd );
		data = mmap( NULL, mmapsize, PROT_READ, MAP_SHARED|MAP_FILE, fd, 0 );
	}
	if ( (signed long)data == -1 ) {
		perror("mmap");
		return -1;
	}
	return 0;
}

int GeoData::close() {
	if ( data != NULL && fd >= 0 ) {
		int err;
		err = munmap( data, mmapsize );
		data = NULL;
		sb.st_size = 0;
		mmapsize = 0;
		err |= ::close( fd );
		fd = -1;
		return err;
	} else {
		return 0;
	}
}

GeoData::GeoData()
	: fd( -1 ), data( (void*)0 ), mmapsize( 0 ), recnos( NULL )
{
}

GeoData::~GeoData() {
	if ( data != NULL && fd >= 0 ) {
		close();
	}
}

#if 0
void bswapBuf32( void* buf, size_t s ) {
	uintptr_t p = (uintptr_t)buf;
	uintptr_t pos = 0;
	while ( pos + 4 < s ) {
		uint32_t* b = (uint32_t*)(p + pos);
		*b = swap32( *b );
		pos += 4;
	}
}
#endif

void UST_ecpy( GeoData::UST* dst, GeoData::UST* src ) {
	dst->index = swap32( src->index );
	dst->ubid = swap64( src->ubid );
}

int GeoData::writeBin( const char* fname ) {
	int fd, err;
	fd = ::open( fname, O_WRONLY|O_CREAT, 0666 );
	if ( fd < 0 ) {
		perror(fname);
		return -1;
	}
	err = writeBin( fd, fname );
	if ( err != 0 ) {
		::close(fd);
		return err;
	}
	return ::close( fd );
}
int GeoData::writeBin( int fd, const char* fname ) {
	int err;
	int s;
	{
		int32_t endianness = 1;
		err = write( fd, &endianness, sizeof(endianness) );
		if ( err != sizeof(endianness) ) {
			fprintf(stderr,"%s: writing endianness err=%d \"%s\"\n", fname, err, strerror(errno) );
			//::close( fd );
			return -1;
		}
	}
	err = write( fd, &numPoints, sizeof(numPoints) );
	if ( err != sizeof(numPoints) ) {
		fprintf(stderr,"%s: writing numPoints err=%d \"%s\"\n", fname, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	s = sizeof(*pos) * numPoints * 2;
	err = write( fd, pos, s );
	if ( err != s ) {
		fprintf(stderr,"%s: pos size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	s = sizeof(*pop) * numPoints;
	err = write( fd, pop, s );
	if ( err != s ) {
		fprintf(stderr,"%s: pop size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	s = sizeof(*area) * numPoints;
	err = write( fd, area, s );
	if ( err != s ) {
		fprintf(stderr,"%s: area size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	{
		void* buf = malloc( sizeof(uint64_t)*numPoints );
		uint32_t* ib = (uint32_t*)buf;
		uint64_t* sb = (uint64_t*)buf;
		for ( int i = 0; i < numPoints; i++ ) {
			ib[i] = ubids[i].index;
		}
		s = sizeof(uint32_t)*numPoints;
		err = write( fd, ib, s );
		if ( err != s ) {
			fprintf(stderr,"%s: ubids.index size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
			::close( fd );
			return -1;
		}
		for ( int i = 0; i < numPoints; i++ ) {
			sb[i] = ubids[i].ubid;
		}
		s = sizeof(uint64_t)*numPoints;
		err = write( fd, sb, s );
		if ( err != s ) {
			fprintf(stderr,"%s: ubids.ubid size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
			::close( fd );
			return -1;
		}
		free( buf );
	}
	return 0;
}
int GeoData::readBin( const char* fname ) {
	int fd, err;
	fd = ::open( fname, O_RDONLY );
	if ( fd < 0 ) {
		perror(fname);
		return -1;
	}
	err = readBin( fd, fname );
	if ( err != 0 ) {
		::close(fd);
		return err;
	}
	return ::close( fd );
}
int GeoData::readBin( int fd, const char* fname ) {
	int err;
	int s;
	int32_t endianness = 1;
	err = ::read( fd, &endianness, sizeof(endianness) );
	if ( err != sizeof(endianness) ) {
		fprintf(stderr,"%s: reading endianness err=%d \"%s\"\n", fname, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	/* if bin file was written in other endianness, 1 will be at wrong end. */
	endianness = ! ( endianness & 0xff );
	if ( endianness ) {
		fprintf(stderr,"%s: wrong endianness. FIXME write read swapping in readBin\n", fname );
	}
	err = ::read( fd, &numPoints, sizeof(numPoints) );
	if ( err != sizeof(numPoints) ) {
		fprintf(stderr,"%s: reading numPoints err=%d \"%s\"\n", fname, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	if ( endianness ) {
		numPoints = swap32( numPoints );
	}
	pos = new int[numPoints*2];
	maxx = maxy = INT_MIN;
	minx = miny = INT_MAX;
	s = sizeof(*pos) * numPoints * 2;
	err = ::read( fd, pos, s );
	if ( err != s ) {
		fprintf(stderr,"%s: reading pos size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	if ( endianness ) {
		for ( int i = 0; i < numPoints; i++ ) {
		pos[i*2] = swap32( pos[i*2] );
		pos[i*2 + 1] = swap32( pos[i*2 + 1] );
		}
	}
	for ( int i = 0; i < numPoints; i++ ) {
		if ( pos[i*2  ] < minx ) {
			minx = pos[i*2  ];
		}
		if ( pos[i*2  ] > maxx ) {
			maxx = pos[i*2  ];
		}
		if ( pos[i*2+1] < miny ) {
			miny = pos[i*2+1];
		}
		if ( pos[i*2+1] > maxy ) {
			maxy = pos[i*2+1];
		}
	}
	pop = new int[numPoints];
	totalpop = 0;
	s = sizeof(*pop) * numPoints;
	err = ::read( fd, pop, s );
	if ( err != s ) {
		fprintf(stderr,"%s: reading pop size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	for ( int i = 0; i < numPoints; i++ ) {
		if ( endianness ) {
			pop[i] = swap32( pop[i] );
		}
		totalpop += pop[i];
	}
	area = new uint64_t[numPoints];
	s = sizeof(*area) * numPoints;
	err = ::read( fd, area, s );
	if ( err != s ) {
		fprintf(stderr,"%s: reading area size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	if ( endianness ) {
		for ( int i = 0; i < numPoints; i++ ) {
		  // TODO: ERROR 64 bit! Scrap this and only use protobuf
			area[i] = swap32( area[i] );
		}
	}
	ubids = new UST[numPoints];
	{
		void* buf = malloc( sizeof(uint64_t)*numPoints );
		uint32_t* ib = (uint32_t*)buf;
		uint64_t* sb = (uint64_t*)buf;
		s = sizeof(uint32_t)*numPoints;
		err = ::read( fd, ib, s );
		if ( err != s ) {
			fprintf(stderr,"%s: reading ubids.index size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
			//::close( fd );
			return -1;
		}
		if ( endianness ) {
			for ( int i = 0; i < numPoints; i++ ) {
				ubids[i].index = swap32( ib[i] );
			}
		} else {
			for ( int i = 0; i < numPoints; i++ ) {
				ubids[i].index = ib[i];
			}
		}
		s = sizeof(uint64_t)*numPoints;
		err = ::read( fd, sb, s );
		if ( err != s ) {
			fprintf(stderr,"%s: reading ubids.index size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
			//::close( fd );
			return -1;
		}
		if ( endianness ) {
			for ( int i = 0; i < numPoints; i++ ) {
				ubids[i].ubid = swap64( sb[i] );
			}
		} else {
			for ( int i = 0; i < numPoints; i++ ) {
				ubids[i].ubid = sb[i];
			}
		}
		free( buf );
	}
	return 0;
}

#ifndef MAX_DISTRICTS
#define MAX_DISTRICTS 100
#endif

#if 0
int ZCTA::load() {
	return -1;
}
#endif

// Sort by ubid to aid lookup mapping ubid->internal-index
int ubidSortF( const void* a, const void* b ) {
	if ( ((Uf1::UST*)a)->ubid > ((Uf1::UST*)b)->ubid ) {
		return 1;
	} else if ( ((Uf1::UST*)a)->ubid < ((Uf1::UST*)b)->ubid ) {
		return -1;
	} else {
		return 0;
	}
}

// Sort by recno to aid lookup mapping recno->internal-index
int recnoSortF( const void* a, const void* b ) {
	if ( ((Uf1::RecnoNode*)a)->recno > ((Uf1::RecnoNode*)b)->recno ) {
		return 1;
	} else if ( ((Uf1::RecnoNode*)a)->recno < ((Uf1::RecnoNode*)b)->recno ) {
		return -1;
	} else {
		return 0;
	}
}

static inline int countADistrict(void* data, int i, char** dsts, int num) {
	// 108th cong at position 138. 106th at 136.
	char* cd = (char*)((unsigned long)data + i*sizeof_GeoUf1 + 138 );
	bool match = false;
	for ( int j = 0; j < num; j++ ) {
		if ( (cd[0] == dsts[j][0]) && (cd[1] == dsts[j][1]) ) {
			match = true;
			break;
		}
	}
	if ( ! match ) {
		dsts[num] = cd;
		num++;
	}
	return num;
}

PlGeo::PlGeo() {
}
PlGeo::~PlGeo() {
}
/*
Census 2010 PL94-171 (redistricting) geography
http://www.census.gov/prod/cen2010/pl94-171.pdf

0,6 "PLST  "
6,8	state postal code
8,11	summary level
18,25	logical record number
27,29	state (fips)
29,32	county
54,60	cenus tract
60,61	block group
61,65	block
198,212	area land
212,226	area water
318,327	population
336,347	latitude
347,359	longitude

len=501
 */
static const unsigned int sizeof_pl_line = 501;
#define copyPlGeoField( dst, src, i, start, stop ) memcpy( dst, (void*)((unsigned long)src + i*sizeof_pl_line + start ), stop - start ); ((unsigned char*)dst)[stop-start] = '\0';
static inline void PL_UBID_CHAR(uint64_t& tubid, char c) {
	assert(c >= '0');
	assert(c <= '9');
	tubid = (tubid * 10) + (c - '0');
}

int PlGeo::load() {
	assert(0 == memcmp(data, "PLST  ", 6));
	// For each point, read lat,lon,area,population,ubid
	int i;
	char buf[128];
	char* endp;

	numPoints = sb.st_size / sizeof_pl_line;
	pos = new int[numPoints*2];
	maxx = maxy = INT_MIN;
	minx = miny = INT_MAX;
	area = new uint64_t[numPoints];
	pop = new int[numPoints];
	totalpop = 0;
	maxpop = 0;
	ubids = new UST[numPoints];
	recno_map = new RecnoNode[numPoints];
	recnos = new uint32_t[numPoints];
	for ( i = 0; i < numPoints; i++ ) {
		char* line = reinterpret_cast<char*>(
			reinterpret_cast<uintptr_t>(data) + (i * sizeof_pl_line));
		// longitude, "x"
		copyPlGeoField( buf, data, i, 347, 359 );
		pos[i*2  ] = strtod( buf, &endp ) * 1000000;
		assert( endp != buf );
		if ( pos[i*2  ] > maxx ) {
			maxx = pos[i*2  ];
		}
		if ( pos[i*2  ] < minx ) {
			minx = pos[i*2  ];
		}
		// latitude, "y"
		copyPlGeoField( buf, data, i, 336, 347 );
		pos[i*2+1] = strtod( buf, &endp ) * 1000000;
		assert( endp != buf );
		if ( pos[i*2+1] > maxy ) {
			maxy = pos[i*2+1];
		}
		if ( pos[i*2+1] < miny ) {
			miny = pos[i*2+1];
		}

		// area land
		copyPlGeoField( buf, data, i, 198, 212 );
		{
			errno = 0;
			area[i] = strtoull( buf, &endp, 10 );
			if (errno != 0) {
				perror("strtoull");
				fprintf(stderr, "failed to parse \"%s\" => %llu\n", buf, area[i]);
				assert( errno == 0 );
			}
			assert( endp != buf );
		}

		// population
		copyPlGeoField( buf, data, i, 318, 327 );
		pop[i] = strtoul( buf, &endp, 10 );
		assert( endp != buf );
		totalpop += pop[i];
		if ( pop[i] > maxpop ) {
			maxpop = pop[i];
		}

		// ubid
		{
			//unsigned int offset = 0;
			uint64_t tubid = 0;
			// state county tract blkgrp block
			PL_UBID_CHAR(tubid, line[27]); // state
			PL_UBID_CHAR(tubid, line[28]); // 
			PL_UBID_CHAR(tubid, line[29]); // county
			PL_UBID_CHAR(tubid, line[30]); // 
			PL_UBID_CHAR(tubid, line[31]); // 
			PL_UBID_CHAR(tubid, line[54]); // tract
			PL_UBID_CHAR(tubid, line[55]); // 
			PL_UBID_CHAR(tubid, line[56]); // 
			PL_UBID_CHAR(tubid, line[57]); // 
			PL_UBID_CHAR(tubid, line[58]); // 
			PL_UBID_CHAR(tubid, line[59]); // 
			//PL_UBID_CHAR(tubid, line[60]); // block group
			PL_UBID_CHAR(tubid, line[61]); // block
			PL_UBID_CHAR(tubid, line[62]); // 
			PL_UBID_CHAR(tubid, line[63]); // 
			PL_UBID_CHAR(tubid, line[64]); // 
			//printf("tubid=%lu i=%d\n", tubid, i);
			ubids[i].ubid = tubid;
			ubids[i].index = i;
		}

		// logrecno
		copyPlGeoField( buf, data, i, 18, 25 );
		recno_map[i].recno = strtoul( buf, NULL, 10 );
		recno_map[i].index = i;
		recnos[i] = recno_map[i].recno;
	}

	qsort( ubids, numPoints, sizeof( UST ), ubidSortF );
	for ( i = 0; i < 10; i++ ) {
		fprintf( stderr, "%lld\t%d\n", ubids[i].ubid, ubids[i].index );
	}
	printf("PlGeo::load() minx %d, miny %d, maxx %d, maxy %d\n", minx, miny, maxx, maxy );
	if (recnos != NULL) {
		qsort( recno_map, numPoints, sizeof(RecnoNode), recnoSortF );
	}
	return 0;
}
int PlGeo::numDistricts() {
	return 0;
}
#if 0
uint64_t PlGeo::ubid( int index ) {
	return 0;
}
uint32_t PlGeo::logrecno( int index ) {
	return 0;
}
#endif
/*
 Census 2000 Summary File 1 geography
http://www.census.gov/prod/cen2000/doc/sf1.pdf

0,6	"uSF1  "
6,8	state postal code
8,11	summary level
18,25	logical record number
27,29	state (census)
29,31	state (fips)
31,34	county
55,61	cenus tract
61,62	block group
62,66	block
len=402
 */
int Uf1::load() {
	int i;
	char buf[128];
	char* endp;
		
	numPoints = sb.st_size / sizeof_GeoUf1;
#if 0
	if ( blaf != NULL ) {
		fprintf( blaf, "there are %d blocks\n", numPoints );
	}
#endif
	
	pos = new int[numPoints*2];
	maxx = maxy = INT_MIN;
	minx = miny = INT_MAX;
	area = new uint64_t[numPoints];
	pop = new int[numPoints];
	totalpop = 0;
	maxpop = 0;
	ubids = new UST[numPoints];
#if COUNT_DISTRICTS
	char* dsts[MAX_DISTRICTS];
	congressionalDistricts = 0;
#endif
	if (true) {
		recno_map = new RecnoNode[numPoints];
		recnos = new uint32_t[numPoints];
	}

	for ( i = 0; i < numPoints; i++ ) {
		// longitude, "x"
		copyGeoUf1Field( buf, data, i, 319, 331 );
		pos[i*2  ] = strtol( buf, &endp, 10 );
		assert( endp != buf );
		if ( pos[i*2  ] > maxx ) {
			maxx = pos[i*2  ];
		}
		if ( pos[i*2  ] < minx ) {
			minx = pos[i*2  ];
		}
		// latitude, "y"
		copyGeoUf1Field( buf, data, i, 310, 319 );
		pos[i*2+1] = strtol( buf, &endp, 10 );
		assert( endp != buf );
		if ( pos[i*2+1] > maxy ) {
			maxy = pos[i*2+1];
		}
		if ( pos[i*2+1] < miny ) {
			miny = pos[i*2+1];
		}

		copyGeoUf1Field( buf, data, i, 172, 186 );
		{
			errno = 0;
			area[i] = strtoull( buf, &endp, 10 );
			if (errno != 0) {
				perror("strtoull");
				fprintf(stderr, "failed to parse \"%s\" => %llu\n", buf, area[i]);
				assert( errno == 0 );
			}
			assert( endp != buf );
		}

		// population
		copyGeoUf1Field( buf, data, i, 292, 301 );
		pop[i] = atoi( buf );
		totalpop += pop[i];
		if ( pop[i] > maxpop ) {
			maxpop = pop[i];
		}
		ubids[i].ubid = ubid( i );
		ubids[i].index = i;
		if (recnos != NULL) {
			recno_map[i].recno = logrecno( i );
			recno_map[i].index = i;
			recnos[i] = recno_map[i].recno;
		}
#if COUNT_DISTRICTS
		congressionalDistricts = countADistrict(data, i, dsts, congressionalDistricts);
#endif
	}
	qsort( ubids, numPoints, sizeof( UST ), ubidSortF );
	for ( i = 0; i < 10; i++ ) {
		fprintf( stderr, "%lld\t%d\n", ubids[i].ubid, ubids[i].index );
	}
	printf("Uf1::load() minx %d, miny %d, maxx %d, maxy %d\n", minx, miny, maxx, maxy );
	if (recnos != NULL) {
		qsort( recno_map, numPoints, sizeof(RecnoNode), recnoSortF );
	}
	return 0;
}
uint64_t Uf1::ubid( int index ) {
	char buf[2+3+6+1+4+1];
	char* line = reinterpret_cast<char*>(
		reinterpret_cast<uintptr_t>(data) + (index * sizeof_GeoUf1));
	unsigned int offset = 0;
#define UF1_UBID_CHAR(pos) buf[offset] = line[pos]; offset++;
	UF1_UBID_CHAR(29); // state
	UF1_UBID_CHAR(30); // state
	UF1_UBID_CHAR(31); // county
	UF1_UBID_CHAR(32); // county
	UF1_UBID_CHAR(33); // county
	UF1_UBID_CHAR(55); // tract
	UF1_UBID_CHAR(56); // tract
	UF1_UBID_CHAR(57); // tract
	UF1_UBID_CHAR(58); // tract
	UF1_UBID_CHAR(59); // tract
	UF1_UBID_CHAR(60); // tract
	UF1_UBID_CHAR(62); // block
	UF1_UBID_CHAR(63); // block
	UF1_UBID_CHAR(64); // block
	UF1_UBID_CHAR(65); // block
	// TODO: 2010 data may add one char block suffix
	assert(offset < sizeof(buf));
	buf[offset] = '\0';
#undef UF1_UBID_CHAR
	char* endp;
	uint64_t out = strtoull( buf, &endp, 10 );
	if (endp != (buf + offset)) {
		fprintf(stderr, "bogus ubid \"%s\" at index %d\n", buf, index);
		assert(endp == (buf + offset));
		return (uint64_t)-1;
	}
	return out;
}
// get "Logical Record Number" which links to deeper census data.
uint32_t Uf1::logrecno( int index ) {
	char buf[8];
	copyGeoUf1Field( buf, data, index, 18, 25 );
	return strtoul( buf, NULL, 10 );
}
POPTYPE Uf1::oldDist( int index ) {
		char buf[3];
		char* bp;
		POPTYPE toret;
		bp = buf;
		//copyGeoUf1Field( buf, data, index, 136, 138 ); // cd106
		copyGeoUf1Field( buf, data, index, 138, 140 ); // cd108
		toret = strtol( buf, &bp, 10 ) - 1;
		if ( bp !=  NULL && bp != buf ) {
			return toret;
		}
		return ERRDISTRICT;
	}

#if 0
GeoData* openZCTA( char* inputname ) {
	ZCTA* toret = new ZCTA();
	toret->open( inputname );
	return toret;
}
#endif
GeoData* openUf1( char* inputname ) {
	Uf1* toret = new Uf1();
	toret->open( inputname );
	return toret;
}
GeoData* openPlGeo( char* inputname ) {
	PlGeo* toret = new PlGeo();
	toret->open( inputname );
	return toret;
}

int GeoData::numDistricts() {
#if COUNT_DISTRICTS
	return congressionalDistricts;
#else
	return -1;
#endif
}

uint32_t GeoData::indexOfUbid( uint64_t u ) {
	// binary search of sorted ubid sort things
	int lo = 0;
	int hi = numPoints - 1;
	int mid;
	uint64_t t;
	while ( hi >= lo ) {
		mid = (hi + lo) / 2;
		t = ubids[mid].ubid;
		if ( t > u ) {
			hi = mid - 1;
		} else if ( t < u ) {
			lo = mid + 1;
		} else {
			assert(ubids[mid].index >= 0);
			assert(ubids[mid].index < ((uint32_t)numPoints));
			return ubids[mid].index;
		}
	}
	return (uint32_t)-1;
}
uint64_t GeoData::ubidOfIndex( uint32_t index ) {
	for ( int i = 0; i < numPoints; i++ ) {
		if ( ubids[i].index == index ) {
			return ubids[i].ubid;
		}
	}
	return (uint64_t)-1;
}

/* binary search, fastish */
uint32_t GeoData::indexOfRecno( uint32_t rn ) {
	if (recno_map == NULL) {
		return (uint32_t)-1;
	}
	int lo = 0;
	int hi = numPoints - 1;
	int mid;
	uint32_t t;
	while ( hi >= lo ) {
		mid = (hi + lo) / 2;
		t = recno_map[mid].recno;
		if ( t > rn ) {
			hi = mid - 1;
		} else if ( t < rn ) {
			lo = mid + 1;
		} else {
			assert(recno_map[mid].index >= 0);
			assert(recno_map[mid].index < ((uint32_t)numPoints));
			return recno_map[mid].index;
		}
	}
	return (uint32_t)-1;
}

int Uf1::countDistricts() {
	if (data == NULL) {
		return -1;
	}
	int num = 0;
	char** dsts = new char*[MAX_DISTRICTS];
	for ( int i = 0; i < numPoints; i++ ) {
		num = countADistrict(data, i, dsts, num);
	}
	delete [] dsts;
	if ( num > 1 ) {
		return num;
	}
	return -1;
}

int Uf1::numDistricts() {
#if COUNT_DISTRICTS
	return congressionalDistricts;
#else
	return countDistricts();
#endif
}
