#include "StatThing.h"

StatThing::StatThing()
: minimum(0.0), maximum(0.0), sum(0.0), count(0)
{
}

void StatThing::log(double x) {
	if (count == 0) {
		minimum = x;
		maximum = x;
	} else {
		if (x < minimum) {
			minimum = x;
		}
		if (x > maximum) {
			maximum = x;
		}
	}
	sum += x;
	count++;
}
