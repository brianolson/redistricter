#include "District2GLDebug.h"
#include "GeoData.h"
#include "NodeColoring.h"
#include "Solver.h"

#include <assert.h>
#include <math.h>

extern const unsigned char* colors;
extern const int numColors;

#define LINK_RATE (0.7 / 255.0)


class ColorNodeByDistrict : public NodeColoring {
public:
	virtual ~ColorNodeByDistrict(){}
virtual void colorNode(const Solver* sov, int index, GLfloat* varray) {
	int da;
	const unsigned char* color;
	da = sov->winner[index];
	// GL_C3F_V3F
	if ( da == NODISTRICT ) {
		varray[0] = 0.2;
		varray[1] = 0.2;
		varray[2] = 0.2;
	} else {
		color = colors + ((da % numColors) * 3);
		varray[0] = color[0] * LINK_RATE;
		varray[1] = color[1] * LINK_RATE;
		varray[2] = color[2] * LINK_RATE;
	}
	varray[4] = 1.0;
}
virtual const char* name() {
	return "By District";
}
};

class ColorNodeByPopulation : public NodeColoring {
public:
	virtual ~ColorNodeByPopulation(){}
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	int pop = sov->gd->pop[nodeIndex];
	//varray[0] = varray[1] = varray[2] = (pop * 1.0) / sov->gd->maxpop;
	varray[0] = varray[1] = varray[2] = log(pop) / log(sov->gd->maxpop);
	varray[3] = 1.0;
}
virtual const char* name() {
	return "By Population";
}
};

NodeColoring* colorings[] = {
	new ColorNodeByDistrict(),
	new ColorNodeByPopulation(),
	new ColorNodeByERD(),
	new ColorNodeByOERD(),
	new ColorNodeByNeighRat(),
	new ColorNodeByPopRat(),
	new ColorNodeByLockStrength(),
	new ColorNodeByPopField(),
	NULL
};
NodeColoring* coloring = colorings[0];

extern "C" const char* getNodeColoringName(int index);
extern "C" void setNodeColoringByIndex(int index);

const char* getNodeColoringName(int index) {
	for (int i = 0; i <= index; ++i) {
		if (colorings[i] == NULL) {
			return NULL;
		}
	}
	return colorings[index]->name();
}
void setNodeColoringByIndex(int index) {
	for (int i = 0; i <= index; ++i) {
		if (colorings[i] == NULL) {
			return;
		}
	}
	coloring = colorings[index];
}


