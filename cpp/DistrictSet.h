#ifndef DISTRICT_SET_H
#define DISTRICT_SET_H

class AbstractDistrict;
class Solver;
class SolverStats;

// Should be subclassed by a tightly bound district implementation
class DistrictSet {
public:
	DistrictSet(Solver* sovIn) : districts(-1), they((AbstractDistrict**)0), sov(sovIn) {}
	virtual ~DistrictSet();
	
	virtual void alloc(int size) = 0;
	virtual void initNewRandomStart() = 0;
	virtual void initFromLoadedSolution() = 0;
	virtual int step() = 0;

	virtual char* debugText() = 0;
	
	virtual void getStats(SolverStats* stats) = 0;
	virtual void print(const char* filename);

	virtual void fixupDistrictContiguity() = 0;

	AbstractDistrict& operator[](int index) {
		return *(they[index]);
	}
	
	virtual int numParameters() = 0;
	// Return NULL if index too high or too low.
	virtual const char* getParameterLabelByIndex(int index) = 0;
	virtual double getParameterByIndex(int index) = 0;
	virtual void setParameterByIndex(int index, double value) = 0;
	
	// how long should this run?
	virtual int defaultGenerations() = 0;

	// How many are they?
	int districts;
	
	// Probably pointers to a backing array of T[districts]
	// for some subclass T of AbstractDistrict
	AbstractDistrict** they;

	// Every DistrictSet is associated with some Solver
	Solver* sov;
};

#endif /* DISTRICT_SET_H */
