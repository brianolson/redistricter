#ifndef NODE_COLORING_H
#define NODE_COLORING_H

class Solver;
typedef float GLfloat;

class NodeColoring {
public:
	virtual void colorNode(const Solver* sov, int index, GLfloat* varray) = 0;
	virtual const char* name() = 0;
	virtual ~NodeColoring() {}
};

extern NodeColoring* coloring;

#endif /* NODE_COLORING_H */
