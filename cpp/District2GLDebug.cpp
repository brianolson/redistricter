#include "District2GLDebug.h"
#include "Solver.h"
#include "District2.h"
#include "GrabIntermediateStorage.h"

#define grabdebug (((District2Set*)(sov->dists))->grabdebug)

ColorNodeByERD::~ColorNodeByERD(){}
void ColorNodeByERD::colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	varray[3] = 1.0;
	if ( grabdebug == NULL ) {
		varray[0] = varray[1] = varray[2] = 0.5;
		return;
	}
	varray[0] = varray[1] = varray[2] = grabdebug->edgeRelativeDistanceNorm(nodeIndex);
}
const char* ColorNodeByERD::name() {
	return "By Edge Relative Distance";
}

ColorNodeByOERD::~ColorNodeByOERD(){}
void ColorNodeByOERD::colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	varray[3] = 1.0;
	if ( grabdebug == NULL ) {
		varray[0] = varray[1] = varray[2] = 0.5;
		return;
	}
	varray[0] = varray[1] = varray[2] = grabdebug->odEdgeRelativeDistanceNorm(nodeIndex);
}
const char* ColorNodeByOERD::name() {
	return "By Other District's Edge Relative Distance";
}

ColorNodeByNeighRat::~ColorNodeByNeighRat(){}
void ColorNodeByNeighRat::colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	varray[3] = 1.0;
	if ( grabdebug == NULL ) {
		varray[0] = varray[1] = varray[2] = 0.5;
		return;
	}
	varray[0] = varray[1] = varray[2] = grabdebug->neighborRatioNorm(nodeIndex);
}
const char* ColorNodeByNeighRat::name() {
	return "By Neighbor Ratio";
}

ColorNodeByPopRat::~ColorNodeByPopRat(){}
void ColorNodeByPopRat::colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	varray[3] = 1.0;
	if ( grabdebug == NULL ) {
		varray[0] = varray[1] = varray[2] = 0.5;
		return;
	}
	varray[0] = varray[1] = varray[2] = grabdebug->popRatioNorm(nodeIndex);
}
const char* ColorNodeByPopRat::name() {
	return "By Population Ratio";
}

ColorNodeByLockStrength::~ColorNodeByLockStrength(){}
void ColorNodeByLockStrength::colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	varray[3] = 1.0;
	if ( grabdebug == NULL ) {
		varray[0] = varray[1] = varray[2] = 0.5;
		return;
	}
	varray[0] = varray[1] = varray[2] = grabdebug->lockStrengthNorm(nodeIndex);
}
const char* ColorNodeByLockStrength::name() {
	return "By Lock Strength";
}

ColorNodeByPopField::~ColorNodeByPopField(){}
void ColorNodeByPopField::colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	varray[3] = 1.0;
	if ( grabdebug == NULL ) {
		varray[0] = varray[1] = varray[2] = 0.5;
		return;
	}
	varray[0] = varray[1] = varray[2] = grabdebug->popFieldNorm(nodeIndex);
}
const char* ColorNodeByPopField::name() {
	return "By Population Field";
}

