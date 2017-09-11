#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "DistrictSet.h"
#include "AbstractDistrict.h"
#include "Solver.h"
#include "GeoData.h"

DistrictSet::~DistrictSet() {
	if ( they != NULL ) {
		delete [] they;
	}
}

void DistrictSet::alloc(int size) {
	districts = size;
	they = new AbstractDistrict*[size];
}

void DistrictSet::print(const char* filename) {
	fprintf(stderr,"FIXME WRITEME DistrictSet::print(\"%s\")\n", filename);
}

int DistrictSet::popTarget() const {
	return sov->gd->totalpop / districts;
}
