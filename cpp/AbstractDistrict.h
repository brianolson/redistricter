#ifndef ABSTRACT_DISTRICT_H
#define ABSTRACT_DISTRICT_H

#include "config.h"

class Solver;

class AbstractDistrict {
public:
	AbstractDistrict() {};
	virtual ~AbstractDistrict();
	virtual double centerX() = 0;
	virtual double centerY() = 0;
};

#endif /* ABSTRACT_DISTRICT_H */
