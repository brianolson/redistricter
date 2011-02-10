#include "NearestNeighborDistrictSet.h"

#include "AbstractDistrict.h"
#include "Bitmap.h"
#include "Node.h"
#include "Solver.h"
#include <assert.h>
#include <math.h>
#include "GeoData.h"

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
	double distx, disty;
	double weight;
	double dx, dy;
	double poperr;
	double areax, areay, areasum;
	int pop;
	NearestNeighborDistrict() : weight(1.0) {}
	NearestNeighborDistrict(double xin, double yin)
		: distx(xin), disty(yin), weight(1.0) {}

	void set(double xin, double yin) {
		distx = xin;
		disty = yin;
	}
	virtual int add( Solver* sov, int n, POPTYPE dist ) { assert(0); return 0; };
	virtual int remove( Solver* sov, int n, POPTYPE dist,
		double x, double y, double npop ) { assert(0); return 0; };
	virtual double centerX() { return distx; };
	virtual double centerY() { return disty; };
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
		winner[i] = d;
	}
	setWinners();
	resumDistrictCenters();
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
}

void NearestNeighborDistrictSet::fixupDistrictContiguity() {
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;
	Bitmap hit(numPoints);
	hit.zero();
	int* bfsearchq = new int[numPoints];
	int bfin = 0;
	int bfout = 0;
	ContiguousGroup* groups = new ContiguousGroup[districts];
	Node* nodes = sov->nodes;
	
	for ( int i = 0; i < numPoints; ++i ) {
		if ( ! hit.test(i) ) {
			POPTYPE d = winner[i];
			//bfin = 0;
			bfout = 0;
			bfsearchq[0] = i;
			hit.set(i);
			bfin = 1;
			ContiguousGroup* cur;
			if (groups[d].start == -1) {
				// original array set
				groups[d].start = i;
				groups[d].count = 1;
				cur = &(groups[d]);
			} else {
				cur = groups[d].next = new ContiguousGroup( i, groups[d].next );
			}
			// breadth-first search
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
	}
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
					bfout = 0;
					bfin = 1;
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
	if ( pointsRepod ) {
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
							dx = dists[d].distx - bx;
							dy = dists[d].disty - by;
							crcr = ((dx * dx) + (dy * dy)) * dists[d].weight;
						} else if ( d != closest ) {
							dx = dists[d].distx - bx;
							dy = dists[d].disty - by;
							double tr = ((dx * dx) + (dy * dy)) * dists[d].weight;
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
	
	delete [] groups;
	delete [] bfsearchq;
}

void NearestNeighborDistrictSet::setWinners() {
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;
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
	/*for ( int i = 0; i < numPoints; ++i ) {
		int closest = winner[i];
		dists[closest].pop += gd->pop[i];
		double ta = gd->area[i];
		dists[closest].areax += ta * gd->pos[i*2  ];
		dists[closest].areay += ta * gd->pos[i*2+1];
		dists[closest].areasum += ta;
	}*/
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
	/*for ( int i = 0; i < numPoints; ++i ) {
		closest = winner[i];
		dists[closest].pop += gd->pop[i];
		double ta = gd->area[i];
		dists[closest].areax += ta * bx;
		dists[closest].areay += ta * by;
		dists[closest].areasum += ta;
        }*/
#else
	/* ye olde straight line code */
	for ( int i = 0; i < numPoints; ++i ) {
		double bx, by;
		bx = gd->pos[i*2  ];
		by = gd->pos[i*2+1];
		POPTYPE closest = 0;
		double crcr;
		double dx, dy;
		dx = dists[0].distx - bx;
		dy = dists[0].disty - by;
		crcr = ((dx * dx) + (dy * dy)) * dists[0].weight;
		for ( POPTYPE d = 1; d < districts; ++d ) {
			dx = dists[d].distx - bx;
			dy = dists[d].disty - by;
			double tr = ((dx * dx) + (dy * dy)) * dists[d].weight;
			if (tr < crcr) {
				crcr = tr;
				closest = d;
			}
		}
		winner[i] = closest;
	}
#endif
}

void NearestNeighborDistrictSet::resumDistrictCenters() {
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	int numPoints = gd->numPoints;
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].pop = 0;
		dists[d].areax = 0;
		dists[d].areay = 0;
		dists[d].areasum = 0;
	}
	for ( int i = 0; i < numPoints; ++i ) {
		POPTYPE closest = winner[i];
		double ta = gd->area[i];
		double bx = gd->pos[i*2  ];
		double by = gd->pos[i*2+1];
		dists[closest].pop += gd->pop[i];
		dists[closest].areax += ta * bx;
		dists[closest].areay += ta * by;
		dists[closest].areasum += ta;
	}
	for ( POPTYPE d = 0; d < districts; ++d ) {
		dists[d].areax /= dists[d].areasum;
		dists[d].areay /= dists[d].areasum;
	}
}

// center motion step multiplier
double NearestNeighborDistrictSet::kNu = 0.01;
// weight increase multiplier
double NearestNeighborDistrictSet::kInc = 1.0 + kNu;
// weight decrease multiplier
double NearestNeighborDistrictSet::kDec = 1.0 - kNu;
// field effect weight
double NearestNeighborDistrictSet::kField = 0.0001;
// do n^2 weight field
bool NearestNeighborDistrictSet::donndsfield = false;
// FIXME implement jitter and add it to parameter interface.
double NearestNeighborDistrictSet::kDefaultPosMaxJitter = 0.01;
double NearestNeighborDistrictSet::kDefaultWeightMaxJitter = kNu;
double NearestNeighborDistrictSet::kDefaultJitterPeriod = 2000;

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
		dists[d].dx += (dists[d].areax - dists[d].distx) * kNu;
		dists[d].dy += (dists[d].areay - dists[d].disty) * kNu;
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
				dx = dists[e].distx - dists[d].distx;
				dy = dists[e].disty - dists[d].disty;
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
		dists[d].distx += dists[d].dx;
		dists[d].disty += dists[d].dy;
                //dists[d].weight += jitter(weightMaxJitter, jitterPeriod, sov->gencount);
	}
	setWinners();
	fixupDistrictContiguity();
	resumDistrictCenters();
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
	
	double nodpop = 0.0;
	int nod = 0;
	double moment = 0.0;
	POPTYPE* winner = sov->winner;
	const GeoData* gd = sov->gd;
	for ( int i = 0; i < gd->numPoints; i++ ) {
		if ( winner[i] == NODISTRICT ) {
			nod++;
			nodpop += gd->pop[i];		
		} else {
			double dx, dy;
			NearestNeighborDistrict* cd;
			cd = &(dists[winner[i]]);
			dx = cd->distx - gd->pos[i*2  ];
			dy = cd->disty - gd->pos[i*2+1];
			moment += sqrt(dx * dx + dy * dy) * gd->pop[i];
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
void NearestNeighborDistrictSet::print(const char* filename) {

}

static const char* parameterNames[] = {
"center motion step multiplier",
"weight increase multiplier",
"weight decrease multiplier",
"field effect weight",
"do n^2 weight field",
};
static const int kNumParameterNames = 5;

int NearestNeighborDistrictSet::numParameters() {
	return kNumParameterNames;
}
// Return NULL if index too high or too low.
const char* NearestNeighborDistrictSet::getParameterLabelByIndex(int index) {
	if ((index < 0) || (index >= kNumParameterNames)) {
		return NULL;
	}
	return parameterNames[index];
}
double NearestNeighborDistrictSet::getParameterByIndex(int index) {
	if ((index < 0) || (index >= kNumParameterNames)) {
		return NAN;
	}
	switch (index) {
		case 0:
			// center motion step multiplier
			return NearestNeighborDistrictSet::kNu;
		case 1:
			// weight increase multiplier
			return NearestNeighborDistrictSet::kInc;
		case 2:
			// weight decrease multiplier
			return NearestNeighborDistrictSet::kDec;
		case 3:
			// field effect weight
			return NearestNeighborDistrictSet::kField;
		case 4:
			// do n^2 weight field
			if (NearestNeighborDistrictSet::donndsfield) {
				return 1.0;
			} else {
				return 0.0;
			}
		default:
			assert(0);
	}
	assert(0);
	return NAN;
}
void NearestNeighborDistrictSet::setParameterByIndex(int index, double value) {
	if ((index < 0) || (index >= kNumParameterNames)) {
		return;
	}
	switch (index) {
		case 0:
			// center motion step multiplier
			NearestNeighborDistrictSet::kNu = value;
		case 1:
			// weight increase multiplier
			NearestNeighborDistrictSet::kInc = value;
		case 2:
			// weight decrease multiplier
			NearestNeighborDistrictSet::kDec = value;
		case 3:
			// field effect weight
			NearestNeighborDistrictSet::kField = value;
		case 4:
			// do n^2 weight field
			if (value >= 0.5) {
				NearestNeighborDistrictSet::donndsfield = true;
			} else {
				NearestNeighborDistrictSet::donndsfield = false;
			}
		default:
			assert(0);
	}
}

int NearestNeighborDistrictSet::defaultGenerations() {
	return 15000;
}
