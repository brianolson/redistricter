#include "CountyCityDistricter.h"

#include <assert.h>
#include <unistd.h>

CountyCityDistricterSet::CountyCityDistricterSet(Solver* sovIn)
	: DistrictSet(sovIn) {
}

CountyCityDistricterSet::~CountyCityDistricterSet() {
}

void CountyCityDistricterSet::alloc(int size) {
	assert(false); // TODO: WRITEME
}
void CountyCityDistricterSet::initNewRandomStart() {
	assert(false); // TODO: WRITEME
}
void CountyCityDistricterSet::initFromLoadedSolution() {
	assert(false); // TODO: WRITEME
}
// return err { 0: ok, else err }
int CountyCityDistricterSet::step() {
	assert(false); // TODO: WRITEME
	return 0;
}

char* CountyCityDistricterSet::debugText() {
	assert(false); // TODO: WRITEME
	return NULL;
}
	
void CountyCityDistricterSet::getStats(SolverStats* stats) {
	assert(false); // TODO: WRITEME
}
void CountyCityDistricterSet::print(const char* filename) {
	assert(false); // TODO: WRITEME
}

void CountyCityDistricterSet::fixupDistrictContiguity() {
	assert(false); // TODO: WRITEME
}

int CountyCityDistricterSet::numParameters() {
	assert(false); // TODO: WRITEME
}
// Return NULL if index too high or too low.
const char* CountyCityDistricterSet::getParameterLabelByIndex(int index) {
	assert(false); // TODO: WRITEME
	return NULL;
}
double CountyCityDistricterSet::getParameterByIndex(int index) {
	assert(false); // TODO: WRITEME
	return 0.0;
}
void CountyCityDistricterSet::setParameterByIndex(int index, double value) {
	assert(false); // TODO: WRITEME
}
	
// how long should this run?
int CountyCityDistricterSet::defaultGenerations() {
	assert(false); // TODO: WRITEME
	return 0;
}


CountyCityDistrict::CountyCityDistrict()
	: AbstractDistrict() {
}
CountyCityDistrict::~CountyCityDistrict() {
}
int CountyCityDistrict::add( Solver* sov, int n, POPTYPE dist ) {
	// TODO: unused?
	assert(false); // TODO: WRITEME
	return 0;
}
int CountyCityDistrict::remove( Solver* sov, int n, POPTYPE dist, double x, double y, double npop ) {
	// TODO: unused?
	assert(false); // TODO: WRITEME
	return 0;
}
double CountyCityDistrict::centerX() {
	assert(false); // TODO: WRITEME
	return 0.0;
}
double CountyCityDistrict::centerY() {
	assert(false); // TODO: WRITEME
	return 0.0;
}
