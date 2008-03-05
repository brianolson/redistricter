#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "districter.h"

// storage floating type
#define SFTYPE double
// register floating type
#define RFTYPE double
// just has to be big enough to identify a district number. unsigned char good up through 255 districts
#define POPTYPE unsigned char

#if 01
extern "C" double greatCircleDistance(double lat1,double long1,double lat2,double long2);
#else
// cheat, use cartesian distance because it's faster
double greatCircleDistance(double lat1,double long1,double lat2,double long2) {
	double dlat = lat1 - lat2;
	double dlon = lon1 - lon2;
	return sqrt( dlat * dlat + dlon * dlon );
}
#endif

PosPop* zipd = NULL;
int numZips;
PosPop* scoretmp = NULL;
int districts = 80;

/* edge[0..numZips-1] are the first edge to come off each zipd */
Edge* edges = NULL;
int numEdges = 0;

// the GA gene pool
POPTYPE* pop = NULL;
double* fitness = NULL;
double* moments = NULL;
double* popvars = NULL;
int* sorti = NULL;
int popsize = 10000;
int topn;

double totalpop = 0.0;
double districtPopTarget = 0.0;

FILE* blaf;

double fitavg;

// a genetic string is int[numZips] with each index containing the district assignment of a zip index

double getPopvar( POPTYPE* a ) {
	int i;
	double popvar = 0.0;
	for ( i = 0; i < districts; i++ ) {
		scoretmp[i].pos.lat = 0;
		scoretmp[i].pos.lon = 0;
		scoretmp[i].pop = 0;
	}
	// find population weighted centers
	for ( i = 0; i < numZips; i++ ) {
		int d;
		double zpop = zipd[i].pop;
		d = a[i];
		scoretmp[d].pos.lat += zipd[i].pos.lat * zpop;
		scoretmp[d].pos.lon += zipd[i].pos.lon * zpop;
		scoretmp[d].pop += zpop;
	}
	for ( i = 0; i < districts; i++ ) {
		double p;
		p = scoretmp[i].pop;
		scoretmp[i].pos.lat /= p;
		scoretmp[i].pos.lon /= p;
		p -= districtPopTarget;
		popvar += p * p;
	}
#if 01
#elif 01
	// dividing by a constant factor, while handy for human readable output, doesn't actually change anything and is thus a waste of time
	popvar /= (districtPopTarget*districtPopTarget);
#else
	popvar = sqrt(popvar)/* / districtPopTarget*/;
#endif
	return popvar;
}

// low score better
// getPopvar must have been run first, to calculate district centers
double getMoment( POPTYPE* a ) {
	int i;
	double toret = 0.0;
	// calculate moment of inertia of districts
	for ( i = 0; i < numZips; i++ ) {
		int d;
		double zpop;
		double dist;
		d = a[i];
		zpop = zipd[i].pop;
#if 01
		// cheat, pytharogan/cartesian distance squared is much faster
		// no trig and no sqrt == fast!
		// um, enable true great circle distance for fine tuning at end?
		{
		    double dlat, dlon;
		    dlat = scoretmp[d].pos.lat - zipd[i].pos.lat;
		    dlon = scoretmp[d].pos.lon - zipd[i].pos.lon;
		    dist = dlat * dlat + dlon * dlon;
		}
#else
		dist = greatCircleDistance( scoretmp[d].pos.lat, scoretmp[d].pos.lon, zipd[i].pos.lat, zipd[i].pos.lon );
		dist = dist * dist;
#endif
		toret += dist * zpop;
	}
	//printf("toret=%g, popvar=%g\n", toret, popvar );
	return toret;
}
double score( POPTYPE* a ) {
	double popvar, moment;
	popvar = getPopvar( a );
	moment = getMoment( a );
#if 01
	return moment;
#elif 01
	return popvar * moment * moment * moment * moment;
#elif 0
	return popvar * moment * moment * moment;
#elif 01
	return popvar * moment * moment;
#else
	return popvar * moment;
#endif
}

void mutate( POPTYPE* dest ) {
	if ( (random() % 100) < 60 ) {
		// swap two assignments
		POPTYPE t;
		int pa, pb;
		pa = random() % numZips;
		pb = random() % numZips;
		t = dest[pa];
		dest[pa] = dest[pb];
		dest[pb] = t;
	} else {
		//change one assignment
		int pa = random() % numZips;
		dest[pa] = random() % districts;
	}
}

void mate( POPTYPE* dest, POPTYPE* a, POPTYPE* b ) {
	// crossover
	int crossPoint = random() % numZips;
	int i;
	for ( i = 0; i < crossPoint; i++ ) {
		dest[i] = a[i];
	}
	for ( ; i < numZips; i++ ) {
		dest[i] = b[i];
	}
	// mutation
	mutate( dest );
	i = 10;
	while ( (random() % 1000) < 100 ) {
		mutate( dest );
		if ( --i == 0 ) {
			// hit max mutations.
			break;
		}
	}
}

void writeDistrictText( FILE* distf, POPTYPE* winner ) {
	int i, j;
	for ( i = 0; i < districts; i++ ) {
		double dpop = 0.0;
		fprintf( distf, "district %d:",i);
		for ( j = 0; j < numZips; j++ ) {
			if ( winner[j] == i ) {
				fprintf( distf, " %d",j);
				dpop += zipd[j].pop;
			}
		}
		fprintf( distf, " (pop=%f)\n", dpop );
	}
	fflush( distf );
}
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

#if 01
/* two pass fitness that adjusts for the current range of moments and popvars*/
#define SCALE_P_M_FACTORS 1
double pvsmin;
double msmin;
double pvsmax, msmax;
void testFitness( int start ) {
	int i;
	pvsmin = HUGE_VAL;
	msmin = HUGE_VAL;
	pvsmax = -HUGE_VAL;
	msmax = -HUGE_VAL;
	fitavg = 0.0;
	int si;
	for ( si = 0; si < start; si++ ) {
		i = sorti[si];
		if ( popvars[i] < pvsmin ) {
			pvsmin = popvars[i];
		}
		if ( moments[i] < msmin ) {
			msmin = moments[i];
		}
		if ( popvars[i] > pvsmax ) {
			pvsmax = popvars[i];
		}
		if ( moments[i] > msmax ) {
			msmax = moments[i];
		}
	}
	for ( si = start; si < popsize; si++ ) {
		i = sorti[si];
		POPTYPE* a = pop + (i*numZips);
		popvars[i] = getPopvar( a );
		moments[i] = getMoment( a );
		if ( popvars[i] < pvsmin ) {
			pvsmin = popvars[i];
		}
		if ( moments[i] < msmin ) {
			msmin = moments[i];
		}
		if ( popvars[i] > pvsmax ) {
			pvsmax = popvars[i];
		}
		if ( moments[i] > msmax ) {
			msmax = moments[i];
		}
	}
	double pvsdiff, msdiff;
	pvsdiff = pvsmax - pvsmin;
	msdiff = msmax - msmin;
	for ( i = 0; i < popsize; i++ ) {
		double mspart, pvspart;
		mspart = (moments[i] - msmin) / msdiff;
		pvspart = (popvars[i] - pvsmin) / pvsdiff;
		fitness[i] = pvspart + mspart;
		fitavg += fitness[i];
		//printf("score %4d: %f\n", i, fitness[i] );
	}
	fitavg /= popsize;
}
#elif 01
/* two pass fitness that adjusts for the current range of moments and popvars*/
#define SCALE_P_M_FACTORS 1
double pvsmin;
double msmin;
void testFitness( int start ) {
	int i;
	pvsmin = HUGE_VAL;
	msmin = HUGE_VAL;
	fitavg = 0.0;
	int si;
	for ( si = 0; si < start; si++ ) {
		i = sorti[si];
		if ( popvars[i] < pvsmin ) {
			pvsmin = popvars[i];
		}
		if ( moments[i] < msmin ) {
			msmin = moments[i];
		}
	}
	for ( si = start; si < popsize; si++ ) {
		i = sorti[si];
		POPTYPE* a = pop + (i*numZips);
		popvars[i] = getPopvar( a );
		moments[i] = getMoment( a );
		if ( popvars[i] < pvsmin ) {
			pvsmin = popvars[i];
		}
		if ( moments[i] < msmin ) {
			msmin = moments[i];
		}
	}
	// make it so the trail with the best moment or popvar doesn't have zero fitness
	msmin -= 1.0;
	pvsmin -= 1.0;
	for ( i = 0; i < popsize; i++ ) {
		double mspart, pvspart;
		mspart = moments[i] - msmin;
		pvspart = popvars[i] - pvsmin;
#if 0
		fitness[i] = pvspart * mspart * mspart * mspart;
#elif 01
		fitness[i] = pvspart * mspart * mspart;
#else
		fitness[i] = pvspart * mspart;
#endif
		fitavg += fitness[i];
		//printf("score %4d: %f\n", i, fitness[i] );
	}
	fitavg /= popsize;
}
#else
void testFitness( int start ) {
	int i;
	fitavg = 0.0;
	int si;
	for ( si = start; si < popsize; si++ ) {
		i = sorti[si];
		POPTYPE* a = pop + (i*numZips);
		popvars[i] = getPopvar( a );
		moments[i] = getMoment( a );
#if 01
		fitness[i] = moments[i];
#elif 01
		fitness[i] = popvars[i] * moments[i] * moments[i] * moments[i] * moments[i];
#elif 01
		fitness[i] = popvars[i] * moments[i] * moments[i] * moments[i];
#elif 01
		fitness[i] = popvars[i] * moments[i] * moments[i];
#elif 01
		fitness[i] = popvars[i] * moments[i];
#endif
		fitavg += fitness[i];
		//printf("score %4d: %f\n", i, fitness[i] );
	}
	fitavg /= popsize;
}
#endif

void sortFitnessFully() {
	int i;
	// bubble sort
	bool notdone = true;
	while ( notdone ) {
		notdone = false;
		for ( i = 1; i < popsize; i++ ) {
			if ( fitness[sorti[i-1]] > fitness[sorti[i]] ) {
				int t = sorti[i-1];
				sorti[i-1] = sorti[i];
				sorti[i] = t;
				notdone = true;
			}
		}
	}
}

void sortFitness() {
	int i;
	// insertion sort, customized to only care about the top n
	for ( i = 1; i < popsize; i++ ) {
		double cf;
		int csi;
		int kp;
		csi = sorti[i];
		cf = fitness[csi];
		if ( i < topn ) {
			kp = i - 1;
		} else {
			kp = topn-1;
		}
		if ( cf < fitness[sorti[kp]] ) {
			sorti[i] = sorti[kp];
			kp--;
			while ( (kp >= 0) && (cf < fitness[sorti[kp]]) ) {
				sorti[kp+1] = sorti[kp];
				kp--;
			} 
			sorti[kp+1] = csi;
		}
	}
}

int main( int argc, char** argv ) {
	int i;
	char* inputname = "data/zcta5.txt";
	int g;
	int generations = 1000;
	char* dumpname = NULL;
	char* loadname = NULL;
	FILE* loadfile = NULL;
	FILE* dumpfile = NULL;

	FILE* distf = NULL;
	FILE* coordf = NULL;

	char* pngname = NULL;
	int pngWidth = 1000, pngHeight = 1000;

	GeoData* gd;
	GeoData* (*geoFact)(char*) = openZCTA;

	blaf = stdout;
	
	for ( i = 1; i < argc; i++ ) {
		if ( ! strcmp( argv[i], "-i" ) ) {
			i++;
			inputname = argv[i];
		} else if ( ! strcmp( argv[i], "-U" ) ) {
			i++;
			inputname = argv[i];
			geoFact = openUf1;
		} else if ( ! strcmp( argv[i], "-g" ) ) {
			i++;
			generations = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "-p" ) ) {
			i++;
			popsize = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "-d" ) ) {
			i++;
			districts = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "-o" ) ) {
			i++;
			dumpname = argv[i];
		} else if ( ! strcmp( argv[i], "-r" ) ) {
			i++;
			loadname = argv[i];
		} else if ( ! strcmp( argv[i], "--distout" ) ) {
			i++;
			distf = fopen( argv[i], "w" );
			if ( distf == NULL ) {
			    perror( argv[i] );
			    exit(1);
			}
		} else if ( ! strcmp( argv[i], "--coordout" ) ) {
			i++;
			coordf = fopen( argv[i], "w" );
			if ( coordf == NULL ) {
			    perror( argv[i] );
			    exit(1);
			}
		} else if ( ! strcmp( argv[i], "--pngout" ) ) {
			i++;
			pngname = argv[i];
		} else if ( ! strcmp( argv[i], "--pngW" ) ) {
			i++;
			pngWidth = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "--pngH" ) ) {
			i++;
			pngHeight = atoi( argv[i] );
		} else if ( ! strcmp( argv[i], "-q" ) ) {
			blaf = NULL;
			if ( distf == stdout ) {
				distf = NULL;
			}
		} else {
			fprintf( stderr, "bogus arg \"%s\"\n", argv[i] );
			exit(1);
		}
	}
	
	gd = geoFact( inputname );
	numZips = gd->read( &zipd );
	totalpop = 0.0;
	for ( i = 0; i < numZips; i++ ) {
		totalpop += zipd[i].pop;
	}
	
	scoretmp = new PosPop[districts];

	if ( loadname != NULL ) {
		int fnz, fileversion;
		loadfile = fopen( loadname, "rb" );
		if ( loadfile == NULL ) {
			perror( loadname );
			exit(1);
		}
		fread( &fileversion, sizeof(int), 1, loadfile );
		if ( fileversion != 1 ) {
			fprintf( stderr, "bad file version \"%d\", wanted \"%d\"\n", fileversion, 1 );
			exit(1);
		}
		fread( &fnz, sizeof(int), 1, loadfile );
		if ( fnz != numZips ) {
			fprintf( stderr, "stored numZips=%d doesn't match loaded numZips=%d\n", fnz, numZips );
			exit(1);
		}
		fread( &popsize, sizeof(int), 1, loadfile );
		fread( &districts, sizeof(int), 1, loadfile );
	}
	
	pop = new POPTYPE[popsize*numZips];
	fitness = new double[popsize];
	moments = new double[popsize];
	popvars = new double[popsize];
	sorti = new int[popsize];
	topn = popsize / 10;
	districtPopTarget = totalpop / districts;
	printf("total pop %f, %f / %d districts\n", totalpop, districtPopTarget, districts );
	
	srandom(time(NULL));
	if ( loadname != NULL ) {
		fread( pop, sizeof(POPTYPE), popsize*numZips, loadfile );
		fclose( loadfile );
	} else /*if ( loadname == NULL )*/ {
		for ( i = 0; i < popsize*numZips; i++ ) {
			pop[i] = random() % districts;
		}
	}
	for ( i = 0; i < popsize; i++ ) {
		sorti[i] = i;
	}

	/* loop is structured this way so that generations=0 can be used to spew text from a loaded dump file */
	g = 0;
	while ( true ) {
		testFitness( (g == 0) ? 0 : topn );
		// sort fitness
		/*if ( g == 0 ) {
			sortFitnessFully();
		} else {*/
			sortFitness();
		//}
		if ( g >= generations ) break;
		if ( blaf != NULL ) {
#if SCALE_P_M_FACTORS
			int wsi;
			wsi = sorti[0];
			fprintf( blaf, "gen %4d: win popvar %9g win moment %9g fit %9g at %4d, fitavg %9g\n", g, popvars[wsi], moments[wsi], fitness[wsi], wsi, fitavg );
//			fprintf( blaf, "gen %4d: min popvar %9g min moment %9g best fit %9g at %4d, fitavg %9g\n", g, pvsmin, msmin, fitness[sorti[0]], sorti[0], fitavg );
#else
			fprintf( blaf, "gen %4d: fitmax %0.9g at %4d, fitavg %0.9g\n", g, fitness[sorti[0]], sorti[0], fitavg );
#endif
			fflush( blaf );
		}
		// breed
		for ( i = topn; i < popsize; i++ ) {
			int dp, pa, pb;
			dp = sorti[i];
			pa = sorti[random() % topn];
			pb = sorti[random() % topn];
			//printf("replace %4d with %4d + %4d\n", dp, pa, pb );
			POPTYPE* dest = pop + (dp * numZips);
			POPTYPE* a = pop + (pa * numZips);
			POPTYPE* b = pop + (pb * numZips);
			mate( dest, a, b );
		}
		g++;
	}
	{
		POPTYPE* winner = pop + (sorti[0] * numZips);
		if ( blaf != NULL ) {
			writeFinalSpew( winner, blaf );
		}
		if ( coordf != NULL ) {
			writeCoordText( coordf, winner );
		}
		if ( distf != NULL ) {
			writeDistrictText( distf, winner );
		}
	}
	if ( dumpname != NULL ) {
		dumpfile = fopen( dumpname, "wb" );
		if ( dumpfile == NULL ) {
			perror( dumpname );
			exit( 1 );
		}
		i = 1;
		fwrite( &i, sizeof(int), 1, dumpfile );
		fwrite( &numZips, sizeof(int), 1, dumpfile );
		fwrite( &popsize, sizeof(int), 1, dumpfile );
		fwrite( &districts, sizeof(int), 1, dumpfile );
		fwrite( pop, sizeof(POPTYPE), popsize*numZips, dumpfile );
		fclose( dumpfile );
	}
	if ( pngname != NULL ) {
		double* rs = NULL;
		POPTYPE* winner = pop + (sorti[0] * numZips);
		renderDistricts( numZips, zipd, rs, winner, pngname, pngHeight, pngWidth );
	}
	return 0;
}
