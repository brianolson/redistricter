#ifndef LINEAR_INTERPOLATE_H
#define LINEAR_INTERPOLATE_H
#include <stdlib.h>

class LinearInterpolate {
public:
	LinearInterpolate() : root(NULL), cur(NULL) {}
	~LinearInterpolate() {
		clear();
	}

	// parse comma separated string, could be command line arg
	// "x,y,x,y,x,y" ...
	void parse(const char* spec);

	void setPoint(double x, double y);

	double value(double x);

	void clear();

private:
	class Node {
public:
		double x, y;
		Node* next;

		Node(double ix, double iy) : x(ix), y(iy), next(NULL) {}
	};

	Node* root;
	Node* cur;
};

#endif /* LINEAR_INTERPOLATE_H */
