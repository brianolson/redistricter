#include "District2.h"
#include "GeoData.h"
#include "popSSD.h"

// Fryer-Holden sum of squared distances between users within a
// district, summed across all disticts.
double popSSD(POPTYPE* winner, GeoData* gd, int districts) {
	double ssd = 0.0;

	// naive implementation runs in order n^2 of total number of
	// points in a state; might be able to do better by partitioning
	// into districts using scratch memory? O(n) pass to copy, then a
	// handful of smaller n^2 parts?
	//
	// This is also potentially vulnerable to scale based roundoff
	// loss. If the sum in ssd becomes too large adding another value
	// to it will be insignificant and lost.
	for (int i = 0; i < gd->numPoints; i++) {
		POPTYPE d = winner[i];
		if (d == NODISTRICT) {
			continue;
		}
		for (int j = i + 1; j < gd->numPoints; j++) {
			if (winner[j] == d) {
				double dx = gd->pos[i*2  ] - gd->pos[j*2  ];
				double dy = gd->pos[i*2+1] - gd->pos[j*2+1];
				ssd += (dx * dx + dy * dy) * (gd->pop[i] + gd->pop[j]);
			}
		}
	}
	
	return ssd;
}
