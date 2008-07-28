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

#include "Solver.h"
#include "LinearInterpolate.h"
#include "GrabIntermediateStorage.h"
#include "Node.h"
#include "tiger/mmaped.h"
#include "NearestNeighborDistrictSet.h"

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

struct DistrictSetFactory {
	DistrictSet* (*factory)(Solver*);
	const char* name;
};

DistrictSetFactory districtSetFactories[] = {
	{ NearestNeighborDistrictSetFactory, "Nearest Neighbor" },
	{ District2SetFactory, "Grab Solver" },
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


static char oldDefaultInputName[] = "NM_zcta5";

Solver::Solver() :
	districts( 5 ), totalpop( 0.0 ), districtPopTarget( 0.0 ),
	nodes( NULL ), allneigh( NULL ), winner( NULL ),
	districtSetFactory( NULL ), dists( NULL ),
	inputname( oldDefaultInputName ), generations( -1 ),
	dumpname( NULL ),
	loadname( NULL ),
	initMode( initWithNewDistricts ),
	solutionLogPrefix( NULL ), solutionLogInterval( 100 ), solutionLogCountdown( 0 ),
#if WITH_PNG
	pngLogPrefix( NULL ), pngLogInterval( 100 ), pngLogCountdown( 0 ),
#endif
	statLog( NULL ), statLogInterval( 100 ), statLogCountdown( 0 ),
	distfname( NULL ), coordfname( NULL ),
#if WITH_PNG
	pngname( NULL ), pngWidth( 1000 ), pngHeight( 1000 ),
#endif
	geoFact( openZCTA ),
	/*sorti( NULL ),*/
#if READ_DOUBLE_POS
	minx( HUGE_VAL ), miny( HUGE_VAL ), maxx( -HUGE_VAL ), maxy( -HUGE_VAL ),
#elif READ_INT_POS
	minx( INT_MAX ), miny( INT_MAX ), maxx( INT_MIN ), maxy( INT_MIN ),
#endif
	viewportRatio( 1.0 ), gencount( 0 ), blaf( NULL ),
	showLinks( 0 ),
	renumber( NULL ),
	_point_draw_array( NULL ), 
	lastGenDrawn( -1 ), vertexBufferObject(0), colorBufferObject(0), linesBufferObject(0),
	drawHistoryPos( 0 ),
	maxSpreadFraction( 1000.0 ), maxSpreadAbsolute( 9.0e9 )
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
	if ( dists != NULL ) {
		delete dists;
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
}

#if HAVE_PROTOBUF
int writeToProtoFile(Solver* sov, const char* filename);
int readFromProtoFile(Solver* sov, const char* filename);

// doesn't do anything, just a tag to switch on in Solver::load()
GeoData* protobufGeoDataTag( char* inputname ) {
	assert(0);
	return NULL;
}

int Solver::writeProtobuf( const char* fname ) {
	writeToProtoFile(this, fname);
}
#endif

void Solver::load() {
#if HAVE_PROTOBUF
	if ( geoFact == protobufGeoDataTag ) {
		int err = readFromProtoFile(this, inputname);
		if (err < 0) {
			return;
		}
	} else
#endif
	{
		gd = geoFact( inputname );
		gd->load();
		if ( geoFact == openBin ) {
			readLinksBin();
		} else {
			readLinksFile();
		}
	}
	
	if ( districts <= 0 ) {
		int tdistricts = gd->numDistricts();
		if ( tdistricts > 1 ) {
			districts = tdistricts;
		} else {
			districts = districts * -1;
		}
	}

	minx = gd->minx;
	maxx = gd->maxx;
	miny = gd->miny;
	maxy = gd->maxy;
	totalpop = gd->totalpop;
	dcx = (maxx + minx) / 2.0;
	dcy = (miny + maxy) / 2.0;
	printf("minx %0.6lf, miny  %0.6lf, maxx %0.6lf, maxy %0.6lf\n", (double)minx, (double)miny, (double)maxx, (double)maxy );
	zoom = 1.0;
	districtPopTarget = totalpop / districts;
}

void Solver::readLinksFile() {
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
	numEdges = linksFile.sb.st_size / sizeof_linkLine;
	edgeData = new int32_t[numEdges*2];
	char buf[14];
	buf[13] = '\0';
	int j = 0;
	for ( unsigned int i = 0 ; i < numEdges; i++ ) {
		uint64_t tubid;
		memcpy( buf, ((caddr_t)linksFile.data) + sizeof_linkLine*i, 13 );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2  ] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2  ] < 0 ) {
			printf("ubid %lld => index %d\n", tubid, edgeData[j*2] );
			continue;
		}
		memcpy( buf, ((caddr_t)linksFile.data) + sizeof_linkLine*i + 13, 13 );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2+1] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2+1] < 0 ) {
			printf("ubid %lld => index %d\n", tubid, edgeData[j*2+1] );
			continue;
		}
		j++;
	}
	numEdges = j;
	linksFile.close();
	free( linkFileName );
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
#if READ_INT_POP
	p += sizeof(int)*gd->numPoints;
#endif
#if READ_INT_AREA
	p += sizeof(uint32_t)*gd->numPoints;
#endif
#if READ_UBIDS
	p += sizeof(uint32_t)*gd->numPoints;
	p += sizeof(uint64_t)*gd->numPoints;
#endif
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

void Solver::allocSolution() {
	winner = new POPTYPE[gd->numPoints];
	assert(winner != NULL);
	for ( int i = 0; i < gd->numPoints; i++ ) {
		winner[i] = NODISTRICT;
	}
	if ( districtSetFactory != NULL ) {
		dists = districtSetFactory(this);
	} else {
#if 1
		dists = new NearestNeighborDistrictSet(this);
#else
		dists = new District2Set(this);
#endif
	}
	assert(dists != NULL);
	dists->alloc(districts);
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
int Solver::loadZSolution( const char* filename ) {
	ZFILE_INT ti;
	uLongf unzlen;
	Bytef* zb;
	int readfd;
	int endianness = 0;
	
	readfd = open( filename, O_RDONLY, 0 );
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
#if 1
	dists->initFromLoadedSolution();
#else
	for ( POPTYPE d = 0; d < districts; d++ ) {
		dists[d].numNodes = 0;
	}
	for ( int i = 0; i < gd->numPoints; i++ ) {
		POPTYPE d;
		d = winner[i];
		if ( d == ERRDISTRICT || d == NODISTRICT ) {
			continue;
		}
		//assert( d >= 0 );
		if ( d >= districts ) {
			fprintf(stderr,"%s element %d is invalid district %d of %d districts\n", filename, i, d, districts );
			exit(1);
		} else {
			dists[d].numNodes++;
		}
	}
	for ( POPTYPE d = 0; d < districts; d++ ) {
		dists[d].recalc( this, d );
	}
#endif
	return close( readfd );
loadZFail:
	free( zb );
	close( readfd );
	return -1;
}


void Solver::initSolution() {
	dists->initNewRandomStart();
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
	dists->initFromLoadedSolution();
}

void Solver::init() {
}

#ifndef TRIANGLE_STEP
#define TRIANGLE_STEP 0
#endif

int Solver::step() {
	int err = dists->step();
	gencount++;
	return err;
}

#if 1
void Solver::printDistricts(const char* filename) {
	dists->print(filename);
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
	dists->getStats(stats);
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
	snprintf( cstr, clen, " %0.11lg Km/person\npopulation avg=%.0lf std=%0.9g\n", avgPopDistToCenterOfDistKm, popavg, popstd ); CCUP;
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
	unsigned char* data = (unsigned char*)malloc(pngWidth*pngHeight*3*sizeof(unsigned char) );
	unsigned char** rows = (unsigned char**)malloc(pngHeight*sizeof(unsigned char*) );
	assert( data != NULL );
	assert( rows != NULL );
	
	for ( int y = 0; y < pngHeight; y++ ) {
		rows[y] = data + (y*pngWidth*3);
	}
	memset( data, 0x0, pngWidth*pngHeight*3*sizeof(unsigned char) );

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
		row = data + (y*pngWidth*3);
		x = (int)ox;
		x *= 3;
		row[x  ] = color[0];//((unsigned char)( (((unsigned int)color[0]) * 3)/7 ));
		row[x+1] = color[1];//((unsigned char)( (((unsigned int)color[1]) * 3)/7 ));
		row[x+2] = color[2];//((unsigned char)( (((unsigned int)color[2]) * 3)/7 ));
	}
	
	myDoPNG( outname, rows, pngHeight, pngWidth );
	free( rows );
	free( data );
}
#endif

int Solver::handleArgs( int argc, char** argv ) {
	int argcout = 1;
	popRatioFactor.clear();
	popRatioFactor.setPoint( -1, District2::popRatioFactor );
	for ( int i = 1; i < argc; i++ ) {
		if ( ! strcmp( argv[i], "-i" ) ) {
			i++;
			inputname = argv[i];
		} else if ( ! strcmp( argv[i], "-U" ) ) {
			i++;
			inputname = argv[i];
			geoFact = openUf1;
		} else if ( ! strcmp( argv[i], "-B" ) ) {
			i++;
			inputname = argv[i];
			geoFact = openBin;
#if HAVE_PROTOBUF
		} else if ( ! strcmp( argv[i], "-P" ) ) {
			i++;
			inputname = argv[i];
			geoFact = protobufGeoDataTag;
#endif
		} else if ( ! strcmp( argv[i], "-g" ) ) {
			i++;
			generations = atoi( argv[i] );
#if 0
		} else if ( ! strcmp( argv[i], "-p" ) ) {
			i++;
			popsize = atoi( argv[i] );
#endif
		} else if ( ! strcmp( argv[i], "-d" ) ) {
			i++;
			districts = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "-o" ) ) {
			i++;
			dumpname = strdup( argv[i] );
		} else if ( (! strcmp( argv[i], "-r" )) ||
			    (! strcmp( argv[i], "--loadSolution")) ) {
			i++;
			loadname = strdup( argv[i] );
		} else if ( ! strcmp( argv[i], "--distout" ) ) {
			i++;
			distfname = strdup( argv[i] );
		} else if ( ! strcmp( argv[i], "--coordout" ) ) {
			i++;
			coordfname = strdup( argv[i] );
#if WITH_PNG
		} else if ( ! strcmp( argv[i], "--pngout" ) ) {
			i++;
			pngname = strdup( argv[i] );
		} else if ( ! strcmp( argv[i], "--pngW" ) ) {
			i++;
			pngWidth = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "--pngH" ) ) {
			i++;
			pngHeight = atoi( argv[i] );
#endif
		} else if ( ! strcmp( argv[i], "--statLog" ) ) {
			i++;
			statLog = fopen( argv[i], "w" );
			if ( statLog == NULL ) {
				perror( argv[i] );
				exit(1);
			}
		} else if ( ! strcmp( argv[i], "--sLog" ) ) {
			i++;
			solutionLogPrefix = strdup( argv[i] );
#if WITH_PNG
		} else if ( ! strcmp( argv[i], "--pLog" ) ) {
			i++;
			pngLogPrefix = strdup( argv[i] );
#endif
		} else if ( ! strcmp( argv[i], "--oldCDs" ) ) {
			initMode = initWithOldDistricts;
		} else if ( ! strcmp( argv[i], "--blankDists" ) ) {
			initMode = initWithNewDistricts;
		} else if ( ! strcmp( argv[i], "-q" ) ) {
			blaf = NULL;
		} else if ( ! strcmp( argv[i], "--popRatioFactor" ) ) {
			char* endp;
			double td;
			i++;
			td = strtod( argv[i], &endp );
			if ( endp == argv[i] ) {
				fprintf( stderr, "bogus popRatioFactor \"%s\"\n", argv[i] );
				exit(1);
			}
			popRatioFactor.setPoint(0, td);
			District2::popRatioFactor = td;
		} else if ( ! strcmp( argv[i], "--popRatioFactorEnd" ) ) {
			char* endp;
			double td;
			i++;
			td = strtod( argv[i], &endp );
			if ( endp == argv[i] ) {
				fprintf( stderr, "bogus popRatioFactorEnd \"%s\"\n", argv[i] );
				exit(1);
			}
			popRatioFactor.setPoint(gencount + generations, td);
		} else if ( ! strcmp( argv[i], "--popRatioFactorPoints" ) ) {
			i++;
			popRatioFactor.parse(argv[i]);
		} else if ( ! strcmp( argv[i], "--nearest-neighbor" ) ) {
			districtSetFactory = NearestNeighborDistrictSetFactory;
		} else if ( ! strcmp( argv[i], "--d2" ) ) {
			districtSetFactory = District2SetFactory;
		} else if ( ! strcmp( argv[i], "--maxSpreadFraction" ) ) {
			char* endp;
			double td;
			i++;
			td = strtod( argv[i], &endp );
			if ( endp == argv[i] ) {
				fprintf( stderr, "bogus maxSpreadFraction \"%s\"\n", argv[i] );
				exit(1);
			}
			maxSpreadFraction = td;
		} else if ( ! strcmp( argv[i], "--maxSpreadAbsolute" ) ) {
			char* endp;
			double td;
			i++;
			td = strtod( argv[i], &endp );
			if ( endp == argv[i] ) {
				fprintf( stderr, "bogus maxSpreadAbsolute \"%s\"\n", argv[i] );
				exit(1);
			}
			maxSpreadAbsolute = td;
		} else {
#if 0
			argv[argcout] = argv[i];
			argcout++;
#else
			fprintf( stderr, "%s: bogus arg \"%s\"\n", argv[0], argv[i] );
			exit(1);
#endif
		}
	}
	return argcout;
}

#if WITH_PNG
static char bestStdPng[] = "bestStd.png";
static char bestSpreadPng[] = "bestSpread.png";
static char bestKmppPng[] = "bestKmpp.png";
#endif

int Solver::main( int argc, char** argv ) {
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
	
	handleArgs( argc, argv );
	
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
		generations = gd->numPoints * 10 / districts;
	}
	SolverStats* bestStd = NULL;
	SolverStats* bestSpread = NULL;
	SolverStats* bestKmpp = NULL;
	POPTYPE* bestStdMap = (POPTYPE*)malloc( sizeof(POPTYPE) * gd->numPoints );
	POPTYPE* bestSpreadMap = (POPTYPE*)malloc( sizeof(POPTYPE) * gd->numPoints );
	POPTYPE* bestKmppMap = (POPTYPE*)malloc( sizeof(POPTYPE) * gd->numPoints );
	// don't count kmpp till half way, early solutions cheat.
	int bestKmppStart = gencount + (generations / 2);
	int genmax = gencount + generations;
	assert(bestStdMap != NULL);
	assert(bestSpreadMap != NULL);
	assert(bestKmppMap != NULL);
	while ( gencount < genmax ) {
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
		double spread = curst->popmax - curst->popmin;
		if ( (gencount > bestKmppStart) && (curst->nod == 0) &&
			 (spread < maxSpreadAbsolute) && ((spread / districtPopTarget) < maxSpreadFraction) ) {
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
		if ( statLog != NULL ) {
			if ( statLogCountdown == 0 ) {
				char ds[256];
				//getDistrictStats( ds, sizeof(ds) );
				curst->toString( ds, sizeof(ds) );
				fprintf( statLog, "generation: %d\n%s\n", gencount, ds );
				statLogCountdown = statLogInterval;
				fflush( statLog );
			}
			statLogCountdown--;
		}
		delete curst;
		if ( (gencount % 50) == 0 ) {
			printf("gen %d:\n", gencount );
			fflush(stdout);
		}
		step();
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
	: generation( -1 ), avgPopDistToCenterOfDistKm( 0.0 ),
	popavg( 0.0 ), popstd( 0.0 ), popmin( HUGE_VAL ), popmax( -HUGE_VAL ), popmed( 0.0 ),
	mindist( -1 ), maxdist( -1 ), meddist( -1 ),
	nod( 0 ), nodpop( 0 ),
	next( NULL )
{}
SolverStats::SolverStats( int geni, double pd, double pa, double ps, double pmi, double pma, double pme,
			   int mid, int mad, int med, int noDist, double noDistPop, SolverStats* n )
	: generation( geni ), avgPopDistToCenterOfDistKm( pd ),
	popavg( pa ), popstd( ps ), popmin( pmi ), popmax( pma ), popmed( pme ),
	mindist( mid ), maxdist( mad ), meddist( med ),
	nod( noDist ), nodpop( noDistPop ),
	next( n )
{}

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
