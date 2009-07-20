#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "Bitmap.h"
#include "District2.h"
#include "Solver.h"
#include "Node.h"
#include "GrabIntermediateStorage.h"
#include "StatThing.h"
#include "GeoData.h"


// TODO: optimization
// Recent run shows time going:
//	32.1% recalc()
//	31.1% grab() (28.5% grabScore())
//	24.1% fixupDistrictContiguity() (most of that in findContiguousGroups())
//        Running CA this gets up to 50% findContiguousGroups
//	10.6% getStats()

District2Set::District2Set(Solver* sovIn)
	: DistrictSet(sovIn), grabdebug(NULL),
	debugStats(NULL),
	fixupFrequency(1.0), fixupBucket(0.0) {
}

char* District2Set::debugText() {
	char* dstext = NULL;
	char* gdtext = NULL;
	if ( debugStats != NULL ) {
		dstext = debugStatsText();
	}
	if ( grabdebug != NULL ) {
		gdtext = grabdebug->debugText();
	}
	if (dstext != NULL) {
		if (gdtext != NULL) {
			char* out = (char*)malloc(strlen(dstext) + strlen(gdtext) + 2);
			strcpy(out, dstext);
			strcat(out, "\n");
			strcat(out, gdtext);
			free(dstext);
			free(gdtext);
			return out;
		} else {
			return dstext;
		}
	} else {
		return gdtext;
	}
}
enum {
	edgeRelativeDistanceIndex = 0,
	edgeRelativeDistanceFactorIndex,
	odEdgeRelativeDistanceIndex,
	odEdgeRelativeDistanceFactorIndex,
	neighborRatioIndex,
	neighborRatioFactorIndex,
	popRatioIndex,
	popRatioFactorIndex,
	lockStrengthIndex,
	lockStrengthFactorIndex,
	popFieldIndex,
	popFieldFactorIndex,
	currentRandomIndex,
	currentRandomFactorIndex,
	debugStatsLength
};
char* District2Set::debugStatsText() {
	static const size_t outLen = 4096;
	char* out = (char*)malloc(outLen);
	char* cur = out;
	size_t remaining = outLen;
	int err;
#define PRINT_LOG(x) err = snprintf(cur, remaining, "%23s: % 8g/% 8g/% 8g, *weight % 8g/% 8g/% 8g\n", #x, debugStats[x##Index].min(), debugStats[x##Index].average(), debugStats[x##Index].max(), debugStats[x##FactorIndex].min(), debugStats[x##FactorIndex].average(), debugStats[x##FactorIndex].max()); if (err >= 0) {cur += err; remaining -= err;} else {fprintf(stderr, "error printing %s\n", #x);}
	PRINT_LOG(edgeRelativeDistance);
	PRINT_LOG(odEdgeRelativeDistance);
	PRINT_LOG(neighborRatio);
	PRINT_LOG(popRatio);
	PRINT_LOG(lockStrength);
	PRINT_LOG(popField);
	PRINT_LOG(currentRandom);
	return out;
}
void District2Set::getStats(SolverStats* stats) {
	double minp = HUGE_VAL;
	int dmin = -1;
	double maxp = -HUGE_VAL;
	int dmax = -1;
	double ptot = 0.0;
	double popavg;
	double popvar = 0.0;
	for ( POPTYPE d = 0; d < districts; d++ ) {
		ptot += dists[d].pop;
		if ( dists[d].pop < minp ) {
			minp = dists[d].pop;
			dmin = d;
		}
		if ( dists[d].pop > maxp ) {
			maxp = dists[d].pop;
			dmax = d;
		}
	}
	popavg = ptot / districts;
	for ( POPTYPE d = 0; d < districts; d++ ) {
		double dp;
		dp = popavg - dists[d].pop;
		popvar += dp * dp;
	}
	POPTYPE median = sorti[districts/2];
	stats->poptotal = ptot;
	stats->popavg = popavg;
	stats->popstd = sqrt(popvar / (districts - 1.0));
	stats->popmin = minp;
	stats->popmax = maxp;
	stats->popmed = dists[median].pop;
	stats->mindist = dmin;
	stats->maxdist = dmax;
	stats->meddist = median;
	
	double nodpop = 0.0;
	int nod = 0;
	double moment = 0.0;
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	for ( int i = 0; i < gd->numPoints; i++ ) {
		if ( winner[i] == NODISTRICT ) {
			nod++;
#if READ_INT_POP
			nodpop += gd->pop[i];		
#endif
		} else {
#if READ_INT_POP && (READ_INT_POS || READ_DOUBLE_POS)
			double dx, dy;
			District2* cd;
			cd = &(dists[winner[i]]);
#if READ_INT_POS || READ_DOUBLE_POS
			dx = cd->distx - gd->pos[i*2  ];
			dy = cd->disty - gd->pos[i*2+1];
#else
#error "what?"
#endif
			moment += sqrt(dx * dx + dy * dy) * gd->pop[i];
#endif
		}
	}
	// earthradius_equatorial  6378136.49 m * 2 * Pi = 40075013.481 m earth circumfrence at equator
	// sum microdegrees / pop = avg microdegrees
	// avg microdegrees / ( 360000000 microdegrees per diameter ) = avg diameters of earth
	// avg diameters of earth * 40075.013481 = avg Km per person to center of dist
	// TODO: adjust for average latitude of region instead of assuming equatorial.
	double avgPopDistToCenterOfDistKm = ((moment/stats->poptotal)/360000000.0)*40075.013481;
	stats->nod = nod;
	stats->nodpop = nodpop;
	stats->avgPopDistToCenterOfDistKm = avgPopDistToCenterOfDistKm;
}

void District2Set::alloc(int size) {
	DistrictSet::alloc(size);
	lock = new unsigned char[sov->gd->numPoints];
	assert(lock != NULL);
	sorti = new POPTYPE[districts];
	assert(sorti != NULL);
	for ( POPTYPE d = 0; d < districts; d++ ) {
		sorti[d] = d;
	}
	dists = new District2[districts];
	for (int i = 0; i < districts; ++i) {
		they[i] = dists + i;
	}
}

void District2Set::initNewRandomStart() {
	int numPoints = sov->gd->numPoints;
	Node* nodes = sov->nodes;
	POPTYPE* winner = sov->winner;
	for ( POPTYPE d = 0; d < districts; d++ ) {
		int i;
		//i = dstep * d;
		do {
			i = random() % numPoints;
		} while ( winner[i] != NODISTRICT );
		//setDist( d , i );
		dists[d].addFirst( sov, i, d );
#if 0
		dists[d].calcMoment( nodes, winner, tin.pointlist, tin.pointattributelist, d );
		if ( dists[d].moment < minMoment ) {
			minMoment = dists[d].moment;
		}
		if ( dists[d].moment > maxMoment ) {
			maxMoment = dists[d].moment;
		}
		double pe;
		pe = fabs( dists[d].pop - districtPopTarget );
		if ( pe < minPoperr ) {
			minPoperr = pe;
		}
		if ( pe > maxPoperr ) {
			maxPoperr = pe;
		}
#endif
	}
#if 01
	// Kickstart the growth process.
	// Add every neighbor of an initial node not claimed by some other district.
	bool notdone = true;
	while ( notdone ) {
		notdone = false;
		for ( POPTYPE d = 0; d < districts; d++ ) {
			int i;
			//i = dstep * d;
			i = dists[d].edgelist[0];
			Node* n = nodes + i;
			for ( int ni = 0; ni < n->numneighbors; ni++ ) {
				int nii;
				nii = n->neighbors[ni];
				if ( winner[nii] == NODISTRICT ) {
					//notdone = true;
					sov->setDist( d, nii );
					break;
				}
			}
		}
	}
#endif
}

void District2Set::initFromLoadedSolution() {
	for ( POPTYPE d = 0; d < districts; d++ ) {
		dists[d].numNodes = 0;
	}
	for ( int i = 0; i < sov->gd->numPoints; i++ ) {
		POPTYPE d;
		d = sov->winner[i];
		if ( d == ERRDISTRICT || d == NODISTRICT ) {
			continue;
		}
		//assert( d >= 0 );
		if ( d >= districts ) {
			fprintf(stderr,"element %d is invalid district %d of %d districts\n", i, d, districts );
			exit(1);
		} else {
			dists[d].numNodes++;
		}
	}
	recalc();
}

int District2Set::step() {
	bool sorting;
	int err = 0;
#if 0
	if ( grabdebug == NULL ) {
		// grabdebug is horribly slow. use only if you really need it.
		grabdebug = new GrabIntermediateStorage(sov);
	}
#endif
#if 0
	// compile in for debugging
	if ( debugStats == NULL ) {
		debugStats = new StatThing[debugStatsLength];
	}
#endif
	if ( grabdebug != NULL ) {
		// This is horribly slow. use grabdebug only if you really need it.
		grabdebug->clear();
	}
	if (debugStats != NULL) {
		for (int i = 0; i < debugStatsLength; ++i) {
			debugStats[i].clear();
		}
	}
#if TRIANGLE_STEP
	for ( int runl = districts; runl > 0; runl-- ) {
#else
#define runl districts
#endif
		sorting = true;
	while ( sorting ) {
		sorting = false;
		for ( POPTYPE d = 1; d < districts; d++ ) {
			if ( dists[sorti[d]].pop < dists[sorti[d-1]].pop ) {
				POPTYPE ts;
				ts = sorti[d];
				sorti[d] = sorti[d-1];
				sorti[d-1] = ts;
				sorting = true;
			}
		}
	}
#if 01
	for ( int i = 0; i < sov->gd->numPoints; i++ ) {
		if ( lock[i] != 0 ) {
			lock[i]--;
		}
	}
#else
	memset( lock, 0, sizeof(*lock) * gd->numPoints );
#endif
	// grab for whichever district has the lowest population
	for ( int i = 0; i < runl; i++ ) {
		POPTYPE d;
#if (!(TRIANGLE_STEP)) && 0
		// always grab for the lowest population district
		POPTYPE ze = 0;
		while ( (ze < districts - 1) && (dists[sorti[ze]].pop > dists[sorti[ze+1]].pop) ) {
			POPTYPE ts;
			ts = sorti[ze];
			sorti[ze] = sorti[ze+1];
			sorti[ze+1] = ts;
			ze++;
		}
		d = sorti[0];
#else
		d = sorti[i];
#endif
		if ( dists[d].pop < sov->districtPopTarget ) {
			err = dists[d].grab( this, d );
		} else if ( sov->gencount & 1 ) {
			err = dists[d].grab( this, d );
			//dists[d].disown( this, d );
		}
		if ( err < 0 ) {
			return err;
		}
	}
#if TRIANGLE_STEP
	}
#endif
	if (sov->gencount > 500) {
		if (fixupFrequency > 0.0) {
			if (fixupBucket >= 1.0) {
				fixupDistrictContiguity();
				recalc();
				fixupBucket -= 1.0;
			}
			fixupBucket += fixupFrequency;
		}
	}
	District2::step();
	currentRandomFactor = District2::randomFactor * (cos( sov->gencount / 1000.0 ) + 1.0);
	return err;
}


#if !defined(NDEBUG) && 0
#ifdef assert
#undef assert
#endif
int bolson_assert( const char* file, int line, const char* assertion );
#define assert(n) ((n) ? 0 : bolson_assert( __FILE__, __LINE__, #n ))
#include <pthread.h>
pthread_cond_t bolson_assert_cond;
pthread_mutex_t bolson_assert_mutex;
pthread_t assertReleaseThread;
volatile int bolson_assert_release_flag = 0;
void* assertReleaseThreadFoo( void* arg ) {
	while ( 1 ) {
		sleep(1);
		if ( bolson_assert_release_flag ) {
			bolson_assert_release_flag = 0;
			pthread_mutex_lock( &bolson_assert_mutex );
			pthread_cond_signal( &bolson_assert_cond );
		}
	}
}
int bolson_assert_init(void) {
	int err;
	err = pthread_cond_init( &bolson_assert_cond, NULL );
	if ( err != 0 ) {
		return err;
	}
	err = pthread_mutex_init( &bolson_assert_mutex, NULL );
	if ( err != 0 ) {
		return err;
	}
	err = pthread_create( &assertReleaseThread, NULL, assertReleaseThreadFoo, NULL );
	return err;
}
int bolson_assert_initted = bolson_assert_init();
int bolson_assert( const char* file, int line, const char* assertion ) {
	fprintf( stderr, "%s:%d assertion failed \"%s\"\n", file, line, assertion );
	pthread_mutex_lock( &bolson_assert_mutex );
	pthread_cond_wait( &bolson_assert_cond, &bolson_assert_mutex );
}
#endif

#ifndef DISTRICT_EDGEISTLEN_DEFAULT
#define DISTRICT_EDGEISTLEN_DEFAULT 137
#endif

District2::District2() : AbstractDistrict(0.0,0.0), numNodes( 0 ),
	edgelistLen( 0 ), edgelistCap( DISTRICT_EDGEISTLEN_DEFAULT ),
	pop( 0.0 ), wx( 0.0 ), wy( 0.0 )//distx( 0.0 ), disty( 0.0 )
{
	edgelist = (int*)malloc( sizeof(int) * DISTRICT_EDGEISTLEN_DEFAULT );
	assert(edgelist != NULL);
	for ( int i = 0; i < edgelistCap; i++ ) {
		edgelist[i] = -1;
	}
	validate();
}

#if ! NO_D2_VALIDATE
void District2::validate(const char* file, int line) {
	assert(edgelist != NULL);
	assert(edgelistCap >= DISTRICT_EDGEISTLEN_DEFAULT);
	assert(edgelistLen <= edgelistCap);
}
#endif

#ifndef UNWEIGHTED_CENTER
#define UNWEIGHTED_CENTER 01
#endif

#if READ_INT_AREA
#define addFirstToCenter() area = sov->gd->area[n];_addFirstToCenter( x, y, sov->gd->area[n] )
#elif UNWEIGHTED_CENTER
#define addFirstToCenter() _addFirstToCenter( x, y )
#else
#define addFirstToCenter() _addFirstToCenter( x, y, npop )
#endif

void District2::addFirst( Solver* sov, int n, POPTYPE dist ) {
	Node* nodes;
	POPTYPE* pit;
	double x;
	double y;
	double npop;
	validate();
	nodes = sov->nodes;
	pit = sov->winner;
	x = sov->gd->pos[(n)*2];
	y = sov->gd->pos[(n)*2+1];
	npop = sov->gd->pop[n];
	//printf("District::add(,, n=%d, dist=%d,,, npop=%9.0lf)\n", n, dist, npop );
	numNodes++;
	assert( ! isnan( distx ) );
	assert( ! isnan( disty ) );
	assert( ! isinf( distx ) );
	assert( ! isinf( disty ) );
	assert( pop == 0.0 );

	//printf("district %d has zero pop at add", dist );
	pop = npop;
	
	addFirstToCenter();
	assert( ! isnan( distx ) );
	assert( ! isnan( disty ) );
	assert( ! isinf( distx ) );
	assert( ! isinf( disty ) );
	pit[n] = dist;
	Node* nit = nodes + n;
	nit->order = 0;
	assert(edgelistLen == 0);
	edgelist[edgelistLen] = n;
	edgelistLen++;
	validate();
}

int District2::add( Solver* sov, int n, POPTYPE dist ) {
	Node* nodes;
	POPTYPE* pit;
	double x;
	double y;
	double npop;
	validate();
	nodes = sov->nodes;
	pit = sov->winner;
	x = sov->gd->pos[(n)*2];
	y = sov->gd->pos[(n)*2+1];
	npop = sov->gd->pop[n];
	//printf("District::add(,, n=%d, dist=%d,,, npop=%9.0lf)\n", n, dist, npop );
	assert( ! isnan( distx ) );
	assert( ! isnan( disty ) );
	assert( ! isinf( distx ) );
	assert( ! isinf( disty ) );
	if ( pop == 0.0 ) {
		//printf("district %d has zero pop at add", dist );
		pop = npop;
		addFirstToCenter();
	} else {
		pop += npop;
#if READ_INT_AREA
		uint32_t narea = sov->gd->area[n];
		area += narea;
		wx += narea * x;
		wy += narea * y;
		distx = wx / area;
		disty = wy / area;
#elif UNWEIGHTED_CENTER
		wx += x;
		wy += y;
		distx = wx / (numNodes+1);
		disty = wy / (numNodes+1);
#else
		wx += npop * x;
		wy += npop * y;
		distx = wx / pop;
		disty = wy / pop;
#endif
	}
	assert( ! isnan( distx ) );
	assert( ! isnan( disty ) );
	assert( ! isinf( distx ) );
	assert( ! isinf( disty ) );
	pit[n] = dist;
	Node* nit = nodes + n;
	numNodes++;
	// update order values
	nit->order = 0;
	for ( int ni = 0; ni < nit->numneighbors; ni++ ) {
		int nni = nit->neighbors[ni];
		if ( pit[nni] == dist ) {
			nodes[nni].order++;
			if ( nodes[nni].order == nodes[nni].numneighbors ) {
				// yank it from edge list.
				removeEdge( nni );
			}
			nit->order++;
		}
	}
	if ( nit->order < nit->numneighbors ) {
		addEdge( n );
	}
	validate();
	return 0;
}

class BreadthFirstNode {
public:
	int nodeIndex;
	BreadthFirstNode* next;
	BreadthFirstNode* prev;
	BreadthFirstNode( int nin ) : nodeIndex( nin ), next( NULL ), prev( NULL ) {}
	BreadthFirstNode( int nin, BreadthFirstNode* pin ) : nodeIndex( nin ), next( NULL ), prev( pin ) {}
	void deleteList() {
		BreadthFirstNode* tn;
		while ( next != NULL ) {
			tn = next->next;
			next->next = NULL;
			delete next;
			next = tn;
		}
		delete this;
	}
};

void District2::addEdge( int n ) {
	validate();
	if ( edgelistLen == edgelistCap ) {
		int nelc = edgelistCap * 2 + 7;
#if 0
		int* tel = (int*)malloc( sizeof(int) * nelc );
		assert(tel != NULL);
		for ( int i = 0; i < edgelistLen; ++i ) {
			tel[i] = edgelist[i];
		}
		printf("expand edgelist, %d -> %d, %p -> %p\n",
			edgelistCap, nelc,
			edgelist, tel);
		free(edgelist);
		edgelist = tel;
#else
		edgelist = (int*)realloc( edgelist, sizeof(int) * nelc );
		assert(edgelist != NULL);
#endif
		edgelistCap = nelc;
	}
	validate();
	edgelist[edgelistLen] = n;
	edgelistLen++;
	validate();
}
void District2::removeEdge( int n ) {
	for ( int i = 0; i < edgelistLen; i++ ) {
		if ( edgelist[i] == n ) {
			edgelistLen--;
			edgelist[i] = edgelist[edgelistLen];
			return;
		}
	}
}

int District2::remove( /*Node* nodes, POPTYPE* pit, */Solver* sov, int n, POPTYPE dist, double x, double y, double npop ) {
	Node* nodes = sov->nodes;
	POPTYPE* pit = sov->winner;
	int i;
	pop -= npop;
	numNodes--;
	assert( ! isnan( distx ) );
	assert( ! isnan( disty ) );
	assert( ! isinf( distx ) );
	assert( ! isinf( disty ) );
#if READ_INT_AREA
	uint32_t narea = sov->gd->area[n];
	wx -= narea * x;
	wy -= narea * y;
	area -= narea;
	if ( area == 0 ) {
		printf("district %d has zero pop at remove", dist );
		distx = wx;
		disty = wy;
	} else {
		distx = wx / area;
		disty = wy / area;
	}
#elif UNWEIGHTED_CENTER
	wx -= x;
	wy -= y;
	distx = wx / numNodes;
	disty = wy / numNodes;
#else
	wx -= npop * x;
	wy -= npop * y;
	if ( pop == 0 ) {
		printf("district %d has zero pop at remove", dist );
		distx = wx;
		disty = wy;
	} else {
		distx = wx / pop;
		disty = wy / pop;
	}
#endif
	assert( ! isnan( distx ) );
	assert( ! isnan( disty ) );
	assert( ! isinf( distx ) );
	assert( ! isinf( disty ) );
	Node* nit = nodes + n;
	removeEdge( n );
	// decrease order of ex-neighbors
	//BreadthFirstNode* bfhead;
	//BreadthFirstNode* bftail;
	//BreadthFirstNode* path;

	//printf("removing node %6d from dist %d\n", n, dist );
	for ( i = 0; i < nit->numneighbors; i++ ) {
		int ni;
		ni = nit->neighbors[i];
		//printf("\t%6d dist=%d\n", ni, pit[ni] );
		if ( pit[ni] == dist ) {
			if ( nodes[ni].order == nodes[i].numneighbors ) {
				nodes[ni].order--;
				addEdge( ni );
			} else {
				nodes[ni].order--;
			}
		}
	}

	return 0;
}

#define unsetDist( d, i ) dists[(d)].remove( sov, (i), (d), gd->pos[(i)*2], gd->pos[(i)*2+1], gd->pop[i] )

double District2::edgeRelativeDistanceFactor = 1.2;
double District2::odEdgeRelativeDistanceFactor = 1.0;
double District2::neighborRatioFactor = 0.3;
double District2::popRatioFactor = 0.5;
double District2::lockStrengthFactor = 0.1;//1.0;
double District2::randomFactor = 0.1;
double District2::popFieldFactor = 0.001;
const char* District2::parameterNames[] = {
"distance relative to edge",
"distance relative to edge (OD)",
"neighbor ratio",
"pop ratio",
"lock strength",
"random",
"pop field",
"fixup frequency",
"do debug stats",
NULL,
};
static const int kNumParameterNames = 9;

double District2::lastAvgPopField = HUGE_VAL;
double District2::nextPopFieldSum = 0.0;
int District2::numPopFieldsSummed = 0;

void District2::step() {
	lastAvgPopField = nextPopFieldSum / numPopFieldsSummed;
	nextPopFieldSum = 0.0;
	numPopFieldsSummed = 0;
}

static inline double d2drand() {
	return ((double)random()) / ((double)0x7fffffff) - 0.5;
}



/** score a potential grab. lower score better to grab. */
double District2::grabScore( District2Set* d2set, POPTYPE d, int nein
#if GRAB_SCORE_DEBUG
							 , int gsdebug
#else
#define gsdebug (0)
#endif
							 ) {
	Solver* sov = d2set->sov;
	Node* nodes = sov->nodes;
	POPTYPE* winner = sov->winner;
	District2* dists = ((District2Set*)(sov->dists))->dists;
	const GeoData* gd = sov->gd;

	POPTYPE odi;
	Node* on;
	odi = winner[nein];
	on = nodes + nein;
	if ( odi == d ) {
		if ( gsdebug ) {
			printf("grabScore: other district is this district\n");
		}
		return HUGE_VAL;
	}

	/* FACTORS TO CONSIDER
		
		Edge Relative Distance (ERD)	1.0 = neutral, lower better
		ERD to other district			1.0 = neutral, higher better
		neighbors of this dist
		neighbors of other dist
		(this neigh - od neigh) / neigh	0.0 = neutral, higher better
		this pop / od pop				1.0 = neutral, lower better
		node is unclaimed or not		unclaimed = good
		lock age						recently locked = bad, lower better
		
		variables get offset compensated to have 0.0 neutral. This contributes to the large district decision to not grab when all options are bad.
		*/
	
	// not already part of district d and would not bisect another district.
	// candidate for adding to this district...
	double tm;
	double bx, by;
	double dx, dy;
	bx = gd->pos[nein*2  ];
	by = gd->pos[nein*2+1];
	dx = bx - distx;
	dy = by - disty;
	tm = sqrt( (dx * dx) + (dy * dy) );
	//tm /= 8.0;
	double edgeRelativeDistance;
	if ( edgelistLen <= 1 ) {
		edgeRelativeDistance = 0.0;
	} else {
		edgeRelativeDistance = tm / edgeMeanR - 1.0;
	}
	// defaults paint a somewhat favorable condition for a no-district node
	double odEdgeRelativeDistance = -0.4;	// lower better
	double popRatio = -0.9;					// lower better
	double neighborRatio = -1.0;			// lower better
	double lockStrength = (d2set->lock[nein]/5.0 + 1.0); // lower better
	if ( odi != NODISTRICT ) {
		District2* od;
		od = (District2*)&(dists[odi]);
		if ( od->edgelistLen <= 1 ) {
			return HUGE_VAL; // don't take other district's last node.
//			odEdgeRelativeDistance = 0.0;
		} else {
			dx = bx - od->distx;
			dy = by - od->disty;
			tm = sqrt( (dx * dx) + (dy * dy) );
			odEdgeRelativeDistance = (tm / od->edgeMeanR - 1.0) * -1.0;
		}
		// tm should be higher if taking it from a district is costly to that district. this is to prevent thrashing and promote district mobility.
		// equivalently, taking a block from a bigger district should be easier by making tm smaller.
		if ( od->pop <= 1 ) {
			popRatio = 10.0;
		} else if ( pop > sov->districtPopTarget && pop > od->pop ) {
			// large districts CANNOT take from smaller
			if ( gsdebug ) {
				printf("grabScore: large district forbidden to poach from smaller\n");
			}
			return HUGE_VAL;
		} else {
#if 01
			if ( pop > od->pop ) {
				// more strongly discourage taking from smaller districts
				popRatio = 3 * (pop - od->pop) / od->pop;
			} else {
				popRatio = (pop - od->pop) / od->pop;
			}
#else
			popRatio = (pop - od->pop) / od->pop;
#endif
			//popRatio -= 1.0;
		}
		int thisn, othern;
		thisn = 0;
		othern = 0;
		for ( int nni = 0; nni < on->numneighbors; nni++ ) {
			int nnid;
			nnid = winner[on->neighbors[nni]];
			if ( nnid == d ) {
				thisn++;
			} else if ( nnid == odi ) {
				othern++;
			}
		}
		neighborRatio = othern - thisn;
		neighborRatio /= on->numneighbors;
	}
#ifndef FIELD_VS_AVG_POP
#define FIELD_VS_AVG_POP 0
#endif
#if FIELD_VS_AVG_POP
	double avgDistPop = 0;
	for ( POPTYPE di = 0; di < sov->districts; di++ ) {
		avgDistPop += dists->pop;
	}
	avgDistPop /= sov->districts;
#endif
	double popField = 0.0;
	for ( POPTYPE di = 0; di < sov->districts; di++ ) {
		double rs;
		District2* od;
		od = (District2*)&(dists[di]);
		dx = bx - od->distx;
		dy = by - od->disty;
		rs = (dx * dx) + (dy * dy);
#if FIELD_VS_AVG_POP
		popField += (avgDistPop - od->pop) / rs;
#else
		popField += (sov->districtPopTarget - od->pop) / rs;
#endif
	}
	nextPopFieldSum += popField;
	numPopFieldsSummed++;
	popField /= lastAvgPopField;

	double d2dr = d2drand();
	if ( d2set->debugStats != NULL ) {
#define LOG_STAT(x) d2set->debugStats[x##Index].log(x); d2set->debugStats[x##FactorIndex].log(x * x##Factor)
		LOG_STAT(edgeRelativeDistance);
		LOG_STAT(odEdgeRelativeDistance);
		LOG_STAT(neighborRatio);
		LOG_STAT(popRatio);
		LOG_STAT(lockStrength);
		LOG_STAT(popField);
		d2set->debugStats[currentRandomIndex].log(d2dr);
		d2set->debugStats[currentRandomFactorIndex].log(d2dr * d2set->currentRandomFactor);
	}
	if ( gsdebug ) {
		printf("grabScore:  erd: %f*%f=%f\n", edgeRelativeDistance, edgeRelativeDistanceFactor, edgeRelativeDistance * edgeRelativeDistanceFactor );
		printf("grabScore: oerd: %f*%f=%f\n", odEdgeRelativeDistance, odEdgeRelativeDistanceFactor, odEdgeRelativeDistance * odEdgeRelativeDistanceFactor );
		printf("grabScore:   nr: %f*%f=%f\n", neighborRatio, neighborRatioFactor, neighborRatio * neighborRatioFactor );
		printf("grabScore: popr: %f*%f=%f\n", popRatio, popRatioFactor, popRatio * popRatioFactor );
		printf("grabScore: lock: %f*%f=%f\n", lockStrength, lockStrengthFactor, lockStrength * lockStrengthFactor );
		printf("grabScore: popf: %f*%f=%f\n", popField, popFieldFactor, popField * popFieldFactor );
		printf("grabScore: rand: %f*%f=%f\n", d2dr, d2set->currentRandomFactor, d2dr * d2set->currentRandomFactor );
	}
	if ( (d2set->grabdebug != NULL) &&
			((sov->debugDistrictNumber < 0) || (sov->debugDistrictNumber == d)) ) {
		d2set->grabdebug->set(nein, edgeRelativeDistance, odEdgeRelativeDistance,
							popRatio, neighborRatio, lockStrength, popField);
	}

	return edgeRelativeDistance * edgeRelativeDistanceFactor
		+ odEdgeRelativeDistance * odEdgeRelativeDistanceFactor
		+ neighborRatio * neighborRatioFactor
		+ popRatio * popRatioFactor
		+ lockStrength * lockStrengthFactor
		+ popField * popFieldFactor
		+ d2dr * d2set->currentRandomFactor;
}

#ifndef INTRA_GRAB_MULTITHREAD
#define INTRA_GRAB_MULTITHREAD 0
#endif
#if INTRA_GRAB_MULTITHREAD
#include "PreThread.h"

class GrabTaskParams {
public:
	// (dist,sov,d,stride) should be same for each task, offset should be unique
	District2* dist;    // parameter for grab()
	Solver* sov;        // parameter for grab()
	POPTYPE d;          // parameter for grab()
	int stride;         // constant for all threads
	int offset;         // constant for this thread, use it to initialize grabTask local
	double minm;        // return value for grab()
	int mi;             // return value for grab()
	GrabTaskParams( District2* distIn, Solver* sovIn, POPTYPE dIn, int strideIn, int offsetIn )
		: dist( distIn ), sov( sovIn ), d( dIn ), stride( strideIn ), offset( offsetIn )
	{}
	GrabTaskParams()
		: dist( NULL ), sov( NULL ), d( (POPTYPE)-1 ), stride( -1 ), offset( -1 )
	{}
	
	void run( PreThread* );
};
static void* grabTask( void* arg ) {
	GrabTaskParams* p = (GrabTaskParams*)arg;
	p->dist->grabTask( p );
	return p;
}
void GrabTaskParams::run( PreThread* p ) {
	p->run( grabTask, this );
}
void District2::grabTask( GrabTaskParams* p ) {
	Solver* sov = p->sov;
	POPTYPE d = p->d;
	int stride = p->stride;
	int offset = p->offset;
	double minm = 9e9;
	int mi = -1;
	int counter = offset;
	Node* nodes = sov->nodes;

	// get score for every neighbor of each edge node
	for ( int i = 0; i < edgelistLen; i++ ) {
		Node* cdn;
		cdn = nodes + edgelist[i];
		for ( int nei = 0; nei < cdn->numneighbors; nei++ ) {
			if ( counter != 0 ) {
				counter--;
				continue;
			} else {
				counter = stride;
			}
			int nein;
			nein = cdn->neighbors[nei];
			double tm = grabScore( sov, d, nein );
			if ( tm < minm ) {
				minm = tm;
				mi = nein;
			}
		}
	}
	
	p->minm = minm;
	p->mi = mi;
}

int numDistrict2Threads = 2;

static PreThread* ptPool = NULL;
static GrabTaskParams* ptParms = NULL;

static inline void setupPtPool(void) {
	if ( ptPool != NULL ) {
		return;
	}
	ptPool = new PreThread[numDistrict2Threads];
	ptParms = new GrabTaskParams[numDistrict2Threads];
	for ( int i = 0; i < numDistrict2Threads; i++ ) {
		ptParms[i].offset = i;
		ptParms[i].stride = numDistrict2Threads;
	}
}
#endif


int District2::grab( District2Set* d2set, POPTYPE d ) {
	Solver* sov = d2set->sov;
	Node* nodes = sov->nodes;
	POPTYPE* winner = sov->winner;
	District2* dists = ((District2Set*)(sov->dists))->dists;
	const GeoData* gd = sov->gd;
	//double districtPopTarget = sov->districtPopTarget;
	// best neighbor to claim
	double minm = 9e9;
	int mi = -1, i;
	
	edgeMeanR = 0.0;
	// measure average distance of edge from center
	for ( i = 0; i < edgelistLen; i++ ) {
		int eln = edgelist[i];
		double dx, dy;
		dx = gd->pos[eln*2  ] - distx;
		dy = gd->pos[eln*2+1] - disty;
		edgeMeanR += (/*edgeRScrap[eli] = */sqrt( (dx * dx) + (dy * dy) ));
	}
	edgeMeanR /= edgelistLen;
#if INTRA_GRAB_MULTITHREAD
	setupPtPool();
	for ( int i = 0; i < numDistrict2Threads; i++ ) {
		ptParms[i].d = d;
		ptParms[i].sov = sov;
		ptParms[i].dist = this;
		ptParms[i].run( ptPool + i );
	}
	for ( int i = 0; i < numDistrict2Threads; i++ ) {
		GrabTaskParams* t;
		t = (GrabTaskParams*)ptPool[i].join();
		if ( (mi == -1) || (t->minm < minm) ) {
			mi = t->mi;
			minm = t->minm;
		}
	}
#else
	// get score for every neighbor of each edge node
	for ( i = 0; i < edgelistLen; i++ ) {
		Node* cdn;
		cdn = nodes + edgelist[i];
		for ( int nei = 0; nei < cdn->numneighbors; nei++ ) {
			int nein;
			nein = cdn->neighbors[nei];
			double tm = grabScore( d2set, d, nein );
			if ( tm < minm ) {
				minm = tm;
				mi = nein;
			}
		}
	}
#endif

#ifndef GRABSPEW
#define GRABSPEW 0
#endif
	if ( mi != -1 ) {
		if ((pop > sov->districtPopTarget) && (minm > 0.0)) {
			return 0;
		}
		POPTYPE odi = winner[mi];
		int err;
		if ( odi != NODISTRICT ) {
			err = unsetDist( odi, mi );
			if ( err < 0 ) {
				sov->centerOnPoint( mi );
				return err;
			}
		}
		err = sov->setDist( d, mi );
		if ( err < 0 ) {
			return err;
		}
		d2set->lock[mi] = 60;
#if GRABSPEW
		if ( odi != NODISTRICT ) {
			//printf("%d (pop %9.0lf) grabs %d (%9.0lf) from %hhu (%9.0lf)\n", d, pop, mi, gd->pop[mi], odi, dists[odi].pop );
			printf("%d (pop %9.0lf) grabs %d (%9d) from %hhu (%9.0lf)\n", d, pop, mi, gd->pop[mi], odi, dists[odi].pop );
		} else {
			//printf("%d (pop %9.0lf) grabs %d (%9.0lf) (unclaimed)\n", d, pop, mi, gd->pop[mi] );
		}
#endif
	} else if ( pop < sov->districtPopTarget ) {
#if GRABSPEW || 01
		printf("warning: district %d cannot grab, (%10g,%10g) %d nodes, %d on edge\n",
			d, distx, disty, numNodes, edgelistLen );
		for ( i = 0; i < edgelistLen; i++ ) {
			Node* cdn;
			int eln;
			eln = edgelist[i];
			cdn = nodes + eln;
			double ex, ey;
			ex = gd->pos[eln*2];
			ey = gd->pos[eln*2+1];
			printf("edgenode %d: (%10lf,%10lf) %d neighbors\n", eln, ex, ey, cdn->numneighbors );
			for ( int nei = 0; nei < cdn->numneighbors; nei++ ) {
				int nein;
				POPTYPE odi;
				Node* on;
				nein = cdn->neighbors[nei];
				odi = winner[nein];
				on = nodes + nein;
				printf("\tnei %2d: node %6d: score=%10g (%10g, %10g) th=%10g district %d, order %d\n",
					   nei, nein, grabScoreDebug( d2set, d, nein ), (double)(gd->pos[nein*2]),
					   (double)(gd->pos[nein*2+1]), atan2( gd->pos[nein*2+1]-ey, gd->pos[nein*2]-ex ),
					   odi, on->order );
			}
		}
		printf("\n");
		exit(1);
#endif
	}
	return 0;
}

/* disown the furthest */
int District2::disown( Solver* sov, POPTYPE d ) {
	int mi = -1;
	const GeoData* gd = sov->gd;
	if ( mi != -1 ) {
		POPTYPE* winner = sov->winner;
		District2* dists = ((District2Set*)(sov->dists))->dists;
		int err = unsetDist( d, mi );
		if ( err < 0 ) {
			sov->centerOnPoint( mi );
			return err;
		}
		winner[mi] = NODISTRICT;
	}
	return 0;
}

int District2::write( int fd ) {
	int toret = 0;
	return toret;
}
int District2::read( int fd, Node* nodes, POPTYPE* pit, double* xy, double* popRadii, POPTYPE dist ) {
	int toret = 0;
	return toret;
}

namespace {
class ContiguousGroup {
public:
	// POPTYPE d; // implicit in container or index in ContiguousGroup[]
	int start;
	int count;
	ContiguousGroup* next;
	
	ContiguousGroup()
	: start(-1), count(-1), next(NULL)
	{
	}
	ContiguousGroup(int start_, ContiguousGroup* next_)
	: start(start_), count(1), next(next_)
	{
	}
	// Deletes next chain off this so that delete[] deletes deeply.
	~ContiguousGroup() {
		while ( next != NULL ) {
			ContiguousGroup* t = next->next;
			next->next = NULL;
			delete next;
			next = t;
		}
	}
};

static void bfSearchDistrict(int* bfsearchq, POPTYPE d, Node* nodes,
		POPTYPE* winner, Bitmap& hit, ContiguousGroup* cur, int numPoints) {
	// breadth-first search
	int bfin = 1;
	int bfout = 0;
	while ( bfin != bfout ) {
		Node* n = nodes + bfsearchq[bfout];
		bfout = (bfout + 1) % numPoints;
		for ( int ni = 0; ni < n->numneighbors; ++ni ) {
			int nin;
			nin = n->neighbors[ni];
			if ( (winner[nin] == d) && (! hit.test(nin)) ) {
				bfsearchq[bfin] = nin;
				bfin = (bfin + 1) % numPoints;
				hit.set(nin);
				cur->count++;
			}
		}
	}
}

#if 0
// parallelized breadth-first-search attempts dropped in favor of parallelizing runallstates.pl
#include <pthread.h>
class BFSearch {
public:
	Bitmap& hit;
	POPTYPE* winner;
	Node* nodes;
	POPTYPE d;
	int* bfsearchq;
	int bfin;
	int bfout;
	int searchers;
	int numPoints;
	ContiguousGroup* cur;
	
	pthread_mutex_t lock;
	
	BFSearch(Bitmap& b) : hit(b) {
		pthread_mutex_init(&lock, NULL);
	}
	~BFSearch() {
		pthread_mutex_destroy(&lock);
	}
	
	void searchThread() {
		Node* n;
		bool iAmSearching = true;
		pthread_mutex_lock(&lock);
		searchers++;
		pthread_mutex_unlock(&lock);
		while ( true ) {
			pthread_mutex_lock(&lock);
			if ( bfin == bfout ) {
				if ( iAmSearching ) {
					searchers--;
					iAmSearching = false;
				}
				if ( searchers == 0 ) {
					pthread_mutex_unlock(&lock);
					return;
				} else {
					pthread_mutex_unlock(&lock);
					continue;
				}
			}
			if ( ! iAmSearching ) {
				searchers++;
			}
			n = nodes + bfsearchq[bfout];
			bfout = (bfout + 1) % numPoints;
			pthread_mutex_unlock(&lock);
			for ( int ni = 0; ni < n->numneighbors; ++ni ) {
				int nin;
				nin = n->neighbors[ni];
				if ( winner[nin] == d ) {
					pthread_mutex_lock(&lock);
					if ( ! hit.test(nin) ) {
						bfsearchq[bfin] = nin;
						bfin = (bfin + 1) % numPoints;
						hit.set(nin);
						cur->count++;
					}
					pthread_mutex_unlock(&lock);
				}
			}
		}
	}
};
void* BFSearch_searchThread(void* it) {
	static_cast<BFSearch*>(it)->searchThread();
	return NULL;
}
static void bfSearchDistrictThreaded(int* bfsearchq, POPTYPE d, Node* nodes,
		POPTYPE* winner, Bitmap& hit, ContiguousGroup* cur, int numPoints) {
	int bfin = 1;
	int bfout = 0;
	while ( bfin != bfout ) {
		Node* n = nodes + bfsearchq[bfout];
		bfout = (bfout + 1) % numPoints;
		for ( int ni = 0; ni < n->numneighbors; ++ni ) {
			int nin;
			nin = n->neighbors[ni];
			if ( (winner[nin] == d) && (! hit.test(nin)) ) {
				bfsearchq[bfin] = nin;
				bfin = (bfin + 1) % numPoints;
				hit.set(nin);
				cur->count++;
			}
		}
	}
}
#endif

// Returns: true if need to continue
static bool findContiguousGroups(Solver* sov, Bitmap& hit, ContiguousGroup* groups, int* bfsearchq) {
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;
	Node* nodes = sov->nodes;
	bool anyBroken = false;

	for ( int i = 0; i < numPoints; ++i ) {
		if ( winner[i] == NODISTRICT ) {
			// If there are any unclaimed, it's too early to bother.
			return false; //goto fixup_end;
		} else if ( ! hit.test(i) ) {
			POPTYPE d = winner[i];
			bfsearchq[0] = i;
			hit.set(i);
			ContiguousGroup* cur;
			if (groups[d].start == -1) {
				// original array set
				groups[d].start = i;
				groups[d].count = 1;
				cur = &(groups[d]);
			} else {
				anyBroken = true;
				cur = new ContiguousGroup( i, groups[d].next );
				assert(cur != NULL);
				groups[d].next = cur;
			}
			bfSearchDistrict( bfsearchq, d, nodes, winner, hit, cur, numPoints );
		}
	}
	return anyBroken;
}

static int disownSmallGroups(Solver* sov, Bitmap& hit, ContiguousGroup* groups, int* bfsearchq, int districts) {
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;
	Node* nodes = sov->nodes;
	int pointsRepod = 0;

	for ( POPTYPE d = 0; d < districts; ++d ) {
		if ( groups[d].next != NULL ) {
			// district d is not contiguous. dissolve all but largest fragment.
			ContiguousGroup* largest = &(groups[d]);
			ContiguousGroup* cur = largest->next;
			int groupstat = 1;
			int nodestat = largest->count;
			while ( cur != NULL ) {
				groupstat++;
				nodestat += cur->count;
				if ( cur->count > largest->count ) {
					largest = cur;
				}
				cur = cur->next;
			}
			//fprintf(stderr, "dist %d, %d nodes in %d groups, largest->count=%d\n", d, nodestat, groupstat, largest->count);
			cur = &(groups[d]);
			while ( cur != NULL ) {
				if ( cur != largest ) {
					// disown fragment
					int countCheck = 1;
					bfsearchq[0] = cur->start;
					int bfout = 0;
					int bfin = 1;
					pointsRepod++;
					hit.clear(cur->start);
					winner[cur->start] = NODISTRICT;
					while ( bfin != bfout ) {
						int nn = bfsearchq[bfout];
						Node* n = nodes + nn;
						bfout = (bfout + 1) % numPoints;
						for ( int ni = 0; ni < n->numneighbors; ++ni ) {
							int nin;
							nin = n->neighbors[ni];
							if ( (winner[nin] == d) && hit.test(nin) ) {
								bfsearchq[bfin] = nin;
								bfin = (bfin + 1) % numPoints;
								pointsRepod++;
								countCheck++;
								hit.clear(nin);
								winner[nin] = NODISTRICT;
							}
						}
					}
					assert(countCheck == cur->count);
				}
				cur = cur->next;
			}
		}
	}
	return pointsRepod;
}
}  // local namespace

void District2Set::assignReposessedNodes(int* bfsearchq, int pointsRepod) {
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;
	Node* nodes = sov->nodes;

	// In this block, bfsearchq is repurposed to hold orphaned node indecies.
	int repox = 0;
	for ( int i = 0; i < numPoints; ++i ) {
		if ( winner[i] == NODISTRICT ) {
			bfsearchq[repox] = i;
			repox++;
		}
	}
	assert(repox == pointsRepod);
	while ( repox > 0 ) {
		bool any = false;
		for ( int ri = 0; ri < repox; ++ri ) {
			int i = bfsearchq[ri];
			Node* n = nodes + i;
			double bx, by;
			bx = gd->pos[i*2  ];
			by = gd->pos[i*2+1];
			POPTYPE closest = NODISTRICT;
			double crcr = 9e9;
			double dx, dy;
			for ( int ni = 0; ni < n->numneighbors; ++ni ) {
				int nin;
				nin = n->neighbors[ni];
				POPTYPE d = winner[nin];
				if ( d != NODISTRICT ) {
					if ( closest == NODISTRICT ) {
						closest = d;
						dx = (*this)[d].centerX() - bx;
						dy = (*this)[d].centerY() - by;
						crcr = ((dx * dx) + (dy * dy));
					} else if ( d != closest ) {
						dx = (*this)[d].centerX() - bx;
						dy = (*this)[d].centerY() - by;
						double tr = ((dx * dx) + (dy * dy));
						if ( tr < crcr ) {
							crcr = tr;
							closest = d;
						}
					}
				}
			}
			if ( closest != NODISTRICT ) {
				any = true;
				winner[i] = closest;
				repox--;
				if ( repox == 0 ) {
					break;
				}
				bfsearchq[ri] = bfsearchq[repox];
			}
		}
		assert(any);
	}
}

// Using as high as 48% of total time, optimizing this is a big win.
void District2Set::fixupDistrictContiguity() {
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;

	Bitmap hit(numPoints);
	hit.zero();
	int* bfsearchq = new int[numPoints];
	assert(bfsearchq != NULL);
	ContiguousGroup* groups = new ContiguousGroup[districts];
	assert(groups != NULL);
	int pointsRepod = 0;
	
	// For each point, if we haven't already seen it, breadth-first-search
	// out from it noting a ContiguousGroup for its district.
	bool keepGoing = findContiguousGroups( sov, hit, groups, bfsearchq );
	if ( ! keepGoing ) {
		goto fixup_end;
	}
	// For each district, if there are multiple ContiguousGroups, disown all
	// the points in all but the largest group.
	pointsRepod = disownSmallGroups( sov, hit, groups, bfsearchq, districts );
	// If points were reposessed from small ContiguousGroups, assign them to
	// nearby districts.
	if ( pointsRepod != 0 ) {
		//fprintf(stderr, "changed %d points fixing contiguity at t=%d\n", pointsRepod, sov->gencount);
		assignReposessedNodes( bfsearchq, pointsRepod );
	}
	
	fixup_end:
	delete [] groups;
	delete [] bfsearchq;
}

// TODO merge recalc()/getStats() ?
// Maybe not. It looks like looping over all the points twice is unavoidable.
// Once to recalculate district centers. Again to measure distance to centers.
// Better accounting could avoid recalc() entirely?
void District2Set::recalc() {
	const GeoData* gd = sov->gd;
	POPTYPE* winner = sov->winner;
	//int numPoints = gd->numPoints;
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].pop = 0.0;
#if READ_INT_AREA
		dists[d].area = 0.0;
#endif
		dists[d].wx = 0.0;
		dists[d].wy = 0.0;
		dists[d].edgelistLen = 0;
		assert(dists[d].edgelistCap >= DISTRICT_EDGEISTLEN_DEFAULT);
	}
	for ( int n = 0; n < gd->numPoints; n++ ) {
		POPTYPE d = winner[n];
		double npop;
		double x, y;
		Node* tn;

		if ( d == NODISTRICT ) {
			continue;
		}
		x = gd->pos[(n)*2];
		y = gd->pos[(n)*2+1];
		npop = gd->pop[n];

		dists[d].pop += npop;
#if READ_INT_AREA
		uint32_t narea = gd->area[n];
		//assert(narea >= 0);
		dists[d].area += narea;
		dists[d].wx += narea * x;
		dists[d].wy += narea * y;
#elif UNWEIGHTED_CENTER
		dists[d].wx += x;
		dists[d].wy += y;
#else
		dists[d].wx += npop * x;
		dists[d].wy += npop * y;
#endif
		
		tn = sov->nodes + n;
		for ( int i = 0; i < tn->numneighbors; i++ ) {
			if ( sov->winner[tn->neighbors[i]] != d ) {
				dists[d].addEdge( n );
				break;
			}
		}
	}
	for ( POPTYPE d = 0; d < districts; ++d ) {
#if READ_INT_AREA
		dists[d].distx = dists[d].wx / dists[d].area;
		dists[d].disty = dists[d].wy / dists[d].area;
#elif UNWEIGHTED_CENTER
		dists[d].distx = dists[d].wx / (dists[d].numNodes+1);
		dists[d].disty = dists[d].wy / (dists[d].numNodes+1);
#else
		dists[d].distx = dists[d].wx / dists[d].pop;
		dists[d].disty = dists[d].wy / dists[d].pop;
#endif
	}
}

int District2Set::numParameters() {
	return kNumParameterNames;
}
// Return NULL if index too high or too low.
const char* District2Set::getParameterLabelByIndex(int index) {
	if ((index < 0) || (index >= kNumParameterNames)) {
		return NULL;
	}
	return District2::parameterNames[index];
}
double District2Set::getParameterByIndex(int index) {
	if ((index < 0) || (index >= kNumParameterNames)) {
		return NAN;
	}
	switch (index) {
	case 0:
		return District2::edgeRelativeDistanceFactor;
	case 1:
		return District2::odEdgeRelativeDistanceFactor;
	case 2:
		return District2::neighborRatioFactor;
	case 3:
		return District2::popRatioFactor;
	case 4:
		return District2::lockStrengthFactor;
	case 5:
		return District2::randomFactor;
	case 6:
		return District2::popFieldFactor;
	case 7:
		return fixupFrequency;
	case 8:
		return (debugStats == NULL) ? 0.0 : 1.0;
	default:
		assert(0);
	}
	assert(0);
	return NAN;
}
void District2Set::setParameterByIndex(int index, double value) {
	if ((index < 0) || (index >= kNumParameterNames)) {
		return;
	}
	switch (index) {
		case 0:
			District2::edgeRelativeDistanceFactor = value;
			break;
		case 1:
			District2::odEdgeRelativeDistanceFactor = value;
			break;
		case 2:
			District2::neighborRatioFactor = value;
			break;
		case 3:
			District2::popRatioFactor = value;
			break;
		case 4:
			District2::lockStrengthFactor = value;
			break;
		case 5:
			District2::randomFactor = value;
			break;
		case 6:
			District2::popFieldFactor = value;
			break;
		case 7:
			fixupFrequency = value;
			if (fixupFrequency > 1.0) {
				fixupFrequency = 1.0;
			}
			break;
		case 8:
			if (value > 0.5) {
				if (debugStats == NULL) {
					debugStats = new StatThing[debugStatsLength];
				}
			} else {
				if (debugStats != NULL) {
					delete debugStats;
					debugStats = NULL;
				}
			}
			break;
		default:
			assert(0);
	}
}
