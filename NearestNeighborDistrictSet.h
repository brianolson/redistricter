#ifndef NEAREST_NEIGHBOR_DISTRICT_SET_H
#define NEAREST_NEIGHBOR_DISTRICT_SET_H

#include "DistrictSet.h"

class NearestNeighborDistrict;

#ifndef NEAREST_NEIGHBOR_MULTITHREAD
#define NEAREST_NEIGHBOR_MULTITHREAD 0
#endif
#if NEAREST_NEIGHBOR_MULTITHREAD
class SetWinnersThreadArgs;
#endif

class NearestNeighborDistrictSet : public DistrictSet {
public:
	NearestNeighborDistrictSet(Solver* sovIn);
	virtual ~NearestNeighborDistrictSet();
	
	virtual void alloc(int size);
	virtual void initNewRandomStart();
	virtual void initFromLoadedSolution();
	virtual int step();

	virtual char* debugText();
	
	virtual void getStats(SolverStats* stats);
	virtual void print(const char* filename);

	void setWinners();
#if NEAREST_NEIGHBOR_MULTITHREAD
	void* setWinnersThread(SetWinnersThreadArgs* args);
#endif

	static double kNu;
	static double kInc;
	static double kDec;
	static double kField;
	static double kDefaultPosMaxJitter;
	static double kDefaultWeightMaxJitter;
	static double kDefaultJitterPeriod;
	static bool donndsfield;

	double posMaxJitter;
	double weightMaxJitter;
	double jitterPeriod;
private:
	NearestNeighborDistrict* dists;
};

#endif /* NEAREST_NEIGHBOR_DISTRICT_SET_H */

