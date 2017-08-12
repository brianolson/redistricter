#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <zlib.h>

#include "arghandler.h"
#include "BinaryStatLogger.h"
#include "CountyCityDistricter.h"
#include "District2.h"
#include "Solver.h"
#include "LinearInterpolate.h"
#include "GrabIntermediateStorage.h"
#include "Node.h"
#include "mmaped.h"
#include "NearestNeighborDistrictSet.h"
#include "swap.h"
#include "GeoData.h"
#include "renderDistricts.h"
#include "LastNMinMax_inl.h"

#if 01
extern "C" double greatCircleDistance(double lat1,double long1,double lat2,double long2);
#else
// cheat, use cartesian distance because it's faster
double greatCircleDistance(double lat1,double long1,double lat2,double long2) {
	double dlat = lat1 - lat2;
	double dlon = lon1 - lon2;
	return sqrt( dlat * dlat + dlon * dlon );
}
#endif


inline int nodeCalcOrder( Node* nodes, POPTYPE* pit, int n ) {
	return nodes[n].calcOrder( nodes, pit, n );
}

DistrictSet* NearestNeighborDistrictSetFactory(Solver* sov) {
	return new NearestNeighborDistrictSet(sov);
}
DistrictSet* District2SetFactory(Solver* sov) {
	return new District2Set(sov);
}
DistrictSet* CountyCityDistrictFactory(Solver* sov) {
	return new CountyCityDistricterSet(sov);
}

struct DistrictSetFactory {
	DistrictSet* (*factory)(Solver*);
	const char* name;
};

DistrictSetFactory districtSetFactories[] = {
	{ District2SetFactory, "Grab Solver" },
	{ NearestNeighborDistrictSetFactory, "Nearest Neighbor" },
	{ CountyCityDistrictFactory, "County-City Solver" },
	{ NULL, NULL },
};

const char* Solver::getSetFactoryName(int index) {
	return districtSetFactories[index].name;
}

void Solver::setFactoryByIndex(int index) {
	assert(index >= 0);
	districtSetFactory = districtSetFactories[index].factory;
	assert(districtSetFactory);
}


static char oldDefaultInputName[] = "ca.pb";

Solver::Solver() :
	districts( 5 ), totalpop( 0.0 ), districtPopTarget( 0.0 ),
	nodes( NULL ), allneigh( NULL ), winner( NULL ),
	districtSetFactory( NULL ), _dists( NULL ),
	inputname( oldDefaultInputName ), generations( -1 ),
	dumpname( NULL ),
	initMode( initWithNewDistricts ),
	solutionLogPrefix( NULL ), solutionLogInterval( 100 ), solutionLogCountdown( 0 ),
#if WITH_PNG
	pngLogPrefix( NULL ), pngLogInterval( 100 ), pngLogCountdown( 0 ),
#endif
	statLog( NULL ), statLogInterval( 100 ), statLogCountdown( 0 ), pbStatLog( NULL ),
	distfname( NULL ), coordfname( NULL ),
#if WITH_PNG
	pngname( NULL ), pngWidth( 1000 ), pngHeight( 1000 ),
#endif
	gd( NULL ), geoFact( openUf1 ),
	/*sorti( NULL ),*/
	minx( INT_MAX ), miny( INT_MAX ), maxx( INT_MIN ), maxy( INT_MIN ),
	viewportRatio( 1.0 ), gencount( 0 ), blaf( NULL ),
	runDutySeconds(10), sleepDutySeconds(0),
	recentKmpp( NULL ),
	recentSpread( NULL ),
	giveupSteps( 5000 ),
	recentKmppGiveupFraction( 0.0005 ),
	recentSpreadGiveupFraction( 0.0005 ),
	showLinks( 0 ),
	renumber( NULL ),
	_point_draw_array( NULL ), 
	lastGenDrawn( -1 ), vertexBufferObject(0), colorBufferObject(0), linesBufferObject(0),
	drawHistoryPos( 0 ),
	maxSpreadFraction( 1000.0 ), maxSpreadAbsolute( 9.0e9 ),
	loadFormat( DetectFormat ),
	loadname( NULL )
{
		//_link_draw_array( NULL ),
}
Solver::~Solver() {
	if ( nodes != NULL ) {
		delete [] nodes;
	}
	if ( allneigh != NULL ) {
		delete [] allneigh;
	}
	if ( winner != NULL ) {
		delete [] winner;
	}
	if ( _dists != NULL ) {
		delete _dists;
	}
	if ( gd != NULL ) {
		delete gd;
	}
#if WITH_PNG
	if ( pngname != NULL ) {
		free(pngname);
	}
#endif
	if ( distfname != NULL ) {
		delete distfname;
	}
	if ( coordfname != NULL ) {
		delete coordfname;
	}
	if ( dumpname != NULL ) {
		free( dumpname );
	}
#if 0
	if ( loadname != NULL ) {
		delete loadname;
	}
#endif
	if (pbStatLog != NULL) {
		delete pbStatLog;
	}
}

// doesn't do anything, just a tag to switch on in Solver::load()
GeoData* protobufGeoDataTag( const char* inputname ) {
	assert(0);
	exit(1);
	return NULL;
}

int writeToProtoFile(Solver* sov, const char* filename);
int readFromProtoFile(Solver* sov, const char* filename);

int Solver::writeProtobuf( const char* fname ) {
	return writeToProtoFile(this, fname);
}

void Solver::load() {
	int err = -1;
	if ( err >= 0 ) {
		// success. done.
	} else if ( geoFact == protobufGeoDataTag ) {
		err = readFromProtoFile(this, inputname);
		if (err < 0) {
			return;
		}
	} else
	{
		gd = geoFact( inputname );
		err = gd->load();
		if ( err < 0 ) {
			return;
		}
		readLinksFile(NULL);
	}
	
#if 0
	if ( districts <= 0 ) {
		int tdistricts = gd->numDistricts();
		if ( tdistricts > 1 ) {
			districts = tdistricts;
		} else {
			districts = districts * -1;
		}
	}
#endif

	minx = gd->minx;
	maxx = gd->maxx;
	miny = gd->miny;
	maxy = gd->maxy;
	totalpop = gd->totalpop;
	dcx = (maxx + minx) / 2.0;
	dcy = (miny + maxy) / 2.0;
	//printf("minx %0.6lf, miny  %0.6lf, maxx %0.6lf, maxy %0.6lf\n", (double)minx, (double)miny, (double)maxx, (double)maxy );
	zoom = 1.0;
	districtPopTarget = totalpop / districts;
}

void Solver::readLinksFile(const char* filename) {
	// read edges from edge file
	char* linkFileName = NULL;
	mmaped linksFile;
	if (filename == NULL ) {
		linkFileName = strdup( inputname );
		assert(linkFileName != NULL);
		{
			size_t nlen = strlen( linkFileName ) + 8;
			linkFileName = (char*)realloc( linkFileName, nlen );
			assert(linkFileName != NULL);
		}
		strcat( linkFileName, ".links" );
		linksFile.open( linkFileName );
	} else {
		linksFile.open( filename );
	}
	readLinksFileData((const char*)linksFile.data, linksFile.sb.st_size);
	linksFile.close();
	if (linkFileName != NULL) {
		free( linkFileName );
	}
}
bool Solver::readLinksFileData(const char* data, size_t len) {
	char buf[20];
	memset(buf, 0, sizeof(buf));
	int j = 0;
	size_t sizeof_linkLine = 27;
	size_t sizeof_ubid = 13;
	int offseta = 0;
	int offsetb = 13;
	if ( data[26] == '\n') {
		// old format, two CCCTTTTTTBBBB county-tract-block sets, and '\n'
		//sizeof_linkLine = 27;
	} else if ( data[15] == ',' && data[31] == '\n' ) {
		// new format, two SSCCCTTTTTTBBBB state-county-tract-block values with ',' between and '\n' after
		sizeof_linkLine = 32;
		sizeof_ubid = 15;
		offseta = 0;
		offsetb = 16;
	} else {
		fprintf(stderr, "bad format links file\n");
		return false;
	}
	numEdges = len / sizeof_linkLine;
	edgeData = new int32_t[numEdges*2];
	int noIndexEdgeDataCount = 0;
	for ( unsigned int i = 0 ; i < numEdges; i++ ) {
		uint64_t tubid;
		memcpy( buf, ((caddr_t)data) + sizeof_linkLine*i + offseta, sizeof_ubid );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2  ] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2  ] < 0 ) {
			printf("ubid %lu => index %d\n", tubid, edgeData[j*2] );
			noIndexEdgeDataCount++;
			continue;
		}
		memcpy( buf, ((caddr_t)data) + sizeof_linkLine*i + offsetb, sizeof_ubid );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2+1] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2+1] < 0 ) {
			printf("ubid %lu => index %d\n", tubid, edgeData[j*2+1] );
			continue;
		}
		j++;
	}
	if (noIndexEdgeDataCount) {
		printf("%d no index edgeData parts of %d\n", noIndexEdgeDataCount, numEdges);
	}
	numEdges = j;
	return true;
}

void Solver::readLinksBin() {
	uintptr_t p = (uintptr_t)gd->data;
	int32_t endianness = 1;
	endianness = *((int32_t*)p);
	p += sizeof(endianness);
	/* if bin file was written in other endianness, 1 will be at wrong end. */
	endianness = ! ( endianness & 0xff );
	// TODO: make this some sort of sane encapsulation, or merge the two classes file concepts since I never actually use them separately.
	p += sizeof(gd->numPoints);
	p += sizeof(*(gd->pos)) * gd->numPoints * 2;
	p += sizeof(int)*gd->numPoints;
	p += sizeof(gd->area[0])*gd->numPoints;
	p += sizeof(uint32_t)*gd->numPoints;
	p += sizeof(uint64_t)*gd->numPoints;
	if ( endianness ) {
		numEdges = swap32( *((uint32_t*)p) );
	} else {
		numEdges = *((uint32_t*)p);
	}
	p += sizeof(uint32_t);
	edgeData = new int32_t[numEdges*2];
	if ( endianness ) {
		for ( unsigned int i = 0; i < numEdges*2; i++ ) {
			edgeData[i] = (int32_t)swap32(*((uint32_t*)(p + (i * 4))) );
		}
	} else {
		memcpy( edgeData, (void*)p, sizeof(int32_t)*numEdges*2 );
	}
}

int Solver::writeBin( const char* fname ) {
	int fd, err;
	fd = ::open( fname, O_WRONLY|O_CREAT, 0666 );
	if ( fd < 0 ) {
		perror(fname);
		return -1;
	}
	// write GeoData (position, area, pop, ubid<=>index)
	err = gd->writeBin( fd, fname );
	if ( err < 0 ) {
		::close( fd );
		return err;
	}
	// write edges (node adjacency)
	err = write( fd, &numEdges, sizeof(numEdges) );
	if ( err != sizeof(numEdges) ) {
		fprintf(stderr,"%s: writing numEdges err=%d \"%s\"\n", fname, err, strerror(errno) );
		::close( fd );
		return -1;
	}
	int s = sizeof(int32_t)*numEdges*2;
	err = write( fd, edgeData, s );
	if ( err != s ) {
		fprintf(stderr,"%s: writing edges size=%d err=%d \"%s\"\n", fname, s, err, strerror(errno) );
		//::close( fd );
		return -1;
	}
	return ::close( fd );
}

void Solver::initNodes() {
	int i;
	int maxneighbors = 0;
	int numPoints = gd->numPoints;
	nodes = new Node[numPoints];
	assert(nodes != NULL);
	for ( i = 0; i < numPoints; i++ ) {
		nodes[i].numneighbors = 0;
	}
	// read edges at load() time
	for ( unsigned int e = 0 ; e < numEdges; e++ ) {
		//printf("ubid %lld => index %d\n", tubid, edgeData[i*2] );
		nodes[edgeData[e*2  ]].numneighbors++;
		//printf("ubid %lld => index %d\n", tubid, edgeData[i*2+1] );
		nodes[edgeData[e*2+1]].numneighbors++;
	}
	// allocate all the space
	allneigh = new int[numEdges * 2];
	assert(allneigh != NULL);
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
	for ( unsigned int e = 0; e < numEdges; e++ ) {
		int ea, eb;
		Node* na;
		Node* nb;
		ea = edgeData[e*2];
		eb = edgeData[e*2 + 1];
		na = nodes + ea;
		nb = nodes + eb;
		na->neighbors[na->numneighbors] = eb;
		na->numneighbors++;
		nb->neighbors[nb->numneighbors] = ea;
		nb->numneighbors++;
	}
#if 0
	// sort points around each node into clockwise order
	double* angle = new double[maxneighbors];
	double* anglesave = new double[maxneighbors];
	for ( i = 0; i < numPoints; i++ ) {
		Node* cur;
		cur = nodes + i;
		double ix, iy;
		ix = gd->pos[i * 2];
		iy = gd->pos[i * 2 + 1];
		for ( j = 0; j < cur->numneighbors; j++ ) {
			int nj;
			nj = cur->neighbors[j];
			angle[j] = atan2( gd->pos[nj * 2 + 1] - iy,
							  gd->pos[nj * 2] - ix );
			anglesave[j] = angle[j];
		}
		// selection sort
		for ( j = 0; j < cur->numneighbors - 1; j++ ) {
			int minj;
			minj = j;
			for ( int ij = j+1; ij < cur->numneighbors; ij++ ) {
				if ( angle[ij] < angle[minj] ) {
					minj = ij;
				}
			}
			if ( minj != j ) {
				double ta;
				int ni;
				ta = angle[j];
				ni = cur->neighbors[j];
				angle[j] = angle[minj];
				cur->neighbors[j] = cur->neighbors[minj];
				angle[minj] = ta;
				cur->neighbors[minj] = ni;
			}
		}
		for ( j = 0; j < cur->numneighbors - 1; j++ ) {
			if( angle[j] > angle[j+1] ) {
				for ( int ij = 0; ij < cur->numneighbors; ij++ ) {
					printf("%10g\t%10g\n", anglesave[ij], angle[ij] );
				}
				assert(0);
			}
		}
	}
	delete [] anglesave;
	delete [] angle;
#endif
#if 0
	for ( i = 0; i < numPoints; i++ ) {
		Node* cur;
		cur = nodes + i;
		printf("node %6d neighbors:", i );
		for ( int j = 0; j < cur->numneighbors; j++ ) {
			printf( " %d", cur->neighbors[j] );
		}
		printf( "\n" );
	}
#endif
}

DistrictSet* Solver::getDistricts() { // lazy allocating caching accessor
    if (_dists != NULL) {
	return _dists;
    }
	if ( districtSetFactory != NULL ) {
		_dists = districtSetFactory(this);
	} else {
#if 1
		_dists = new NearestNeighborDistrictSet(this);
#else
		_dists = new District2Set(this);
#endif
	}
	assert(_dists != NULL);
	_dists->alloc(districts);  // TODO: mave this into lazy allocating within a DistrictSet
	return _dists;
}

void Solver::allocSolution() {
	winner = new POPTYPE[gd->numPoints];
	assert(winner != NULL);
	for ( int i = 0; i < gd->numPoints; i++ ) {
		winner[i] = NODISTRICT;
	}
}

#define ZFILE_VERSION 4
#define ZFILE_INT uint32_t

int _writeOrFail( int fd, void* ib, size_t len ) {
	caddr_t buf = (caddr_t)ib;
	int err;
	size_t remain = len;
	while ( (err = write( fd, buf, remain )) > 0 ) {
		size_t diff;
		diff = remain - err;
		if ( diff == 0 ) {
			return len;
		} else {
			buf += diff;
			remain -= diff;
		}
	}
	if ( err == 0 ) {
		return -1;
	}
	return err;
}
#define writeOrFail( f, b, l ) if ( _writeOrFail( (f), (b), (l) ) < 0 ) { fprintf(stderr,"%s:%d error %d \"%s\"\n", __FILE__, __LINE__, errno, strerror(errno) ); return -1; }
#define writeOrFailG( f, b, l, g ) if ( _writeOrFail( (f), (b), (l) ) < 0 ) { fprintf(stderr,"%s:%d error %d \"%s\"\n", __FILE__, __LINE__, errno, strerror(errno) ); goto g; }

int _readOrFail( int fd, void* ib, size_t len ) {
	caddr_t buf = (caddr_t)ib;
	int err;
	size_t remain = len;
	while ( (err = read( fd, buf, remain )) > 0 ) {
		size_t diff;
		diff = remain - err;
		if ( diff == 0 ) {
			return len;
		} else {
			buf += diff;
			remain -= diff;
		}
	}
	if ( err == 0 ) {
		return -1;
	}
	return err;
}
#define readOrFail( f, b, l ) if ( _readOrFail( (f), (b), (l) ) < 0 ) { fprintf(stderr,"%s:%d error %d \"%s\"\n", __FILE__, __LINE__, errno, strerror(errno) ); return -1; }
#define readOrFailG( f, b, l, g ) if ( _readOrFail( (f), (b), (l) ) < 0 ) { fprintf(stderr,"%s:%d error %d \"%s\"\n", __FILE__, __LINE__, errno, strerror(errno) ); goto g; }

#ifndef compressBound
#define compressBound( n ) ( (n) + ((n) / 500) + 12 )
#endif

// TODO: Write magic number prefix bytes? "BDSZ" maybe? (Brian's District Solution (Zlib compressed))
// Format (variable endianness, use fileversion to detect):
// uint32 fileversion
// uint32 numPoints
// uint32 compressed size
// byte[compressed size] compressed data
static int saveZSolution( const char* filename, POPTYPE* winner, int numPoints ) {
	int dumpfd;
	ZFILE_INT fileversion = ZFILE_VERSION;
	uLongf destlen;
	ZFILE_INT compsize;
	dumpfd = open( filename, O_WRONLY|O_CREAT, 0644 );
	if ( dumpfd < 0 ) {
		perror( filename );
		return -1;//exit(1);
	}
	writeOrFail( dumpfd, &fileversion, sizeof(ZFILE_INT) );
	writeOrFail( dumpfd, &(numPoints), sizeof(ZFILE_INT) );
	destlen = compressBound( numPoints * sizeof(POPTYPE) );
	Bytef* winnerz = (Bytef*)malloc( destlen );
	compress( winnerz, &destlen, winner, numPoints * sizeof(POPTYPE) );
	compsize = destlen;
	writeOrFailG( dumpfd, &compsize, sizeof(ZFILE_INT), saveZFail );
	writeOrFailG( dumpfd, winnerz, destlen, saveZFail );
	free( winnerz );
	return close( dumpfd );
saveZFail:
	free( winnerz );
	close( dumpfd );
	return -1;
}

int Solver::saveZSolution( const char* filename ) {
	return ::saveZSolution( filename, winner, gd->numPoints );
}

// todo: template?
int max(int a, int b) {
    if (a > b) return a;
    return b;
}

int Solver::loadZSolution( const char* filename ) {
	ZFILE_INT ti;
	uLongf unzlen;
	Bytef* zb;
	int readfd;
	int endianness = 0;
	
	if (0 == strcmp(filename, "-")) {
		readfd = STDIN_FILENO;
	} else {
		readfd = open( filename, O_RDONLY, 0 );
	}
	if ( readfd < 0 ) {
		perror( filename );
		return -1;
	}
	readOrFail( readfd, &ti, sizeof(ZFILE_INT) );
	if ( swap32(ti) == ZFILE_VERSION ) {
		endianness = 1;
	} else if ( ti != ZFILE_VERSION ) {
		fprintf(stderr,"%s: reading bad file version %d, expected %d\n", filename, ti, ZFILE_VERSION );
		close( readfd );
		return -1;
	}
	readOrFail( readfd, &ti, sizeof(ZFILE_INT) );
	if ( endianness ) {
		ti = swap32(ti);
	}
	if ( ti != (ZFILE_INT)(gd->numPoints) ) {
		fprintf(stderr,"%s: file has %d points but want %d\n", filename, ti, gd->numPoints );
		close( readfd );
		return -1;
	}
	readOrFail( readfd, &ti, sizeof(ZFILE_INT) );
	if ( endianness ) {
		ti = swap32(ti);
	}
	zb = (Bytef*)malloc( ti );
	readOrFailG( readfd, zb, ti, loadZFail );
	unzlen = gd->numPoints * sizeof(POPTYPE);
	uncompress( winner, &unzlen, zb, ti );
	free( zb );
	if ( unzlen != gd->numPoints * sizeof(POPTYPE) ) {
		fprintf(stderr,"%s: decompresses to %lu bytes but wanted %lu\n", filename, unzlen, (unsigned long)(gd->numPoints * sizeof(POPTYPE)) );
		close( readfd );
		return -1;
	}
	if ( endianness ) {
		assert( sizeof(POPTYPE) == 1 );
	}
	if (districts == -1) {
	    int maxd = -1;
	    for (int i = 0; i < gd->numPoints; ++i) {
		maxd = max(winner[i], maxd);
	    }
	    districts = maxd + 1;
	}
	getDistricts()->initFromLoadedSolution();
	return close( readfd );
loadZFail:
	free( zb );
	close( readfd );
	return -1;
}

static bool districtNumberIsValid(int d, int districts) {
    if (d < 1) return false;
    if (districts == -1) {
	if (d > POPTYPE_MAX) return false;
    } else {
	if (d > districts) return false;
    }
    return true;
}

int Solver::loadCsvSolution( const char* filename ) {
	FILE* fin;
	if (0 == strcmp(filename, "-")) {
		fin = stdin;
	} else {
		fin = fopen(filename, "rb");
	}
	if (fin == NULL) {
		perror( filename );
		return -1;
	}
	static const int MAX_LINE_LENGTH = 1024;
	char* lineBuf = new char[MAX_LINE_LENGTH];
	char* line;
	line = fgets(lineBuf, MAX_LINE_LENGTH, fin);
	int err = 0;
	int errcount = 0;
	memset(winner, NODISTRICT, gd->numPoints);
	int setcount = 0;
	int maxd = -1;


	while (line != NULL) {
		char* c = line;
		while ((*c != '\0') && (!isdigit(*c))) {
			c++;
		}
		char* endp = NULL;
		uint64_t tubid = strtoull(c, &endp, 10);
		if ((endp == NULL) || (endp == c)) {
			perror("reading ubid");
			err = errno;
			break;
		}
		c = endp;
		while ((*c != '\0') && (!isdigit(*c))) {
			c++;
		}
		endp = NULL;
		long district = strtol(c, &endp, 10);
		if ((endp == NULL) || (endp == c)) {
			perror("reading district number");
			err = errno;
			break;
		} 
		uint32_t index = gd->indexOfUbid(tubid);
		if (index == INVALID_INDEX) {
			fprintf(stderr, "bogus ubid %lu\n" , tubid);
			errcount++;
			if (errcount > 20) {
				err = -1;
				break;
			}
		} else if (!districtNumberIsValid(district, districts)) {
			errcount++;
			if (errcount < 20) {
			    fprintf(stderr, "winner[%d]=%lu would be out of range (1..%d)\n", index, district, districts);
			}
#if false
			if (errcount > 20) {
				err = -1;
				break;
			}
#endif
			winner[index] = NODISTRICT;
		} else {
			winner[index] = district - 1;
			maxd = max(maxd, winner[index]);
			setcount++;
		}
		line = fgets(lineBuf, MAX_LINE_LENGTH, fin);
	}
	if (err == 0) {
		err = ferror(fin);
	}
	delete [] lineBuf;
	if (err != 0) {
		fprintf(stderr, "error reading file \"%s\" (%d): %s\n", filename, err, strerror(err));
	}
	int notset = 0;
	for (int i = 0; i < gd->numPoints; ++i) {
		if (winner[i] == NODISTRICT) {
			notset++;
		}
	}
	fprintf(stderr, "set %d points of %d, %d not set\n", setcount, gd->numPoints, notset);
	if (districts == -1) {
	    districts = maxd + 1;
	} else {
	    if (maxd + 1 != districts) {
		fprintf(stderr, "expected %d districts but got %d\n", districts, maxd + 1);
	    }
	}
	getDistricts()->initFromLoadedSolution();
	return err;
}


int Solver::loadSolution() {
    switch (loadFormat) {
    case DszFormat:
	return loadZSolution(loadname);
	break;
    case CsvFormat:
	return loadCsvSolution(loadname);
	break;
    default:
	if (strstr(loadname, ".dsz") != NULL) {
	    return loadZSolution(loadname);
	}
	if (strstr(loadname, ".csv") != NULL) {
	    return loadCsvSolution(loadname);
	}
	if (strstr(loadname, ".text") != NULL) {
	    return loadCsvSolution(loadname);
	}
	fprintf(stderr, "loadSolution could not guess type for '%s'\n", loadname);
	assert(false);
    }
    return -1;
}


bool Solver::hasSolutionToLoad() {
    return (loadname != NULL) && (loadname[0] != '\0');
}

const char* Solver::getSolutionFilename() const {
    return loadname;
}

void Solver::initSolution() {
	getDistricts()->initNewRandomStart();
}

void Solver::initSolutionFromOldCDs() {
	POPTYPE d;
	for ( int i = 0; i < gd->numPoints; i++ ) {
		d = ((Uf1*)gd)->oldDist( i );
		if ( d == ERRDISTRICT || d == NODISTRICT ) {
#if 01
			continue;
#else
			initSolution();
			return;
#endif
		}
		//assert( d >= 0 );
		assert( d < districts );
		winner[i] = d;
	}
	getDistricts()->initFromLoadedSolution();
}

void Solver::init() {
}

#ifndef TRIANGLE_STEP
#define TRIANGLE_STEP 0
#endif

int Solver::step() {
	int err = getDistricts()->step();
	gencount++;
	return err;
}

#if 1
void Solver::printDistricts(const char* filename) {
    getDistricts()->print(filename);
}
#else
void Solver::printDistrcts() {
	FILE* distf = NULL;
	distf = fopen( distfname, "w" );
	if ( distf == NULL ) {
		perror( distfname );
		exit(1);
	}
	POPTYPE median = sorti[districts/2];
	fprintf( distf, "# med population=%9.0lf (district %d)\n", dists[median].pop, median );
	fflush( distf );
	fclose( distf );
}
#endif

SolverStats* Solver::getDistrictStats() {
	SolverStats* stats = new SolverStats();
	getDistricts()->getStats(stats);
	stats->generation = gencount;
	return stats;
}
int SolverStats::toString( char* str, int len ) {
	char* cstr = str;
	int clen = len;
	int tlen;
#define CCUP tlen = strlen( cstr ); clen -= tlen; cstr += tlen; if ( clen <= 0 ) { return len; }
	if ( nod != 0 ) {
		snprintf( cstr, clen, "gen %d: %d in no district (pop=%.0f)", generation, nod, nodpop ); CCUP;
	} else {
		snprintf( cstr, clen, "generation %d:", generation ); CCUP;
	}
	snprintf( cstr, clen, " %0.11lg Km/person to pop center; %0.11lg Km/person to land center\npopulation avg=%.0lf std=%0.9g\n", kmppp, avgPopDistToCenterOfDistKm, popavg, popstd ); CCUP;
	snprintf( cstr, clen, "max=%.0lf (dist# %d)  ", popmax, maxdist + 1 ); CCUP;
	snprintf( cstr, clen, "min=%.0lf (dist# %d)  ", popmin, mindist + 1 ); CCUP;
	snprintf( cstr, clen, "median=%.0lf (dist# %d)\n", popmed, meddist + 1 ); CCUP;
	return len - clen;
#undef CCUP
}
int Solver::getDistrictStats( char* str, int len ) {
	SolverStats* ns;
	ns = getDistrictStats();
	if ( ns == NULL ) {
		return 0;
	}
	int toret = ns->toString( str, len );
	delete ns;
	return toret;
}

#if 0
void writeCoordText( FILE* coordf, POPTYPE* winner ) {
	int i, j;
	for ( i = 0; i < districts; i++ ) {
		fprintf( coordf, "coords %d:",i);
		for ( j = 0; j < numZips; j++ ) {
			if ( winner[j] == i ) {
				fprintf( coordf, " (%f,%f)", zipd[j].pos.lon, zipd[j].pos.lat );
			}
		}
		fprintf( coordf, "\n");
	}
	fflush( coordf );
}
void writeFinalSpew( POPTYPE* winner, FILE* blaf ) {
	double popvar = getPopvar( winner );
	double moment = getMoment( winner );
	fprintf( blaf, "winner, popvar=%0.9g, moment=%0.9g, fitness=%0.9g\n", popvar, moment, fitness[sorti[0]] );
#if 0
	for ( int i = 0; i < numZips; i++ ) {
		fprintf( blaf, " %d", winner[i] );
	}
	fprintf( blaf, "\n");
#endif
	fflush( blaf );
}
#endif

inline void swap( struct rusage*& a, struct rusage*& b ) {
	struct rusage* t;
	t = a;
	a = b;
	b = t;
}

static inline int64_t tvDiffUsec( const struct timeval& a, const struct timeval& b ) {
	int64_t toret = a.tv_sec;
	toret -= b.tv_sec;
	toret *= 1000000;
	toret += a.tv_usec;
	toret -= b.tv_usec;
	return toret;
}
static inline int64_t tvDiffUsec( const struct timeval* a, const struct timeval* b ) {
	int64_t toret = a->tv_sec;
	toret -= b->tv_sec;
	toret *= 1000000;
	toret += a->tv_usec;
	toret -= b->tv_usec;
	return toret;
}
#if 0
void printRUDiff( FILE* f, const struct rusage* a, const struct rusage* b ) {
	long udu = (a->ru_utime.tv_sec - b->ru_utime.tv_sec) * 1000000 + a->ru_utime.tv_usec - b->ru_utime.tv_usec;
	fprintf( f, "%ld usec user time\n", udu );
}
#endif
extern const unsigned char* colors;
extern int numColors;

#if WITH_PNG
void Solver::doPNG() {
	doPNG(winner, pngname);
}
void Solver::doPNG(POPTYPE* soln, const char* outname) {
	// TODO: default scale width by 1/cos(mean latitude)
	double ratioError = (((maxy - miny * 1.0) / (maxx - minx)) / ((1.0 * pngHeight) / pngWidth));
	if (ratioError > 1.05) {
		// data is taller than png
		int newPngWidth = pngHeight * ((maxx - minx * 1.0) / (maxy - miny));
		assert(newPngWidth < pngWidth);
		printf("newPngWidth=%d old pngWidth=%d\n", newPngWidth, pngWidth);
		pngWidth = newPngWidth;
	} else if (ratioError < 0.95) {
		// data is wider than png
		int newPngHeight = pngWidth * ((maxy - miny * 1.0) / (maxx - minx));
		assert(newPngHeight < pngHeight);
		printf("newPngHeight=%d old pngHeight=%d\n", newPngHeight, pngHeight);
		pngHeight = newPngHeight;
	}
	unsigned char* data = (unsigned char*)malloc(pngWidth*pngHeight*4*sizeof(unsigned char) );
	unsigned char** rows = (unsigned char**)malloc(pngHeight*sizeof(unsigned char*) );
	assert( data != NULL );
	assert( rows != NULL );
	
	for ( int y = 0; y < pngHeight; y++ ) {
		rows[y] = data + (y*pngWidth*4);
	}
	memset( data, 0x0, pngWidth*pngHeight*4*sizeof(unsigned char) );

	Adjacency ta;
	calculateAdjacency_r(&ta, gd->numPoints, districts, soln, nodes);
	recolorDists( ta.adjacency, ta.adjlen, districts );

	/* setup transformation */
	double ym = 0.999 * pngHeight / (maxy - miny);
	double xm = 0.999 * pngWidth / (maxx - minx);

	for ( int i = 0; i < gd->numPoints; i++ ) {
		double ox, oy;
		const unsigned char* color;
		if ( soln[i] == NODISTRICT ) {
			static const unsigned char colorNODISTRICT[3] = { 0,0,0 };
			color = colorNODISTRICT;
		} else {
			color = colors + ((soln[i] % numColors) * 3);
		}
		oy = (maxy - gd->pos[i*2+1]) * ym;
		ox = (gd->pos[i*2  ] - minx) * xm;
		int y, x;
		y = (int)oy;
		unsigned char* row;
		row = rows[y]; //data + (y*pngWidth*3);
		x = (int)ox;
		x *= 4;
		row[x  ] = color[0];//((unsigned char)( (((unsigned int)color[0]) * 3)/7 ));
		row[x+1] = color[1];//((unsigned char)( (((unsigned int)color[1]) * 3)/7 ));
		row[x+2] = color[2];//((unsigned char)( (((unsigned int)color[2]) * 3)/7 ));
		row[x+3] = 0xff; // 100% alpha
	}
	
	myDoPNG( outname, rows, pngHeight, pngWidth );
	free( rows );
	free( data );
}
#endif

const char* Solver::argHelp = "may use single - or double --; may use -n=v or -n v=n\n"
"More details in Solver.cpp\n"
"-P file             compiled state data\n"
"-g n                generations (steps) to run\n"
"-d n                number of districts to create\n"
"-o file             dump solution here\n"
"-loadSolution file  load previously generated solution (dsz format)\n"
"-csv-solution file  load census/portable csv block association file\n"
"-pngout file        where to write png of final solution\n"
"-pngW n             width of image to make\n"
"-pngH n             height of image to make\n"
"-statLog statlog    file to write per-step stats to\n"
"-binLog statlog     file to write binary per-step stats to\n"
"-sLog prefix        write out a solution file every few genrations to prefix%d.dsz\n"
"-pLog prefix        write out a solution png every few genrations to prefix%d.png\n"
"-oldCDs             use old districts as starting point\n"
"-blankDists         start with empty map and make new districts\n"
"-q                  quiet, less status printed\n"
"-nearest-neighbor   use NearestNeighbor solver\n"
"-d2                 use District2 solver\n"
"-maxSpreadFraction  ignore results if district populations ((max - min)/avg) greater than this\n"
"-maxSpreadAbsolute  ignore results if district populations (max - min) greater than this\n"
"-runDutySeconds     run for this many seconds, then sleep (10)\n"
"-sleepDutySeconds   sleep for this many seconds, then run (0)\n";

int Solver::handleArgs( int argc, const char** argv ) {
	int argcout = 1;
	popRatioFactor.clear();
	popRatioFactor.setPoint( -1, District2::popRatioFactor );
	int argi = 1;
	const char* uf1InputName = NULL;
	const char* plGeoInputName = NULL;
	const char* pbInputName = NULL;
	const char* binLogName = NULL;
	const char* statLogName = NULL;
	bool oldCDs = false;
	bool blankDists = false;
	bool quiet = false;
	double popRatioFactorStart = NAN;
	double popRatioFactorEnd = NAN;
	const char* popRatioFactorPointString = NULL;
	bool nearestNeighbor = false;
	bool d2mode = false;
	bool ccmode = false;
	while (argi < argc) {
		StringArg("i", &inputname);  // deprecated
		StringArg("U", &uf1InputName);  // only for linkfixup
		StringArg("plgeo", &plGeoInputName);  // only for linkfixup
		StringArg("B", &pbInputName);  // deprecated, old binary format
		StringArg("P", &pbInputName);
		IntArg("g", &generations);
		IntArg("d", &districts);
		StringArgWithCopy("o", &dumpname);
		StringArgWithCallback("r", setDszLoadname, NULL);
		StringArgWithCallback("loadSolution", setLoadname, NULL);
		StringArgWithCallback("csv-solution", setCsvLoadname, NULL);
		StringArgWithCopy("distout", &distfname);  // deprecated?
		StringArgWithCopy("coordout", &coordfname);  // deprecated?
#if WITH_PNG
		StringArgWithCopy("pngout", &pngname);
		IntArg("pngW", &pngWidth);
		IntArg("pngH", &pngHeight);
		StringArgWithCopy("pLog", &pngLogPrefix);
#endif
		StringArg("binLog", &binLogName);
		StringArg("statLog", &statLogName);
		StringArgWithCopy("sLog", &solutionLogPrefix);
		BoolArg("oldCDs", &oldCDs);
		BoolArg("blankDists", &blankDists);
		BoolArg("q", &quiet);
		DoubleArg("popRatioFactor", &popRatioFactorStart);  // tuning for d2 solver
		DoubleArg("popRatioFactorEnd", &popRatioFactorEnd);  // tuning for d2 solver
		StringArg("popRatioFactorPoints", &popRatioFactorPointString);  // tuning for d2 solver
		BoolArg("nearest-neighbor", &nearestNeighbor);
		BoolArg("d2", &d2mode);
		BoolArg("cc", &ccmode);
		DoubleArg("maxSpreadFraction", &maxSpreadFraction);
		DoubleArg("maxSpreadAbsolute", &maxSpreadAbsolute);
		IntArg("giveupSteps", &giveupSteps);
		DoubleArg("kmppGiveupFraction", &recentKmppGiveupFraction);
		DoubleArg("spreadGiveupFraction", &recentSpreadGiveupFraction);
		IntArg("sleepDutySeconds", &sleepDutySeconds);
		IntArg("runDutySeconds", &runDutySeconds);
#if 1
		argv[argcout] = argv[argi];
		argcout++;
		argi++;
#else
		fprintf( stderr, "%s: bogus arg \"%s\"\n", argv[0], argv[argi] );
		fputs( argHelp, stderr );
		exit(1);
#endif
	}
	if (uf1InputName != NULL) {
		inputname = uf1InputName;
		geoFact = openUf1;
	}
	if (pbInputName != NULL) {
		inputname = pbInputName;
		geoFact = protobufGeoDataTag;
	}
	if (plGeoInputName != NULL) {
		inputname = plGeoInputName;
		geoFact = openPlGeo;
	}
	if (statLogName != NULL) {
		statLog = fopen(statLogName, "w");
		if ( statLog == NULL ) {
			perror(statLogName);
			exit(1);
		}
	}
	if (binLogName != NULL) {
		pbStatLog = BinaryStatLogger::open(binLogName);
	}
	if (oldCDs) {
		initMode = initWithOldDistricts;
	}
	if (blankDists) {
		initMode = initWithNewDistricts;
	}
	if (quiet) {
		blaf = NULL;
	}
	if (!isnan(popRatioFactorStart)) {
		popRatioFactor.setPoint(0, popRatioFactorStart);
		District2::popRatioFactor = popRatioFactorStart;
	}
	if (!isnan(popRatioFactorEnd)) {
		popRatioFactor.setPoint(gencount + generations, popRatioFactorEnd);
	}
	if (popRatioFactorPointString != NULL) {
		popRatioFactor.parse(popRatioFactorPointString);
	}
	if (nearestNeighbor) {
		districtSetFactory = NearestNeighborDistrictSetFactory;
	}
	if (d2mode) {
		districtSetFactory = District2SetFactory;
	}
	if (ccmode) {
		districtSetFactory = CountyCityDistrictFactory;
	}
	if (giveupSteps > 0) {
		recentKmpp = new LastNMinMax<double>(giveupSteps);
		recentSpread = new LastNMinMax<double>(giveupSteps);
	}
	return argcout;
}


void Solver::setCsvLoadname(void* context, const char* filename) {
    loadname = strdup(filename);
    loadFormat = CsvFormat;
}
void Solver::setDszLoadname(void* context, const char* filename) {
    loadname = strdup(filename);
    loadFormat = DszFormat;
}
void Solver::setLoadname(void* context, const char* filename) {
    loadname = strdup(filename);
    loadFormat = DetectFormat;
}



#if WITH_PNG
static char bestStdPng[] = "bestStd.png";
static char bestSpreadPng[] = "bestSpread.png";
static char bestKmppPng[] = "bestKmpp.png";
#endif

int Solver::main( int argc, const char** argv ) {
	struct rusage start, sa, sb, end;
	struct rusage* a = &sa;
	struct rusage* b = &sb;
	struct timeval wallSA, wallSB;
	struct timeval* walla = &wallSA;
	struct timeval* wallb = &wallSB;
#define estatbuf_length 1024
	char* estatbuf = new char[estatbuf_length];

	assert(estatbuf != NULL);
	srandom(time(NULL));
	getrusage( RUSAGE_SELF, &start );
	
	blaf = stdout;
	
	int argcout = handleArgs( argc, argv );
	if (argcout != 1) {
		fprintf( stderr, "%s: bogus arg \"%s\"\n", argv[0], argv[1] );
		fputs( Solver::argHelp, stderr );
		exit(1);
		return 1;
	}
	
	if ( statLog != NULL ) {
		fprintf(statLog, "# %s", argv[0] );
		for ( int i = 1; i < argc; ++i ) {
			fprintf(statLog, " %s", argv[i] );
		}
		fprintf(statLog, "\n" );
	}

	load();
	
	getrusage( RUSAGE_SELF, b );
	fprintf( stdout, "args & file load: %lf sec user time, %lf system sec\n", tvDiffUsec( b->ru_utime, start.ru_utime ) / 1000000.0, tvDiffUsec( b->ru_stime, start.ru_stime ) / 1000000.0 );
	
	if ( blaf ) {
		fprintf( blaf, "total population: %9.0lf\ntarget pop %9.0lf per each of %d districts\n", totalpop, districtPopTarget, districts );
	}

	initNodes();

	getrusage( RUSAGE_SELF, a );
	gettimeofday( wallb, NULL );
	fprintf( stdout, "Node setup: %lf sec user time, %lf system sec\n", tvDiffUsec( a->ru_utime, b->ru_utime ) / 1000000.0, tvDiffUsec( a->ru_stime, b->ru_stime ) / 1000000.0 );
	swap( a, b );

	allocSolution();
	if ( loadname != NULL ) {
		fprintf( stdout, "loading \"%s\"\n", loadname );
		if ( loadZSolution( loadname ) < 0 ) {
			return 1;
		}
	} else if ( initMode == initWithOldDistricts ) {
		initSolutionFromOldCDs();
	} else {
		assert(districts >= 0);
		assert(districts < 1000);
		initSolution();
	}
	init();
	
	gd->close();
	
	if ( generations == -1 ) {
	    generations = getDistricts()->defaultGenerations();
	}
	SolverStats* bestStd = NULL;
	SolverStats* bestSpread = NULL;
	SolverStats* bestKmpp = NULL;
	POPTYPE* bestStdMap = (POPTYPE*)malloc( sizeof(POPTYPE) * gd->numPoints );
	POPTYPE* bestSpreadMap = (POPTYPE*)malloc( sizeof(POPTYPE) * gd->numPoints );
	POPTYPE* bestKmppMap = (POPTYPE*)malloc( sizeof(POPTYPE) * gd->numPoints );
	// don't count kmpp till half way, early solutions cheat.
	//int bestKmppStart = gencount + (generations / 2);
	int genmax = gencount + generations;
	assert(bestStdMap != NULL);
	assert(bestSpreadMap != NULL);
	assert(bestKmppMap != NULL);
	time_t runDutyStart = time(NULL);
	
	while ( gencount < genmax ) {
		if (sleepDutySeconds && ((time(NULL) - runDutyStart) >= runDutySeconds)) {
			sleep(sleepDutySeconds);
			runDutyStart = time(NULL);
		}
		District2::popRatioFactor = popRatioFactor.value(gencount);
		//if (!(gencount%50))fprintf(stderr,"%d/%d popRatioFactor=%f\n", gencount, genmax, District2::popRatioFactor);
		if ( solutionLogPrefix != NULL ) {
			if ( solutionLogCountdown == 0 ) {
				size_t slnlen = strlen(solutionLogPrefix) + 20;
				char* solutionLogName = (char*)malloc( slnlen );
				snprintf( solutionLogName, slnlen, "%s%06d.dsz", solutionLogPrefix, gencount );
				saveZSolution( solutionLogName );
				free( solutionLogName );
				solutionLogCountdown = solutionLogInterval;
			}
			solutionLogCountdown--;
		}
#if WITH_PNG
		if ( pngLogPrefix != NULL ) {
			if ( pngLogCountdown == 0 ) {
				size_t slnlen = strlen(pngLogPrefix) + 20;
				char* pngLogName = (char*)malloc( slnlen );
				char* tpngname = pngname;
				snprintf( pngLogName, slnlen, "%s%06d.png", pngLogPrefix, gencount );
				pngname = pngLogName;
				doPNG();
				pngname = tpngname;
				free( pngLogName );
				pngLogCountdown = pngLogInterval;
			}
			pngLogCountdown--;
		}
#endif
		SolverStats* curst;
		curst = getDistrictStats();
		if (recentKmpp != NULL) {
			recentKmpp->put(curst->avgPopDistToCenterOfDistKm);
		}
		double spread = curst->popmax - curst->popmin;
		if (recentSpread != NULL) {
			recentSpread->put(spread);
		}
		bool allDistrictsClaimed = curst->nod == 0;
		bool absoluteSpreadOk = spread < maxSpreadAbsolute;
		bool spreadFractionOk = (spread / districtPopTarget) < maxSpreadFraction;
		// TODO: maybe keep stats on these things?
#if 0
		fprintf(stderr, "nod=%d ", curst->nod);
		if (absoluteSpreadOk) {
		  fprintf(stderr, "AOK ");
		} else {
		  fprintf(stderr, "(%f >= %f) ", spread, maxSpreadAbsolute);
		}
		if (spreadFractionOk) {
		  fprintf(stderr, "FOK\n");
		} else {
		  fprintf(stderr, "(%f >= %f)\n", (spread / districtPopTarget), maxSpreadFraction);
		}
#endif
		if ( allDistrictsClaimed && absoluteSpreadOk && spreadFractionOk ) {
			if ( bestStd == NULL ) {
				bestStd = new SolverStats();
				*bestStd = *curst;
				memcpy( bestStdMap, winner, sizeof(POPTYPE) * gd->numPoints );
			} else if ( curst->popstd < bestStd->popstd ) {
				*bestStd = *curst;
				memcpy( bestStdMap, winner, sizeof(POPTYPE) * gd->numPoints );
			}
			if ( bestSpread == NULL ) {
				bestSpread = new SolverStats();
				*bestSpread = *curst;
				memcpy( bestSpreadMap, winner, sizeof(POPTYPE) * gd->numPoints );
			} else if ( spread < (bestSpread->popmax - bestSpread->popmin) ) {
				*bestSpread = *curst;
				memcpy( bestSpreadMap, winner, sizeof(POPTYPE) * gd->numPoints );
			}
			if ( bestKmpp == NULL ) {
				bestKmpp = new SolverStats();
				*bestKmpp = *curst;
				memcpy( bestKmppMap, winner, sizeof(POPTYPE) * gd->numPoints );
			} else if ( curst->avgPopDistToCenterOfDistKm < bestKmpp->avgPopDistToCenterOfDistKm ) {
				*bestKmpp = *curst;
				memcpy( bestKmppMap, winner, sizeof(POPTYPE) * gd->numPoints );
			}
		}
		if ( (statLog != NULL) || (pbStatLog != NULL) ) {
			if ( statLogCountdown == 0 ) {
				if (statLog != NULL) {
					char ds[256];
					//getDistrictStats( ds, sizeof(ds) );
					curst->toString( ds, sizeof(ds) );
					fprintf( statLog, "generation: %d\n%s", gencount, ds );
					if ((recentKmpp != NULL) && (recentSpread != NULL)) {
						fprintf( statLog, "kmpp var per %d=%f, spread var per %d=%f\n",
								recentKmpp->count(), (1.0 * recentKmpp->max() - recentKmpp->min()) / recentKmpp->last(),
								recentSpread->count(), (1.0 * recentSpread->max() - recentSpread->min()) / districtPopTarget);
					}
					fprintf( statLog, "\n");
					statLogCountdown = statLogInterval;
					fflush( statLog );
				}
				if (pbStatLog != NULL) {
					pbStatLog->log(curst, recentKmpp, recentSpread);
				}
			}
			statLogCountdown--;
		}
		delete curst;
		if ( (gencount % 50) == 0 ) {
			printf("gen %d:\n", gencount );
			fflush(stdout);
		}
		step();
		if ( (gencount > giveupSteps) && nonProgressGiveup() ) {
			break;
		}
	}
	
	getrusage( RUSAGE_SELF, a );
	gettimeofday( walla, NULL );
	double usertime = tvDiffUsec( a->ru_utime, b->ru_utime ) / 1000000.0;
	double walltime = tvDiffUsec( walla, wallb ) / 1000000.0;
	snprintf( estatbuf, estatbuf_length, "District calculation: %lf sec user time, %lf system sec, %lf wall sec, %lf g/s\n", usertime, tvDiffUsec( a->ru_stime, b->ru_stime ) / 1000000.0, walltime, generations/usertime );
#define putStdoutAndLog(str) fputs( (str), stdout ); if ( statLog != NULL ) { fputs( "#", statLog ); fputs( (str), statLog ); }
	putStdoutAndLog( estatbuf );
	swap( a, b );

#if WITH_PNG
	if ( pngname != NULL ) {
		doPNG();
		
		getrusage( RUSAGE_SELF, a );
		fprintf( stdout, "PNG writing: %lf sec user time, %lf system sec\n", tvDiffUsec( a->ru_utime, b->ru_utime ) / 1000000.0, tvDiffUsec( a->ru_stime, b->ru_stime ) / 1000000.0 );
		swap( a, b );
	}
#endif

	if ( bestStd == NULL ) {
		putStdoutAndLog( "No generation counted for Best Std\n");
	} else {
		snprintf( estatbuf, estatbuf_length, "Best Std: Km/p=%f spread=%f std=%f gen=%d\n",
				 bestStd->avgPopDistToCenterOfDistKm, bestStd->popmax - bestStd->popmin,
				 bestStd->popstd, bestStd->generation );
		putStdoutAndLog( estatbuf );
	}
	if ( bestSpread == NULL ) {
		putStdoutAndLog( "No generation counted for Best Spread\n");
	} else {
		snprintf( estatbuf, estatbuf_length, "Best Spread: Km/p=%f spread=%f std=%f gen=%d\n",
				 bestSpread->avgPopDistToCenterOfDistKm, bestSpread->popmax - bestSpread->popmin,
				 bestSpread->popstd, bestSpread->generation );
		putStdoutAndLog( estatbuf );
	}
	if ( bestSpread == NULL ) {
		putStdoutAndLog( "No generation counted for Best Km/p\n");
	} else {
		snprintf( estatbuf, estatbuf_length, "Best Km/p: Km/p=%f spread=%f std=%f gen=%d\n",
				 bestKmpp->avgPopDistToCenterOfDistKm, bestKmpp->popmax - bestKmpp->popmin,
				 bestKmpp->popstd, bestKmpp->generation );
		putStdoutAndLog( estatbuf );
	}
	if ( bestStd != NULL ) {
#if WITH_PNG
		doPNG( bestStdMap, bestStdPng );
#endif
		::saveZSolution("bestStd.dsz", bestStdMap, gd->numPoints );
	}
	if ( bestSpread != NULL ) {
#if WITH_PNG
		doPNG( bestSpreadMap, bestSpreadPng );
#endif
		::saveZSolution("bestSpread.dsz", bestSpreadMap, gd->numPoints );
	}
	if ( bestKmpp != NULL ) {
#if WITH_PNG
		doPNG( bestKmppMap, bestKmppPng );
#endif
		::saveZSolution("bestKmpp.dsz", bestKmppMap, gd->numPoints );
	}

#if 0
	if ( distfname != NULL ) {
		printDistrcts();
	}
#endif
	if ( coordfname != NULL ) {
#if 0
		FILE* coordf = NULL;
		coordf = fopen( coordfname, "w" );
		if ( coordf == NULL ) {
			perror( argv[i] );
			exit(1);
		}
		fclose( coordf );
#endif
	}
	if ( dumpname != NULL ) {
		saveZSolution( dumpname );
	}

#if 0
	/* loop is structured this way so that generations=0 can be used to spew text from a loaded dump file */
	{
		POPTYPE* winner = pop + (sorti[0] * numZips);
		if ( blaf != NULL ) {
			writeFinalSpew( winner, blaf );
		}
		if ( coordf != NULL ) {
			writeCoordText( coordf, winner );
		}
	}
#endif
	getrusage( RUSAGE_SELF, &end );
	snprintf( estatbuf, estatbuf_length, "total: %lf sec user time, %lf system sec\n", tvDiffUsec( end.ru_utime, start.ru_utime ) / 1000000.0, tvDiffUsec( end.ru_stime, start.ru_stime ) / 1000000.0 );
	putStdoutAndLog( estatbuf );

	if ( bestStd != NULL ) {
		delete bestStd;
	}
	if ( bestSpread != NULL ) {
		delete bestSpread;
	}
	if ( bestKmpp != NULL ) {
		delete bestKmpp;
	}

	delete [] estatbuf;
	return 0;
}

int Solver::megaInit() {
	srandom(time(NULL));
	load();
	initNodes();
	allocSolution();
	if ( loadname != NULL ) {
		int err = loadZSolution( loadname );
		if ( err < 0 ) {
			return err;
		}
	} else if ( initMode == initWithOldDistricts ) {
		initSolutionFromOldCDs();
	} else {
		assert(districts >= 0);
		assert(districts < 1000);
		initSolution();
	}
	init();
	gd->close();
	return 0;
}

#if WITH_PNG
extern "C" void Solver_doPNG(void* s);

void Solver_doPNG(void *s) {
	((Solver*)s)->doPNG();
}
#endif

extern "C" void Solver_step(void* s);
void Solver_step(void* s) {
	((Solver*)s)->step();
}

void Solver::recordDrawTime() {
	struct timeval tv;
	int64_t t;
	if ( gettimeofday( &tv, NULL ) != 0 ) {
		return;
	}
	t = tv.tv_sec;
	t *= 1000000;
	t += tv.tv_usec;
	drawHistoryPos = (drawHistoryPos + 1) % DRAW_HISTORY_LEN;
	usecDrawTimeHistory[drawHistoryPos] = t;
	int pi = (drawHistoryPos + 1) % DRAW_HISTORY_LEN;
	fps = (1000000.0 * DRAW_HISTORY_LEN) / (t - usecDrawTimeHistory[pi]);
	//printf("fps=%lf\n", fps );
}

// draw stuff that doesn't need OpenGL
static const double stepfract = 5.0;
void Solver::nudgeLeft() {
	double halfWidth = (maxx - minx) / (2.0 * zoom);
	if ( dcx - halfWidth <= minx ) {
		return;
	}
	dcx -= halfWidth / stepfract;
	//printf("dcx=%lf\n", dcx );
}
void Solver::nudgeRight() {
	double halfWidth = (maxx - minx) / (2.0 * zoom);
	if ( dcx + halfWidth >= maxx ) {
		return;
	}
	dcx += halfWidth / stepfract;
	//printf("dcx=%lf\n", dcx );
}
void Solver::nudgeUp() {
	double halfHeight = (maxy - miny) / (2.0 * zoom);
	if ( dcy - halfHeight <= miny ) {
		return;
	}
	dcy -= halfHeight / stepfract;
	//printf("dcy=%lf\n", dcy );
}
void Solver::nudgeDown() {
	double halfHeight = (maxy - miny) / (2.0 * zoom);
	if ( dcy + halfHeight >= maxy ) {
		return;
	}
	dcy += halfHeight / stepfract;
	//printf("dcy=%lf\n", dcy );
}
static const double zoomStep = 1.41421356237309504880; // sqrt( 2 )
void Solver::zoomIn() {
	zoom *= zoomStep;
	//printf("zoom=%lf\n", zoom );
}
void Solver::zoomOut() {
	zoom /= zoomStep;
	//printf("zoom=%lf\n", zoom );
}
void Solver::centerOnPoint( int i ) {
	assert( i >= 0 );
	assert( i < gd->numPoints );
	dcx = gd->pos[i*2];
	dcy = gd->pos[i*2+1];
}
void Solver::zoomAll() {
	dcx = (maxx + minx) / 2.0;
	dcy = (miny + maxy) / 2.0;
	zoom = 1.0;
}


Adjacency::Adjacency()
: adjacency( NULL ), adjlen( 0 ), adjcap( 0 ) {
}
Adjacency::~Adjacency() {
	if ( adjacency != NULL ) {
		free( adjacency );
	}
}

void Solver::calculateAdjacency_r(Adjacency* it, int numPoints, int districts, const POPTYPE* winner,
						 const Node* nodes) {
	if ( it->adjcap == 0 ) {
		it->adjcap = districts * districts;
		it->adjacency = (POPTYPE*)malloc( sizeof(POPTYPE) * it->adjcap * 2 );
		assert(it->adjacency != NULL);
	}
	it->adjlen = 0;
	for ( int i = 0; i < numPoints; i++ ) {
		const Node* cur;
		POPTYPE cd;
		cd = winner[i];
		cur = nodes + i;
		for ( int n = 0; n < cur->numneighbors; n++ ) {
			POPTYPE od;
			od = winner[cur->neighbors[n]];
			if ( od != cd ) {
				int newadj = 1;
				for ( int l = 0; l < it->adjlen*2; l++ ) {
					if ( it->adjacency[l] == cd && it->adjacency[l^1] == od ) {
						newadj = 0;
						break;
					}
				}
				if ( newadj ) {
					assert( it->adjlen + 1 < it->adjcap );
					it->adjacency[it->adjlen*2  ] = cd;
					it->adjacency[it->adjlen*2+1] = od;
					it->adjlen++;
				}
			}
		}
	}
}
void Solver::calculateAdjacency(Adjacency* it) {
	calculateAdjacency_r(it, gd->numPoints, districts, winner, nodes);
}

/* scan from top to bottom, left to right to renumber districts. */
void Solver::californiaRenumber() {
	POS_TYPE* topPoints = new POS_TYPE[districts*2];
	int* sorti = new int[districts];
	int i;
	
	/* first, use sorti as flag, collect top point from each dist. */
	for ( i = 0; i < districts; i++ ) {
		sorti[i] = 0;
	}
	for ( i  = 0; i < gd->numPoints; i++ ) {
		POPTYPE d;
		POS_TYPE x, y;
		d = winner[i];
		x = gd->pos[i*2  ];
		y = gd->pos[i*2+1];
		if ( sorti[d] == 0 ) {
			topPoints[d*2  ] = x;
			topPoints[d*2+1] = y;
			sorti[d] = 1;
		} else if ( (y > topPoints[d*2+1]) ||
					((y == topPoints[d*2+1]) && (x < topPoints[d*2])) ) {
			topPoints[d*2  ] = x;
			topPoints[d*2+1] = y;
		}
	}
	for ( i = 0; i < districts; i++ ) {
		sorti[i] = i;
		//fprintf(stderr,"d%2d topleft=(%d,%d)\n", i, topPoints[i*2], topPoints[i*2+1] );
	}
	/* now sort the districts based on their topmost, leftmost point */
	int notdone = true;
	while ( notdone ) {
		notdone = false;
		for ( i = 0; i < districts-1; i++ ) {
			POS_TYPE* a;
			POS_TYPE* b;
			a = topPoints + (sorti[i]*2);
			b = topPoints + (sorti[i+1]*2);
			if ( (b[1] > a[1]) ||
				 ((b[1] == a[1]) && (b[0] < a[0])) ) {
				int t;
				t = sorti[i];
				sorti[i] = sorti[i+1];
				sorti[i+1] = t;
				notdone = true;
			}
		}
	}
	/* sorti is now the translation from old district to new district */
	if ( renumber != NULL ) {
		delete renumber;
	}
	renumber = new POPTYPE[districts];
	for ( i = 0; i < districts; i++ ) {
		fprintf(stderr,"d%2d topleft=(%d,%d)\n", sorti[i], topPoints[sorti[i]*2], topPoints[sorti[i]*2+1] );
		renumber[i] = sorti[i];
	}
	delete [] sorti;
	delete [] topPoints;
}

void Solver::nullRenumber() {
	if ( renumber != NULL ) {
		delete renumber;
	}
	renumber = new POPTYPE[districts];
	for ( POPTYPE i = 0; i < districts; i++ ) {
		renumber[i] = i;
	}
}

SolverStats::SolverStats()
	: generation( -1 ), avgPopDistToCenterOfDistKm( 0.0 ), kmppp( 0.0 ),
	popavg( 0.0 ), popstd( 0.0 ), popmin( HUGE_VAL ), popmax( -HUGE_VAL ), popmed( 0.0 ),
	mindist( -1 ), maxdist( -1 ), meddist( -1 ),
	nod( 0 ), nodpop( 0 ),
	next( NULL )
{}

// Reads file with arguments split by any whitespace according to isspace().
// \isspace() is excepted and preserved.
// Call free(argv) when done with args.
// argv[0] is always NULL, there is no app name.
// argv[argc] will also be NULL.
// Returns argc
int parseArgvFromFile(const char* filename, char*** argvP) {
	assert(argvP != NULL);
	mmaped mif;
	int argc = 0;
	char** argv;
	unsigned int pos = 0;
	int err = mif.open( filename );
	if ( err < 0 ) {
		perror(filename);
		return -1;
	}
	
	char* f = (char*)(mif.data);
	// count parts
	enum { text, space, escaped } state = space;
	for ( pos = 0; pos < mif.mmapsize; ++pos ) {
		if ( isspace(f[pos]) ) {
			if ( state == text ) {
				state = space;
			} else if ( state == escaped ) {
				state = text;
			} // else eat extra space
		} else if ( f[pos] == '\\' ) {
			if ( state != escaped ) {
				state = escaped;
			} else {
				state = text;
			}
		} else {
			// text char
			if ( state == space ) {
				argc++;
			}
			state = text;
		}
	}
	argv = (char**)malloc( (sizeof(char*) * (argc + 2)) + mif.mmapsize );
	if ( argv == NULL ) {
		fprintf(stderr, "malloc(%zu) fails\n", (sizeof(char*) * (argc + 2)) + mif.mmapsize );
		mif.close();
		return -1;
	}
	argv[0] = NULL;
	char* opos = (char*)(argv + (argc + 2));
	int ci = 0;
	state = space;
	for ( pos = 0; pos < mif.mmapsize; ++pos ) {
		if ( isspace(f[pos]) ) {
			if ( state == text ) {
				*opos = '\0';
				opos++;
				state = space;
			} else if ( state == escaped ) {
				*opos = f[pos];
				opos++;
				state = text;
			} // else eat extra space
		} else if ( f[pos] == '\\' ) {
			if ( state == space ) {
				ci++;
				argv[ci] = opos;
				state = escaped;
			} else if ( state != escaped ) {
				state = escaped;
			} else {
				*opos = f[pos];
				opos++;
				state = text;
			}
		} else {
			// text char
			if ( state == space ) {
				ci++;
				argv[ci] = opos;
			}
			*opos = f[pos];
			opos++;
			state = text;
		}
	}
	assert(ci == argc);
	argv[ci] = NULL;
	*argvP = argv;
	return argc;
}

bool Solver::nonProgressGiveup() const {
	if ((recentKmpp == NULL) || (recentSpread == NULL)) {
		return false;
	}
	double kmppVarFraction = (recentKmpp->max() - recentKmpp->min()) / recentKmpp->last();
	double spreadVarFraction = (recentSpread->max() - recentSpread->min()) / districtPopTarget;
	return (kmppVarFraction < recentKmppGiveupFraction) && (spreadVarFraction < recentSpreadGiveupFraction);
}
