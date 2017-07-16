#ifndef COUNTY_CITY_DISTRICTER_H
#define COUNTY_CITY_DISTRICTER_H

#include "AbstractDistrict.h"
#include "DistrictSet.h"

class Solver;

class CountyCityDistricterSet : public DistrictSet {
	public:
	CountyCityDistricterSet(Solver* sovIn);
	virtual ~CountyCityDistricterSet();

	virtual void alloc(int size);
	virtual void initNewRandomStart();
	virtual void initFromLoadedSolution();
	// return err { 0: ok, else err }
	virtual int step();

	virtual char* debugText();
	
	virtual void getStats(SolverStats* stats);
	virtual void print(const char* filename);

	virtual void fixupDistrictContiguity();

	virtual int numParameters();
	// Return NULL if index too high or too low.
	virtual const char* getParameterLabelByIndex(int index);
	virtual double getParameterByIndex(int index);
	virtual void setParameterByIndex(int index, double value);
	
	// how long should this run?
	virtual int defaultGenerations();

	private:
	// ...
};

class CountyCityDistrict : public AbstractDistrict {
	public:
	CountyCityDistrict();
	virtual ~CountyCityDistrict();
	virtual int add( Solver* sov, int n, POPTYPE dist );
	virtual int remove( Solver* sov, int n, POPTYPE dist, double x, double y, double npop );
	virtual double centerX();
	virtual double centerY();
};

#endif /* COUNTY_CITY_DISTRICTER_H */
