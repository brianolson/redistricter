#include "LinearInterpolate.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifdef TEST_MAIN
#include <assert.h>
#include <stdio.h>
#endif

void LinearInterpolate::clear() {
	Node* dn = root;
	while (dn != NULL) {
		Node* t;
		t = dn->next;
		delete dn;
		dn = t;
	}
	root = NULL;
	cur = NULL;
}

void LinearInterpolate::parse(const char* spec) {
	char* nextp;
	double nx, ny;
	Node* append = root;
	
	if (append != NULL) {
		while (append->next != NULL) {
			append = append->next;
		}
	}
	
	do {
		nx = strtod(spec, &nextp);
		if ((nextp == NULL) || (*nextp == '\0')) {
			return;
		}
		spec = nextp;
		while ((*spec != '\0') &&
			   !(isdigit(*spec) || (*spec == '+') ||
				 (*spec == '-') || (*spec == '.'))) {
			spec++;
		}
		ny = strtod(spec, &nextp);
		if (nextp == NULL) {
			return;
		}
		if (root == NULL) {
			root = new Node(nx, ny);
			append = root;
		} else {
			append->next = new Node(nx, ny);
			append = append->next;
		}
		spec = nextp;
		while ((*spec != '\0') &&
			   !(isdigit(*spec) || (*spec == '+') ||
				 (*spec == '-') || (*spec == '.'))) {
			spec++;
		}
	} while (*spec != '\0');
}

void LinearInterpolate::setPoint(double x, double y) {
	if (root == NULL) {
		root = new Node(x, y);
		return;
	}
	if (root->x > x) {
		Node* tn = new Node(x, y);
		tn->next = root;
		root = tn;
		return;
	}
	Node* append = root;
	while ((append->next != NULL) && (append->next->x < x)) {
		append = append->next;
	}
	if (append->x == x) {
		append->y = y;
		return;
	}
	Node* tn = new Node(x, y);
	tn->next = append->next;
	append->next = tn;
}

double LinearInterpolate::value(double x) {
	if ((cur == NULL) || (x < cur->x)) {
		if (root == NULL) {
			return NAN;
		}
		cur = root;
		if (x <= cur->x) {
			return cur->y;
		}
	}
	do {
		if (cur->next == NULL) {
			return cur->y;
		}
		if (cur->next->x < x) {
			cur = cur->next;
		} else {
			break;
		}
	} while (1);
	double alpha = (x - cur->x) / (cur->next->x - cur->x);
	return (cur->next->y * alpha) + (cur->y * (1.0 - alpha));
}


#ifdef TEST_MAIN

//#define assert_feq(a,b) assert(fabs((a)-(b)) < 0.00001)
#define assert_feq(a,b) if(fabs((a)-(b)) > 0.00001){fprintf(stderr,"%s != %s, got %f, %f\n", #a, #b, (a), (b)); assert(false);}

int main(int argc, char** argv) {
	LinearInterpolate li;
	li.parse("0,0,1,1,11,2");
	assert(li.value(-1.0) == 0.0);
	assert(li.value(12.0) == 2.0);
	assert_feq(li.value(0.0),0.0);
	assert_feq(li.value(0.5),0.5);
	assert_feq(li.value(1.0),1.0);
	assert_feq(li.value(6.0),1.5);
	assert_feq(li.value(11.0),2.0);
}

#endif
