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

#include "District2.h"
#include "Solver.h"
#include "Node.h"
#include "GrabIntermediateStorage.h"


District2Set::District2Set(Solver* sovIn)
	: DistrictSet(sovIn), grabdebug(NULL) {}

char* District2Set::debugText() {
	if ( grabdebug == NULL ) return NULL;
	return grabdebug->debugText();
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
	for ( POPTYPE d = 0; d < districts; d++ ) {
		dists[d].recalc( sov, d );
	}
}

int District2Set::step() {
	bool sorting;
	int err = 0;
#if 1
	if ( grabdebug == NULL ) {
		grabdebug = new GrabIntermediateStorage(sov);
	}
#endif
	if ( grabdebug != NULL ) {
		grabdebug->clear();
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
	if ( (sov->gencount % 1000) == 0 ) {
		for ( POPTYPE d = 0; d < districts; d++ ) {
			dists[d].recalc( sov, d );
		}
	}
	District2::step();
	District2::randomFactor = 0.5 * (cos( sov->gencount / 1000.0 ) + 1.0);
        // FIXME TODO ensure contiguiity
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

District2::District2() : numNodes( 0 ),
	edgelistLen( 0 ), edgelistCap( DISTRICT_EDGEISTLEN_DEFAULT ),
	pop( 0.0 ), wx( 0.0 ), wy( 0.0 ), distx( 0.0 ), disty( 0.0 )
{
	edgelist = (int*)malloc( sizeof(int) * DISTRICT_EDGEISTLEN_DEFAULT );
	assert(edgelist != NULL);
	for ( int i = 0; i < edgelistCap; i++ ) {
		edgelist[i] = -1;
	}
}

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
	edgelist[edgelistLen] = n;
	edgelistLen++;
}

int District2::add( Solver* sov, int n, POPTYPE dist ) {
	Node* nodes;
	POPTYPE* pit;
	double x;
	double y;
	double npop;
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
	if ( edgelistLen == edgelistCap ) {
		int nelc = edgelistCap * 2 + 7;
		edgelist = (int*)realloc( edgelist, sizeof(int) * nelc );
		assert(edgelist != NULL);
		edgelistCap = nelc;
	}
	edgelist[edgelistLen] = n;
	edgelistLen++;
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

#if USE_EDGE_LOOP
	int startnode = en->prev->nodeIndex;
	int endnode = en->next->nodeIndex;
#warning FIXME remove()
	printf("search from %6d to %6d\n", startnode, endnode );
	path = bfhead = bftail = new BreadthFirstNode( startnode );
	do {
		nit = nodes + bfhead->nodeIndex;
		printf("search node %6d from dist %d\n", bfhead->nodeIndex, pit[bfhead->nodeIndex] );
		for ( i = 0; i < nit->numneighbors; i++ ) {
			int ni;
			ni = nit->neighbors[i];
			printf("\t%6d dist=%d\n", ni, pit[ni] );
			if ( pit[ni] == dist ) {
				BreadthFirstNode* tbf = new BreadthFirstNode( ni, bfhead );
				bftail->next = tbf;
				bftail = tbf;
			}
			if ( ni == endnode ) {
				bfhead = bftail;
				goto traceNewEdge;
			}
		}
		bfhead = bfhead->next;
	} while ( 1 );
traceNewEdge:
	// follow the backpointers from bfhead to path
	EdgeNode* cen = en->next;
	while ( bfhead->prev != path ) {
		EdgeNode* nen;
		nen = newEdgeNode();
		nen->next = cen;
		cen->prev = nen;
		nen->nodeIndex = bfhead->nodeIndex;
		bfhead = bfhead->prev;
	}
	cen->prev = en->prev;
	en->prev->next = cen;
	deleteEdgeNode( en );
#else
	//int startnode = 0;
	//int endnode = 0;
#endif
	return 0;
}

#define unsetDist( d, i ) dists[(d)].remove( sov, (i), (d), gd->pos[(i)*2], gd->pos[(i)*2+1], gd->pop[i] )

double District2::edgeRelativeDistanceFactor = 1.2;
double District2::odEdgeRelativeDistanceFactor = 1.0;
double District2::neighborRatioFactor = 1.0;
double District2::popRatioFactor = 1.4;
double District2::lockStrengthFactor = 0.1;//1.0;
double District2::randomFactor = 0.1;
double District2::popFieldFactor = 0.001;

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
	double odEdgeRelativeDistance = 0.4;	// higher better
	double popRatio = -0.9;					// lower better
	double neighborRatio = 1.0;				// higher better
	double lockStrength = (d2set->lock[nein]/5.0 + 1.0); // lower better
	if ( odi != NODISTRICT ) {
		District2* od;
		od = (District2*)&(dists[odi]);
		//dx = bx - od->distx;
		//dy = by - od->disty;
		//tm = sqrt( (dx * dx) + (dy * dy) );
		if ( od->edgelistLen <= 1 ) {
			odEdgeRelativeDistance = 0.0;
		} else {
			odEdgeRelativeDistance = tm / od->edgeMeanR - 1.0;
		}
		//tm = 0.0;
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
		neighborRatio = thisn - othern;
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
	if ( gsdebug ) {
		printf("grabScore:  erd: %f*%f=%f\n", edgeRelativeDistance, edgeRelativeDistanceFactor, edgeRelativeDistance * edgeRelativeDistanceFactor );
		printf("grabScore: oerd: %f*%f=%f\n", odEdgeRelativeDistance, odEdgeRelativeDistanceFactor, odEdgeRelativeDistance * odEdgeRelativeDistanceFactor );
		printf("grabScore:   nr: %f*%f=%f\n", neighborRatio, neighborRatioFactor, neighborRatio * neighborRatioFactor );
		printf("grabScore: popr: %f*%f=%f\n", popRatio, popRatioFactor, popRatio * popRatioFactor );
		printf("grabScore: lock: %f*%f=%f\n", lockStrength, lockStrengthFactor, lockStrength * lockStrengthFactor );
		printf("grabScore: popf: %f*%f=%f\n", popField, popFieldFactor, popField * popFieldFactor );
		printf("grabScore: rand: %f*%f=%f\n", d2dr, randomFactor, d2dr * randomFactor );
	}
	if ( (d2set->grabdebug != NULL) &&
			((sov->debugDistrictNumber < 0) || (sov->debugDistrictNumber == d)) ) {
		d2set->grabdebug->set(nein, edgeRelativeDistance, odEdgeRelativeDistance,
							popRatio, neighborRatio, lockStrength, popField);
	}

	return edgeRelativeDistance * edgeRelativeDistanceFactor
		- odEdgeRelativeDistance * odEdgeRelativeDistanceFactor
		- neighborRatio * neighborRatioFactor
		+ popRatio * popRatioFactor
		+ lockStrength * lockStrengthFactor
		+ popField * popFieldFactor
		+ d2dr * randomFactor;
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
		printf("warning: district %d cannot grab, (%10g,%10g)\n", d, distx, disty );
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
	//double maxm = -HUGE_VAL;
	int mi = -1;
	const GeoData* gd = sov->gd;
#if USE_EDGE_LOOP
	EdgeNode* en = edgelistRoot;
	do {
		double dx, dy, tm;
		int eln = en->nodeIndex;
		dx = gd->pos[eln*2  ] - distx;
		dy = gd->pos[eln*2+1] - disty;
		tm = sqrt( (dx * dx) + (dy * dy) );// * tin.pointattributelist[nein*2];
		if ( tm  > maxm ) {
			maxm = tm;
			mi = eln;
		}
		en = en->next;
	} while ( en != edgelistRoot );
#endif
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
#if 0
	int err;
	err = ::write( fd, &edgelistLen, sizeof(edgelistLen) );
	if ( err < 0 ) { return err; } else { toret += err; }
	err = ::write( fd, edgelist, sizeof(*edgelist)*edgelistLen );
	if ( err < 0 ) { return err; } else { toret += err; }
#endif
	return toret;
}
int District2::read( int fd, Node* nodes, POPTYPE* pit, double* xy, double* popRadii, POPTYPE dist ) {
	int toret = 0;
#if 0
	int err;
	err = ::read( fd, &edgelistLen, sizeof(edgelistLen) );
	if ( err < 0 ) { return err; } else { toret += err; }
	if ( edgelistLen > edgelistCap ) {
		edgelist = (EdgeNode*)realloc( edgelist, sizeof(EdgeNode) * edgelistLen * 2 + 1 );
	}
	err = ::read( fd, edgelist, sizeof(*edgelist)*edgelistLen );
	if ( err < 0 ) { return err; } else { toret += err; }

	pop = 0.0;
	wx = 0.0;
	wy = 0.0;
#endif
	return toret;
}

#if 0
#define recalcdbprintf() printf("d %2d pop %6g wx %9g wy %9g distx %9g disty %9g\n", dist, pop, wx, wy, distx, disty )
#else
#define recalcdbprintf()
#endif

void District2::recalc( Solver* sov, POPTYPE dist ) {
	int nodecount = 0;
	recalcdbprintf();
	pop = 0.0;
#if READ_INT_AREA
	area = 0.0;
#endif
	wx = 0.0;
	wy = 0.0;
	edgelistLen = 0;
	for ( int n = 0; n < sov->gd->numPoints; n++ ) if ( sov->winner[n] == dist ) {
		double npop;
		double x, y;
		Node* tn;

		nodecount++;

		x = sov->gd->pos[(n)*2];
		y = sov->gd->pos[(n)*2+1];
		npop = sov->gd->pop[n];

		pop += npop;
#if READ_INT_AREA
		uint32_t narea = sov->gd->area[n];
		//assert(narea >= 0);
		area += narea;
		wx += narea * x;
		wy += narea * y;
#elif UNWEIGHTED_CENTER
		wx += x;
		wy += y;
#else
		wx += npop * x;
		wy += npop * y;
#endif
		
		tn = sov->nodes + n;
		for ( int i = 0; i < tn->numneighbors; i++ ) {
			if ( sov->winner[tn->neighbors[i]] != dist ) {
				addEdge( n );
				break;
			}
		}
	}
#if READ_INT_AREA
	distx = wx / area;
	disty = wy / area;
#elif UNWEIGHTED_CENTER
	distx = wx / (numNodes+1);
	disty = wy / (numNodes+1);
#else
	distx = wx / pop;
	disty = wy / pop;
#endif
	recalcdbprintf();
	assert( nodecount == numNodes );
}

double District2::centerX() {
	return distx;
}
double District2::centerY() {
	return disty;
}

