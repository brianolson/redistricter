#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "GrabIntermediateStorage.h"
#include "Solver.h"
#include "GeoData.h"

GrabIntermediateStorage::GrabIntermediateStorage(Solver* sovIn)
	: sov(sovIn),
	edgeRelativeDistance(NULL),
	odEdgeRelativeDistance(NULL),
	popRatio(NULL),
	neighborRatio(NULL),
	lockStrength(NULL),
	popField(NULL)
{
	edgeRelativeDistance = new double[sov->gd->numPoints];
	odEdgeRelativeDistance = new double[sov->gd->numPoints];
	popRatio = new double[sov->gd->numPoints];
	neighborRatio = new double[sov->gd->numPoints];
	lockStrength = new double[sov->gd->numPoints];
	popField = new double[sov->gd->numPoints];
	clear();
}

#define GIS_SET(field, index, value) field[index] = value; \
if ( value < field##_min ) { field##_min = value; } \
if ( value > field##_max ) { field##_max = value; }

void GrabIntermediateStorage::set(int index, double erd, double oerd, double pr, double nr, double ls, double pf) {
	GIS_SET(edgeRelativeDistance, index, erd);
	GIS_SET(odEdgeRelativeDistance, index, oerd);
	GIS_SET(popRatio, index, pr);
	GIS_SET(neighborRatio, index, nr);
	GIS_SET(lockStrength, index, ls);
	GIS_SET(popField, index, pf);
}
#undef GIS_SET

void GrabIntermediateStorage::clear() {
	for (int i = 0; i < sov->gd->numPoints; ++i) {
		edgeRelativeDistance[i] = NAN;
		odEdgeRelativeDistance[i] = NAN;
		popRatio[i] = NAN;
		neighborRatio[i] = NAN;
		lockStrength[i] = NAN;
		popField[i] = NAN;
	}
	edgeRelativeDistance_min = HUGE_VAL;
	edgeRelativeDistance_max = -HUGE_VAL;
	odEdgeRelativeDistance_min = HUGE_VAL;
	odEdgeRelativeDistance_max = -HUGE_VAL;
	popRatio_min = HUGE_VAL;
	popRatio_max = -HUGE_VAL;
	neighborRatio_min = HUGE_VAL;
	neighborRatio_max = -HUGE_VAL;
	lockStrength_min = HUGE_VAL;
	lockStrength_max = -HUGE_VAL;
	popField_min = HUGE_VAL;
	popField_max = -HUGE_VAL;
}

char* GrabIntermediateStorage::debugText() {
	// Allocate more than enough, oh well.
	// Should be short lived and not used in time-size critical moments anyway.
	int len = 512;
	char* toret = (char*)malloc(len);
	char* cur = toret;
	int used;
	used = snprintf(cur, len, " erd min=%0.7g max=%0.7g weight=%0.7g\n",
					edgeRelativeDistance_min, edgeRelativeDistance_max, District2::edgeRelativeDistanceFactor);
	len -= used; cur += used;
	used = snprintf(cur, len, "oerd min=%0.7g max=%0.7g weight=%0.7g\n",
					odEdgeRelativeDistance_min, odEdgeRelativeDistance_max, District2::odEdgeRelativeDistanceFactor);
	len -= used; cur += used;
	used = snprintf(cur, len, "popr min=%0.7g max=%0.7g weight=%0.7g\n",
					popRatio_min, popRatio_max, District2::popRatioFactor);
	len -= used; cur += used;
	used = snprintf(cur, len, "  nr min=%0.7g max=%0.7g weight=%0.7g\n",
					neighborRatio_min, neighborRatio_max, District2::neighborRatioFactor);
	len -= used; cur += used;
	used = snprintf(cur, len, "lock min=%0.7g max=%0.7g weight=%0.7g\n",
					lockStrength_min, lockStrength_max, District2::lockStrengthFactor);
	len -= used; cur += used;
	used = snprintf(cur, len, "popf min=%0.7g max=%0.7g weight=%0.7g\n",
					popField_min, popField_max, District2::popFieldFactor);
	return toret;
}
