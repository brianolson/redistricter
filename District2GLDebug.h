#ifndef DISTRICT_2_GL_DEBUG_H
#define DISTRICT_2_GL_DEBUG_H

#include "NodeColoring.h"

class ColorNodeByERD : public NodeColoring {
public:
	virtual ~ColorNodeByERD();
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray);
virtual const char* name();
};

class ColorNodeByOERD : public NodeColoring {
public:
	virtual ~ColorNodeByOERD();
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray);
virtual const char* name();
};

class ColorNodeByNeighRat : public NodeColoring {
public:
	virtual ~ColorNodeByNeighRat();
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray);
virtual const char* name();
};

class ColorNodeByPopRat : public NodeColoring {
public:
	virtual ~ColorNodeByPopRat();
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray);
virtual const char* name();
};

class ColorNodeByLockStrength : public NodeColoring {
public:
	virtual ~ColorNodeByLockStrength();
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray);
virtual const char* name();
};

class ColorNodeByPopField : public NodeColoring {
public:
	virtual ~ColorNodeByPopField();
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray);
virtual const char* name();
};

#endif /* DISTRICT_2_GL_DEBUG_H */
