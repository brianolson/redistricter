#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "Solver.h"
#include "tiger/mmaped.h"



#if 0
void writeCoordText( FILE* coordf, POPTYPE* winner ) {
	int i, j;
	for ( i = 0; i < districts; i++ ) {
		fprintf( coordf, "coords %d:",i);
		for ( j = 0; j < numZips; j++ ) {
			if ( winner[j] == i ) {
				fprintf( coordf, " (%f,%f)", zipd[j].pos.lon, zipd[j].pos.lat );
			}
		}
		fprintf( coordf, "\n");
	}
	fflush( coordf );
}
void writeFinalSpew( POPTYPE* winner, FILE* blaf ) {
	double popvar = getPopvar( winner );
	double moment = getMoment( winner );
	fprintf( blaf, "winner, popvar=%0.9g, moment=%0.9g, fitness=%0.9g\n", popvar, moment, fitness[sorti[0]] );
#if 0
	for ( int i = 0; i < numZips; i++ ) {
		fprintf( blaf, " %d", winner[i] );
	}
	fprintf( blaf, "\n");
#endif
	fflush( blaf );
}
#endif

#if 0
inline void swap( struct rusage*& a, struct rusage*& b ) {
	struct rusage* t;
	t = a;
	a = b;
	b = t;
}

long long tvDiffUsec( const struct timeval& a, const struct timeval& b ) {
	long long toret = a.tv_sec;
	toret -= b.tv_sec;
	toret *= 1000000;
	toret += a.tv_usec;
	toret -= b.tv_usec;
	return toret;
}
#endif
#if 0
void printRUDiff( FILE* f, const struct rusage* a, const struct rusage* b ) {
	long udu = (a->ru_utime.tv_sec - b->ru_utime.tv_sec) * 1000000 + a->ru_utime.tv_usec - b->ru_utime.tv_usec;
	fprintf( f, "%ld usec user time\n", udu );
}
#endif


int main( int argc, char** argv ) {
	Solver sov;
	return sov.main( argc, argv );
}
