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
#include "districter.h"
#include "Node.h"

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
		exit(1);
	}
	err = fstat( fd, &sb );
	if ( err == -1 ) {
		perror("fstat");
		exit(1);
	}
	{
		int pagesize = getpagesize();
		mmapsize = (sb.st_size + pagesize - 1) & (~(pagesize - 1));
		//printf("mmap( NULL, mmapsize = 0x%x, PROT_READ, MAP_FILE, fd=%d, 0 );\n", mmapsize, fd );
		data = mmap( NULL, mmapsize, PROT_READ, MAP_SHARED|MAP_FILE, fd, 0 );
	}
	if ( (signed long)data == -1 ) {
		perror("mmap");
		exit(1);
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
	: fd( -1 ), data( (void*)0 ), mmapsize( 0 )
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
	fd = ::open( fname, O_WRONLY|O_CREAT );
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
#if READ_INT_POP
	s = sizeof(*pop) * numPoints;
	err = write( fd, pop, s );
	if ( err != s ) {
		fprintf(stderr,"%s: pop size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
#endif
#if READ_INT_AREA
	s = sizeof(*area) * numPoints;
	err = write( fd, area, s );
	if ( err != s ) {
		fprintf(stderr,"%s: area size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
#endif
#if READ_UBIDS
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
#endif
	return 0;
}
int GeoData::readBin( const char* fname ) {
	int fd, err;
	fd = ::open( fname, O_RDONLY|O_CREAT );
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
#if READ_INT_POS
	pos = new int[numPoints*2];
	maxx = maxy = INT_MIN;
	minx = miny = INT_MAX;
#endif
#if READ_DOUBLE_POS
	pos = new double[numPoints*2];
#endif
	s = sizeof(*pos) * numPoints * 2;
	err = ::read( fd, pos, s );
	if ( err != s ) {
		fprintf(stderr,"%s: reading pos size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	if ( endianness ) {
	    for ( int i = 0; i < numPoints; i++ ) {
#if READ_INT_POS
		pos[i*2] = swap32( pos[i*2] );
		pos[i*2 + 1] = swap32( pos[i*2 + 1] );
#endif
#if READ_DOUBLE_POS
		((uint64_t*)pos)[i*2] = swap64( ((uint64_t*)pos)[i*2] );
		((uint64_t*)pos)[i*2 + 1] = swap64( ((uint64_t*)pos)[i*2 + 1] );
#endif
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
#if READ_INT_POP
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
#endif
#if READ_INT_AREA
	area = new uint32_t[numPoints];
	s = sizeof(*area) * numPoints;
	err = ::read( fd, area, s );
	if ( err != s ) {
		fprintf(stderr,"%s: reading area size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	if ( endianness ) {
	    for ( int i = 0; i < numPoints; i++ ) {
		area[i] = swap32( area[i] );
	    }
	}
#endif
#if READ_UBIDS
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
#endif
	return 0;
}

#ifndef MAX_DISTRICTS
#define MAX_DISTRICTS 100
#endif

int ZCTA::load() {
	return -1;
}

// Sort by ubid to aid lookup mapping ubid->internal-index
static int ubidSortF( const void* a, const void* b ) {
	if ( ((Uf1::UST*)a)->ubid > ((Uf1::UST*)b)->ubid ) {
		return 1;
	} else if ( ((Uf1::UST*)a)->ubid < ((Uf1::UST*)b)->ubid ) {
		return -1;
	} else {
		return 0;
	}
}

int Uf1::load() {
	int i;
	char buf[128];
		
	numPoints = sb.st_size / sizeof_GeoUf1;
#if 0
	if ( blaf != NULL ) {
		fprintf( blaf, "there are %d blocks\n", numPoints );
	}
#endif
	
#if READ_INT_POS
	pos = new int[numPoints*2];
	maxx = maxy = INT_MIN;
	minx = miny = INT_MAX;
#endif
#if READ_DOUBLE_POS
	pos = new double[numPoints*2];
#endif
#if READ_INT_AREA
	area = new uint32_t[numPoints];
#endif
#if READ_INT_POP
	pop = new int[numPoints];
	totalpop = 0;
	maxpop = 0;
#endif
#if READ_UBIDS
	ubids = new UST[numPoints];
#endif
#if COUNT_DISTRICTS
	char* dsts[MAX_DISTRICTS];
	congressionalDistricts = 0;
#endif

	for ( i = 0; i < numPoints; i++ ) {
#if READ_DOUBLE_POS || READ_INT_POS
		// longitude, "x"
		copyGeoUf1Field( buf, data, i, 319, 331 );
#if READ_DOUBLE_POS
		pos[i*2  ] = atof( buf ) / 1000000.0;
#endif
#if READ_INT_POS
		pos[i*2  ] = strtol( buf, NULL, 10 );
#endif
		if ( pos[i*2  ] > maxx ) {
			maxx = pos[i*2  ];
		}
		if ( pos[i*2  ] < minx ) {
			minx = pos[i*2  ];
		}
		// latitude, "y"
		copyGeoUf1Field( buf, data, i, 310, 319 );
#if READ_DOUBLE_POS
		pos[i*2+1] = atof( buf ) / 1000000.0;
#endif
#if READ_INT_POS
		pos[i*2+1] = strtol( buf, NULL, 10 );
#endif
		if ( pos[i*2+1] > maxy ) {
			maxy = pos[i*2+1];
		}
		if ( pos[i*2+1] < miny ) {
			miny = pos[i*2+1];
		}
#endif /* read pos */

#if READ_INT_AREA
		copyGeoUf1Field( buf, data, i, 172, 186 );
		{
		    char* endp;
		    errno = 0;
		    area[i] = strtoul( buf, &endp, 10 );
		    assert( errno == 0 );
		    assert( endp != buf );
		}
#endif /* READ_INT_AREA*/

#if READ_INT_POP
		// population
		copyGeoUf1Field( buf, data, i, 292, 301 );
		pop[i] = atoi( buf );
		totalpop += pop[i];
		if ( pop[i] > maxpop ) {
			maxpop = pop[i];
		}
#endif
#if READ_UBIDS
		ubids[i].ubid = ubid( i );
		ubids[i].index = i;
#endif
#if COUNT_DISTRICTS
		char* cd;
		//cd = (char*)((unsigned long)data + i*sizeof_GeoUf1 + 136 ); // 106th cong
		cd = (char*)((unsigned long)data + i*sizeof_GeoUf1 + 138 ); // 108th cong
		bool match = false;
		for ( int j = 0; j < congressionalDistricts; j++ ) {
			if ( (cd[0] == dsts[j][0]) && (cd[1] == dsts[j][1]) ) {
				match = true;
				break;
			}
		}
		if ( ! match ) {
			dsts[congressionalDistricts] = cd;
			congressionalDistricts++;
		}
#endif
	}
#if READ_UBIDS
	qsort( ubids, numPoints, sizeof( UST ), ubidSortF );
	for ( i = 0; i < 10; i++ ) {
		fprintf( stderr, "%lld\t%d\n", ubids[i].ubid, ubids[i].index );
	}
#endif
#if READ_INT_POS
	printf("Uf1::load() minx %d, miny %d, maxx %d, maxy %d\n", minx, miny, maxx, maxy );
#endif
	return 0;
}
uint64_t Uf1::ubid( int index ) {
		char buf[3+6+4+1];
		copyGeoUf1Field( buf    , data, index, 31, 34 ); // county
		copyGeoUf1Field( buf + 3, data, index, 55, 61 ); // tract
		copyGeoUf1Field( buf + 9, data, index, 62, 66 ); // block
		//buf[13] = '\0';
		return strtoull( buf, NULL, 10 );
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
int GeoBin::load() {
	size_t s;
	uintptr_t p = (uintptr_t)data;
	int32_t endianness = 1;
	endianness = *((int32_t*)p);
	p += sizeof(endianness);
	/* if bin file was written in other endianness, 1 will be at wrong end. */
	endianness = ! ( endianness & 0xff );
	if ( endianness ) {
		fprintf(stderr,"reading reverse endia gbin\n");
		numPoints = swap32( *((uint32_t*)p) );
	} else {
		numPoints = *((uint32_t*)p);
	}
	p += sizeof(numPoints);
#if READ_INT_POS
	pos = new int[numPoints*2];
	assert(pos != NULL);
	maxx = maxy = INT_MIN;
	minx = miny = INT_MAX;
#define ecopypos( i ) (int)swap32(*((uint32_t*)(p + ((i)*4))))
#endif
#if READ_DOUBLE_POS
	pos = new double[numPoints*2];
#define ecopypos( i ) (int)swap64(*((uint64_t*)(p + ((i)*8))))
#endif
	s = sizeof(*pos) * numPoints * 2;
	if ( endianness ) {
		for ( int i = 0; i < numPoints*2; i++ ) {
			pos[i] = ecopypos(i);
		}
	} else {
		memcpy( pos, (void*)p, s );
	}
	p += s;
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
#if READ_INT_POP
	pop = new int[numPoints];
	totalpop = 0;
	maxpop = 0;
	s = sizeof(int)*numPoints;
	if ( endianness ) {
		for ( int i = 0; i < numPoints; i++ ) {
			pop[i] = (int)swap32(*((uint32_t*)(p + ((i)*4))));
			totalpop += pop[i];
			if ( pop[i] > maxpop ) {
				maxpop = pop[i];
			}
		}
	} else {
		memcpy( pop, (void*)p, s );
		for ( int i = 0; i < numPoints; i++ ) {
			totalpop += pop[i];
			if ( pop[i] > maxpop ) {
				maxpop = pop[i];
			}
		}
	}
	p += s;
#endif
#if READ_INT_AREA
	area = new uint32_t[numPoints];
	s = sizeof(int)*numPoints;
	if ( endianness ) {
		for ( int i = 0; i < numPoints; i++ ) {
			area[i] = swap32(*((uint32_t*)(p + ((i)*4))));
		}
	} else {
		memcpy( area, (void*)p, s );
	}
	p += s;
#endif
#if READ_UBIDS
	ubids = new UST[numPoints];
	{
		void* buf = malloc( sizeof(uint64_t)*numPoints );
		uint32_t* ib = (uint32_t*)buf;
		uint64_t* sb = (uint64_t*)buf;
		// copy ensures that array will be well aligned.
		memcpy( ib, (void*)p, sizeof(uint32_t)*numPoints );
		if ( endianness ) {
			for ( int i = 0; i < numPoints; i++ ) {
				ubids[i].index = swap32( ib[i] );
			}
		} else {
			for ( int i = 0; i < numPoints; i++ ) {
				ubids[i].index = ib[i];
			}
		}
		p += sizeof(uint32_t)*numPoints;
		// copy ensures that array will be well aligned.
		memcpy( sb, (void*)p, sizeof(uint64_t)*numPoints );
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
#endif
	return 0;
}

GeoData* openZCTA( char* inputname ) {
	ZCTA* toret = new ZCTA();
	toret->open( inputname );
	return toret;
}
GeoData* openUf1( char* inputname ) {
	Uf1* toret = new Uf1();
	toret->open( inputname );
	return toret;
}
GeoData* openBin( char* inputname ) {
	GeoBin* toret = new GeoBin();
	toret->open( inputname );
	return toret;
}

/*
First line:  <# of vertices> <dimension (must be 2)> <# of attributes> <# of boundary markers (0 or 1)>
Remaining lines:  <vertex #> <x> <y> [attributes] [boundary marker]
*/
#if 0
int readNodes( char* inputname, PosPop** zipdP ) {
	PosPop* zipd = NULL;
	int numZips = 0;
	
	*zipdP = zipd;
	return numZips;
}
#endif

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


int Uf1::numDistricts() {
#if COUNT_DISTRICTS
	return congressionalDistricts;
#else
	char** dsts;
	int num = 0;
	char* cd;
	int numZips = 0;
	
	numZips = sb.st_size / sizeof_GeoUf1;
	dsts = (char**)malloc( sizeof(*dsts) * MAX_DISTRICTS );
	for ( int i = 0; i < numZips; i++ ) {
		cd = (char*)((unsigned long)data + i*sizeof_GeoUf1 + 136 );
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
	}

	free( dsts );
	if ( num > 1 ) {
		return num;
	}
	return -1;
#endif
}

#include "tiger/mmaped.h"

Node* initNodesFromLinksFile( GeoData* gd, const char* inputname ) {
	int i, j;
	int maxneighbors = 0;
	int numPoints = gd->numPoints;
	Node* nodes = new Node[numPoints];
	for ( i = 0; i < numPoints; i++ ) {
		nodes[i].numneighbors = 0;
	}
	// read edges from edge file
	char* linkFileName = strdup( inputname );
	assert(linkFileName != NULL);
	{
	    size_t nlen = strlen( linkFileName ) + 8;
	    linkFileName = (char*)realloc( linkFileName, nlen );
	    assert(linkFileName != NULL);
	}
	strcat( linkFileName, ".links" );
	mmaped linksFile;
	linksFile.open( linkFileName );
#define sizeof_linkLine 27
	int numEdges = linksFile.sb.st_size / sizeof_linkLine;
	long* edgeData = new long[numEdges*2];
	char buf[14];
	buf[13] = '\0';
	j = 0;
	for ( i = 0 ; i < numEdges; i++ ) {
		uint64_t tubid;
		memcpy( buf, ((caddr_t)linksFile.data) + sizeof_linkLine*i, 13 );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2  ] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2  ] < 0 ) {
			printf("ubid %lld => index %ld\n", tubid, edgeData[j*2] );
			continue;
		}
		memcpy( buf, ((caddr_t)linksFile.data) + sizeof_linkLine*i + 13, 13 );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2+1] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2+1] < 0 ) {
			printf("ubid %lld => index %ld\n", tubid, edgeData[j*2+1] );
			continue;
		}
		//printf("ubid %lld => index %d\n", tubid, edgeData[i*2] );
		nodes[edgeData[j*2  ]].numneighbors++;
		//printf("ubid %lld => index %d\n", tubid, edgeData[i*2+1] );
		nodes[edgeData[j*2+1]].numneighbors++;
		j++;
	}
	numEdges = j;
	linksFile.close();
	free( linkFileName );
	// allocate all the space
	int* allneigh = new int[numEdges * 2]; // if you care, "delete [] nodes[0].neighbors;" somewhere
	int npos = 0;
	// give space to each node as counted above
	for ( i = 0; i < numPoints; i++ ) {
		Node* cur;
		cur = nodes + i;
		cur->neighbors = allneigh + npos;
		if ( cur->numneighbors > maxneighbors ) {
			maxneighbors = cur->numneighbors;
		}
		npos += cur->numneighbors;
		cur->numneighbors = 0;
	}
	// copy edges into nodes
	for ( j = 0; j < numEdges; j++ ) {
		int ea, eb;
		Node* na;
		Node* nb;
		ea = edgeData[j*2];
		eb = edgeData[j*2 + 1];
		na = nodes + ea;
		nb = nodes + eb;
		na->neighbors[na->numneighbors] = eb;
		na->numneighbors++;
		nb->neighbors[nb->numneighbors] = ea;
		nb->numneighbors++;
	}
	delete [] edgeData;
	return nodes;
}
