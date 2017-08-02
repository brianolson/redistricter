#ifndef GRAB_INTERMEDIATE_STORAGE_H
#define GRAB_INTERMEDIATE_STORAGE_H

#include <math.h>

class Solver;

class GrabIntermediateStorage {
public:
	// source solver this instance is associated with.
	Solver* sov;
	double* edgeRelativeDistance;
	double* odEdgeRelativeDistance;	// higher better
	double* popRatio;					// lower better
	double* neighborRatio;				// higher better
	double* lockStrength;				// lower better
	double* popField;
	
	double edgeRelativeDistance_min, edgeRelativeDistance_max;
	double odEdgeRelativeDistance_min, odEdgeRelativeDistance_max;
	double popRatio_min, popRatio_max;
	double neighborRatio_min, neighborRatio_max;
	double lockStrength_min, lockStrength_max;
	double popField_min, popField_max;
	
	GrabIntermediateStorage(Solver* sovIn);
	void set(int index, double erd, double oerd, double pr, double nr, double ls, double pf);
	void clear();
	
	// allocates memory which must be free()'d by caller
	char* debugText();
	
#define DEFINE_field_norm(field) inline double field##Norm(int index) {\
	if (isnan(field[index])) { return 0.0; } \
	return (field[index] - field##_min) / (field##_max - field##_min); }
#define DEFINE_field_norm_reverse(field) inline double field##Norm(int index) {\
	if (isnan(field[index])) { return 0.0; } \
	return 1.0 - ((field[index] - field##_min) / (field##_max - field##_min)); }
	DEFINE_field_norm_reverse(edgeRelativeDistance)
	DEFINE_field_norm(odEdgeRelativeDistance)
	DEFINE_field_norm_reverse(popRatio)
	DEFINE_field_norm(neighborRatio)
	DEFINE_field_norm_reverse(lockStrength)
	DEFINE_field_norm_reverse(popField)
};

#endif
