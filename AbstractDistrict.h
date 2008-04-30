#ifndef ABSTRACT_DISTRICT_H
#define ABSTRACT_DISTRICT_H

#include "districter.h"

class Solver;

class AbstractDistrict {
public:
	AbstractDistrict() : distx(0.0), disty(0.0) {};
	AbstractDistrict(double x, double y) : distx(x), disty(y) {}
	virtual ~AbstractDistrict();
	virtual int add( Solver* sov, int n, POPTYPE dist ) = 0;
	virtual int remove( Solver* sov, int n, POPTYPE dist, double x, double y, double npop ) = 0;
#if 0
	virtual double centerX() = 0;
	virtual double centerY() = 0;
#else
public:
	// TODO: make these protected and use inline accessors for public read
	double distx;
	double disty;

public:
	inline double centerX() const { return distx; }
	inline double centerY() const { return disty; }
#endif
};

#endif /* ABSTRACT_DISTRICT_H */
