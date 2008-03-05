#include "NearestNeighborDistrictSet.h"
#include "districter.h"
#include "Solver.h"
#include "AbstractDistrict.h"
#include <assert.h>
#include <math.h>

// TODO(bolson): It's hill-climbing. It needs jitter and annealing to get out of local minima.

/* random() uniform over 0..0x7fffffff, divide by 0x3fffffff.0 =>
 * uniform over 0 .. 2.0, subtract 1.0 => uniform -1.0 .. 1.0 */
static inline double d2drand() {
	return (((double)random()) / ((double)0x3fffffff)) - 1.0;
}
/* return uniform -max .. max, modulated by an offset cosine (0..1, with period)
*/
inline double jitter(double max, double period, double t) {
	return d2drand() * 0.5 * (cos( (t * M_PI * 2.0) / period ) + 1.0);
}

class NearestNeighborDistrict : public AbstractDistrict {
public:
	double x, y;
	double weight;
	double dx, dy;
	double poperr;
	double areax, areay, areasum;
	int pop;
	NearestNeighborDistrict() : weight(1.0) {}
	NearestNeighborDistrict(double xin, double yin)
		: x(xin), y(yin), weight(1.0) {}
	void set(double xin, double yin) {
		x = xin;
		y = yin;
	}
	virtual int add( Solver* sov, int n, POPTYPE dist ) { assert(0); return 0; };
	virtual int remove( Solver* sov, int n, POPTYPE dist,
		double x, double y, double npop ) { assert(0); return 0; };
	virtual double centerX() { return x; };
	virtual double centerY() { return y; };
};

NearestNeighborDistrictSet::NearestNeighborDistrictSet(Solver* sovIn)
	: DistrictSet(sovIn),
        posMaxJitter(kDefaultPosMaxJitter),
        weightMaxJitter(kDefaultWeightMaxJitter),
        jitterPeriod(kDefaultJitterPeriod)
{}
NearestNeighborDistrictSet::~NearestNeighborDistrictSet() {
	if ( dists != NULL ) {
		delete [] dists;
	}
}

void NearestNeighborDistrictSet::alloc(int size) {
	DistrictSet::alloc(size);
	dists = new NearestNeighborDistrict[size];
	assert(dists != NULL);
	for (int i = 0; i < districts; ++i) {
		they[i] = dists + i;
	}
}
void NearestNeighborDistrictSet::initNewRandomStart() {
	int numPoints = sov->gd->numPoints;
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	for ( POPTYPE d = 0; d < districts; d++ ) {
		int i;
		do {
			i = random() % numPoints;
		} while ( winner[i] != NODISTRICT );
		double bx, by;
		bx = gd->pos[i*2  ];
		by = gd->pos[i*2+1];
		dists[d].set(bx, by);
	}
	setWinners();
}
void NearestNeighborDistrictSet::initFromLoadedSolution() {
	fprintf(stderr,"WARNING NearestNeighborDistrictSet::initFromLoadedSolution not implemented, initting random\n");
	initNewRandomStart();
}

#if NEAREST_NEIGHBOR_MULTITHREAD
#include "PreThread.h"
class SetWinnersThreadArgs {
public:
	int stride;
	int offset;
	Solver* sov;
        NearestNeighborDistrictSet* nnds;
};

const int numSetWinnersThreads = 3;

static PreThread* ptPool = NULL;
static SetWinnersThreadArgs* ptParms = NULL;

static inline void setupPtPool(void) {
	if ( ptPool != NULL ) {
		return;
	}
	ptPool = new PreThread[numSetWinnersThreads];
	ptParms = new SetWinnersThreadArgs[numSetWinnersThreads];
	for ( int i = 0; i < numSetWinnersThreads; i++ ) {
		ptParms[i].offset = i;
		ptParms[i].stride = numSetWinnersThreads;
	}
}

static void* setWinnersThread( void* arg ) {
	SetWinnersThreadArgs* args = (SetWinnersThreadArgs*)arg;
        return args->nnds->setWinnersThread( args );
}
void* NearestNeighborDistrictSet::setWinnersThread(SetWinnersThreadArgs* args) {
	POPTYPE* winner = args->sov->winner;
	const GeoData* gd = args->sov->gd;
	int numPoints = gd->numPoints;
	for ( int i = args->offset; i < numPoints; i += args->stride ) {
		double bx, by;
		bx = gd->pos[i*2  ];
		by = gd->pos[i*2+1];
		POPTYPE closest = 0;
		double crcr;
		double dx, dy;
		dx = dists[0].x - bx;
		dy = dists[0].y - by;
		crcr = ((dx * dx) + (dy * dy)) * dists[0].weight;
		for ( POPTYPE d = 1; d < districts; ++d ) {
			dx = dists[d].x - bx;
			dy = dists[d].y - by;
			double tr = ((dx * dx) + (dy * dy)) * dists[d].weight;
			if (tr < crcr) {
				crcr = tr;
				closest = d;
			}
		}
		winner[i] = closest;
	}
        return NULL;
}
#endif

void NearestNeighborDistrictSet::setWinners() {
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].pop = 0;
		dists[d].areax = 0;
		dists[d].areay = 0;
		dists[d].areasum = 0;
	}
#if NEAREST_NEIGHBOR_MULTITHREAD
/* This doesn't help much, probably better to run two independent instances. */
	setupPtPool();
	for ( int i = 0; i < numSetWinnersThreads; i++ ) {
		ptParms[i].sov = sov;
                ptParms[i].nnds = this;
		ptPool[i].run( ::setWinnersThread, ptParms + i );
	}
	for ( int i = 0; i < numSetWinnersThreads; i++ ) {
		ptPool[i].join();
	}
	for ( int i = 0; i < numPoints; ++i ) {
		int closest = winner[i];
		dists[closest].pop += gd->pop[i];
		double ta = gd->area[i];
		dists[closest].areax += ta * gd->pos[i*2  ];
		dists[closest].areay += ta * gd->pos[i*2+1];
		dists[closest].areasum += ta;
	}
#elif defined(_OPENMP)
/* this is totally untested and probably wrong, but a start */
#pragma omp parallel for default(private)
	for ( int i = 0; i < numPoints; ++i ) {
		double bx, by;
		bx = gd->pos[i*2  ];
		by = gd->pos[i*2+1];
		POPTYPE closest = 0;
		double crcr;
		double dx, dy;
		dx = dists[0].x - bx;
		dy = dists[0].y - by;
		crcr = ((dx * dx) + (dy * dy)) * dists[0].weight;
		for ( POPTYPE d = 1; d < districts; ++d ) {
			dx = dists[d].x - bx;
			dy = dists[d].y - by;
			double tr = ((dx * dx) + (dy * dy)) * dists[d].weight;
			if (tr < crcr) {
				crcr = tr;
				closest = d;
			}
		}
		winner[i] = closest;
	}
	for ( int i = 0; i < numPoints; ++i ) {
		closest = winner[i];
		dists[closest].pop += gd->pop[i];
		double ta = gd->area[i];
		dists[closest].areax += ta * bx;
		dists[closest].areay += ta * by;
		dists[closest].areasum += ta;
        }
#else
	/* ye olde straight line code */
	for ( int i = 0; i < numPoints; ++i ) {
		double bx, by;
		bx = gd->pos[i*2  ];
		by = gd->pos[i*2+1];
		POPTYPE closest = 0;
		double crcr;
		double dx, dy;
		dx = dists[0].x - bx;
		dy = dists[0].y - by;
		crcr = ((dx * dx) + (dy * dy)) * dists[0].weight;
		for ( POPTYPE d = 1; d < districts; ++d ) {
			dx = dists[d].x - bx;
			dy = dists[d].y - by;
			double tr = ((dx * dx) + (dy * dy)) * dists[d].weight;
			if (tr < crcr) {
				crcr = tr;
				closest = d;
			}
		}
		winner[i] = closest;
		dists[closest].pop += gd->pop[i];
		double ta = gd->area[i];
		dists[closest].areax += ta * bx;
		dists[closest].areay += ta * by;
		dists[closest].areasum += ta;
	}
#endif
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].areax /= dists[d].areasum;
		dists[d].areay /= dists[d].areasum;
	}
        // FIXME TODO ensure contiguiity
}

double NearestNeighborDistrictSet::kNu = 0.01;
double NearestNeighborDistrictSet::kInc = 1.0 + kNu;
double NearestNeighborDistrictSet::kDec = 1.0 - kNu;
double NearestNeighborDistrictSet::kField = 0.0001;
double NearestNeighborDistrictSet::kDefaultPosMaxJitter = 0.01;
double NearestNeighborDistrictSet::kDefaultWeightMaxJitter = kNu;
double NearestNeighborDistrictSet::kDefaultJitterPeriod = 2000;
bool NearestNeighborDistrictSet::donndsfield = false;

int NearestNeighborDistrictSet::step() {
	for ( POPTYPE d = 0; d < districts; ++d ) {
#if 0
		dists[d].dx = jitter(posMaxJitter, jitterPeriod, sov->gencount);
		dists[d].dy = jitter(posMaxJitter, jitterPeriod, sov->gencount);
#else
		dists[d].dx = 0;
		dists[d].dy = 0;
#endif
	}
#if 01
	// step towards area center
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].dx += (dists[d].areax - dists[d].x) * kNu;
		dists[d].dy += (dists[d].areay - dists[d].y) * kNu;
	}
#endif
	// adjust weight based on over/under population
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].poperr = dists[d].pop - sov->districtPopTarget;
		if ( dists[d].poperr > 0.0 ) { // dists[d].pop is greater than target
			dists[d].weight *= kInc;
		} else if ( dists[d].poperr < 0.0 ) { // dists[d].pop is less than target
			dists[d].weight *= kDec;
		}
	}
	if ( donndsfield ) {
		// n^2 field application
		for ( POPTYPE d = 0; d < districts; ++d ) {
			for ( POPTYPE e = d+1; e < districts; ++e ) {
				double dx, dy, r;
				dx = dists[e].x - dists[d].x;
				dy = dists[e].y - dists[d].y;
				// (dx,dy) vector d->e
				r = sqrt((dx*dx) + (dy*dy));
				dx /= r;
				dy /= r;
				// attract towards overpopulated districts, repel away from underpopulated districts
				if ( dists[d].poperr > 0.0 ) {
					// attract
					dists[e].dx -= dx * kField;
					dists[e].dy -= dy * kField;
				} else if ( dists[d].poperr < 0.0 ) {
					// repel
					dists[e].dx += dx * kField;
					dists[e].dy += dy * kField;
				}
				if ( dists[e].poperr > 0.0 ) {
					// attract
					dists[d].dx += dx * kField;
					dists[d].dy += dy * kField;
				} else if ( dists[e].poperr < 0.0 ) {
					// repel
					dists[d].dx -= dx * kField;
					dists[d].dy -= dy * kField;
				}
			}
		}
	}
	// integrate step
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].x += dists[d].dx;
		dists[d].y += dists[d].dy;
                //dists[d].weight += jitter(weightMaxJitter, jitterPeriod, sov->gencount);
	}
	setWinners();
	return 0;
}

char* NearestNeighborDistrictSet::debugText() {
	return NULL;
}

void NearestNeighborDistrictSet::getStats(SolverStats* stats) {
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
	//POPTYPE median = sorti[districts/2];
	stats->poptotal = ptot;
	stats->popavg = popavg;
	stats->popstd = sqrt(popvar / (districts - 1.0));
	stats->popmin = minp;
	stats->popmax = maxp;
	stats->popmed = 0;//dists[median].pop;
	stats->mindist = dmin;
	stats->maxdist = dmax;
	stats->meddist = 0;//median;
}
void NearestNeighborDistrictSet::print(const char* filename) {

}
