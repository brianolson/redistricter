#ifndef ABSTRACT_DISTRICT_H
#define ABSTRACT_DISTRICT_H

#include "config.h"

class Solver;

class AbstractDistrict {
public:
	AbstractDistrict() {};
	virtual ~AbstractDistrict();
	// TODO: unused?
	virtual int remove( Solver* sov, int n, POPTYPE dist, double x, double y, double npop ) = 0;
	virtual double centerX() = 0;
	virtual double centerY() = 0;
};

#endif /* ABSTRACT_DISTRICT_H */
