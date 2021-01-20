#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "District2.h"
#include "Bitmap.h"
#include "Node.h"
#include "GeoData.h"
#include "mmaped.h"
#include <proj_api.h>

static const char usage[] =
"usage: linkfixup [-o file][-p file][--have-protobuf][Solver args]\n"
"  -p file   write out new protobuf file\n"
"  -o file   write out old '.gbin' format file\n"
"  --have-protobuf exit success if -p available, failure otherwise.\n"
"\nthis linkfixup was compiled with protobuf support\n"
;

int _aei( const char** argv, int argc, int i, const char* n ) {
	if ( i >= argc ) {
		fprintf(stderr,"expected argument but ran out after \"%s\"\n", argv[argc] );
		exit(1);
	}
	return ! strcmp( argv[i], n );
}
#define aei( n ) _aei( argv, argc, i, (n) )

static inline double dmin(double a, double b) {
	if (a < b) {
		return a;
	}
	return b;
}
static inline double dmax(double a, double b) {
	if (a > b) {
		return a;
	}
	return b;
}

class Proj {
	public:
	Proj() :
		pj_dest(NULL), pj_latlon(NULL)
	{
	}

	~Proj() {
		clear();
	}

	void initRadians(double centerLat, double centerLon) {
		char config[200];
		snprintf(config, sizeof(config), "+proj=aeqd +lat_0=%0.6f +lon_0=%0.6f", centerLat, centerLon);
		pj_dest = pj_init_plus(config);
		pj_latlon = pj_init_plus("+proj=latlong");
	}
	void initDegrees(double centerLat, double centerLon) {
		// laea Lambert Azimuthal Equal Area
		// aeqd Azimuthal Equidistant
		centerLon *= DEG_TO_RAD;
		centerLat *= DEG_TO_RAD;
		initRadians(centerLat, centerLon);
	}

	void clear() {
		if (pj_dest != NULL) {
			pj_free(pj_dest);
			pj_dest = NULL;
		}
		if (pj_latlon != NULL) {
			pj_free(pj_latlon);
			pj_latlon = NULL;
		}
	}

	void transform(double* lat, double* lon) {
		pj_transform(pj_latlon, pj_dest, 1, 1, lon, lat, NULL);
	}

	/**
	 * Transform gd->pos to be a projection
	 */
	void centerProj(GeoData* gd) {
		// sum up the weighted center of x,y,z space
		double sumx = 0.0;
		double sumy = 0.0;
		double sumz = 0.0;
		for (int i = 0; i < gd->numPoints; i++) {
			double lon = (gd->pos[i*2  ] / 1000000.0);// * DEG_TO_RAD;
			double lat = (gd->pos[i*2+1] / 1000000.0);// * DEG_TO_RAD;
			if (false && i % 100 == 0) {
				printf("lon,lat %3.6f, %3.6f\t", lon, lat);
			}
			lon *= DEG_TO_RAD;
			lat *= DEG_TO_RAD;
			double area = gd->area[i];
			double equatorialBaseLength = cos(lat);
			double x = cos(lon) * equatorialBaseLength;
			double y = sin(lon) * equatorialBaseLength;
			double z = sin(lat);
			if (false && i % 100 == 0) {
				printf("lon,lat(%.6f,%.6f) %0.6f %0.6f %0.6f\n", lon, lat, x, y, z);
			}
			x *= area;
			y *= area;
			z *= area;
			sumx += x;
			sumy += y;
			sumz += z;
		}
		//printf("vector sum %g,%g,%g\n", sumx, sumy, sumz);
		double mag = sqrt((sumx*sumx) + (sumy*sumy) + (sumz*sumz));
		//sumx /= mag;
		//sumy /= mag;
		sumz /= mag;
		// now, back to lat,lon (radians)
		double centerLon = atan2(sumy, sumx);
		double centerLat = asin(sumz);
		initRadians(centerLat, centerLon);
		printf("center lat,lon %0.6f,%0.6f\n", centerLat * RAD_TO_DEG, centerLon * RAD_TO_DEG);

		double* tempXY = new double[gd->numPoints * 2];
		double minx = HUGE_VAL;
		double maxx = -HUGE_VAL;
		double miny = HUGE_VAL;
		double maxy = -HUGE_VAL;
		for (int i = 0; i < gd->numPoints; i++) {
			double lon = (gd->pos[i*2  ] / 1000000.0) * DEG_TO_RAD;
			double lat = (gd->pos[i*2+1] / 1000000.0) * DEG_TO_RAD;
			transform(&lat, &lon);
			tempXY[i*2  ] = lon;
			tempXY[i*2+1] = lat;
			minx = dmin(minx, lon);
			maxx = dmax(maxx, lon);
			miny = dmin(miny, lat);
			maxy = dmax(maxy, lat);
		}
		double width = maxx - minx;
		double height = maxy - miny;
		printf("%g < x < %g, %g < y < %g\n", minx, maxx, miny, maxy);
		printf("width=%f, height=%f\n", width, height);
		double minside = dmin(width, height);
		double maxside = dmax(width, height);
		double scale = 1.0;
		bool doScale = false;
		// find a reasonable scale for converting out to integers.
		while (minside < 50000.0) {
			scale *= 2.0;
			minside *= 2.0;
			doScale = true;
		}
		while (maxside > 1000000000.0) {
			scale *= 0.5;
			maxside *= 0.5;
			doScale = true;
		}
		minx *= scale;
		miny *= scale;
		assert(minside > 50000.0);
		assert(maxside < 1000000000.0);
		if (doScale) {
			for (int i = 0; i < gd->numPoints; i++) {
				tempXY[i*2  ] *= scale;
				tempXY[i*2+1] *= scale;
			}
		}
		for (int i = 0; i < gd->numPoints; i++) {
			gd->pos[i*2  ] = -tempXY[i*2+1];
			gd->pos[i*2+1] = tempXY[i*2  ];
		}
		delete [] tempXY;
	}

	protected:
	projPJ pj_dest;
	projPJ pj_latlon;
};


class newedge {
public:
	int32_t a, b;
	newedge* next;

	newedge( int32_t i, int32_t j, newedge* ni = NULL ) : a( i ), b( j ), next( ni ) {}
};
newedge* neroot = NULL;
int necount = 0;


int main( int argc, const char** argv ) {
	Solver sov;
	int i, nargc;
	const char* foname = NULL;
	const char* poname = NULL;
	int maxNewEdgeCount = 100;
	bool doProj = false;

	nargc=1;

	for ( i = 1; i < argc; i++ ) {
		if ( ! strcmp( argv[i], "-o" ) ) {
			i++;
			foname = argv[i];
		} else if ( ! strcmp( argv[i], "-p" ) ) {
			i++;
			poname = argv[i];
		} else if ( ! strcmp( argv[i], "--have-protobuf" ) ) {
			exit(0);
		} else if ( ! strcmp( argv[i], "--proj" ) ) {
			doProj = true;
		} else if ( (!strcmp( argv[i], "--help" )) ||
			    (!strcmp( argv[i], "-help" )) ||
			    (!strcmp( argv[i], "-h" )) ) {
			fputs(usage, stdout);
			exit(0);
		} else {
			argv[nargc] = argv[i];
			nargc++;
		}
	}
	argv[nargc]=NULL;
	int argcout = sov.handleArgs( nargc, argv );
	if (argcout != 1) {
		fprintf( stderr, "%s: bogus arg \"%s\"\n", argv[0], argv[1] );
		fputs( usage, stderr );
		fputs( Solver::argHelp, stderr );
		exit(1);
		return 1;
	}

	if (( foname == NULL ) && ( poname == NULL ))
	{
		fprintf(stderr,"useless linkfixup, null foname\n");
		exit(1);
	}
	sov.load();
	sov.initNodes();

	GeoData* gd = sov.gd;
	Node* nodes = sov.nodes;

	if (doProj) {
		Proj warper;
		warper.centerProj(gd);
	}

	int numPoints = gd->numPoints;
	Bitmap hit(numPoints);
	hit.zero();
	int* bfsearchq = new int[numPoints];
	int bfin = 0;
	int bfout = 0;

	bfsearchq[0] = 0;
	hit.set(0);
	bfin = 1;

#define modinc(n) do { (n) = ((n + 1) % numPoints); } while ( 0 )
	int nohitcount;
	int mini;
	int minj;
	double mind;

	do {
		int bfhitcount = 0;
		while ( bfin != bfout ) {
			int nn = bfsearchq[bfout];
			Node* n = nodes + nn;
			modinc( bfout );
			for ( int ni = 0; ni < n->numneighbors; ni++ ) {
				int nin;
				nin = n->neighbors[ni];
				if ( ! hit.test(nin) ) {
					bfsearchq[bfin] = nin;
					modinc( bfin );
					hit.set( nin );
					bfhitcount++;
				}
			}
		}
		printf("bf search hit %d out of %d\n", bfhitcount, numPoints);

		mini = -1;
		minj = -1;
		mind = 1000000000.0;
		nohitcount = 0;

		for ( int i = 0; i < numPoints; i++ ) {
			if ( ! hit.test(i) ) {
				for ( int j = 0; j < numPoints; j++ ) {
					if ( hit.test(j) ) {
						double d, dx, dy;
						dx = gd->pos[i*2] - gd->pos[j*2];
						dy = gd->pos[i*2 + 1] - gd->pos[j*2 + 1];
						d = sqrt( (dx*dx) + (dy*dy) );
						if ( d < mind ) {
							mini = i;
							minj = j;
							mind = d;
						}
					}
				}
				nohitcount++;
			}
		}
		if ( necount == 0 && nohitcount == 0 ) {
			printf("none unreachable.\n");
			break;
			//exit(0);
		}
		if ( nohitcount == 0 ) {
			break;
		}
		printf("%d not reachable from 0\n", nohitcount);
		printf("add link: %d,%d\n%013lu%013lu\n", mini, minj, gd->ubidOfIndex(mini), gd->ubidOfIndex(minj) );
		neroot = new newedge( mini, minj, neroot );
		necount++;
		if (necount > maxNewEdgeCount) {
			fprintf(stderr, "too many new edges, %d > limit %d\n", necount, maxNewEdgeCount);
			exit(1);
			return 1;
		}

		bfsearchq[0] = mini;
		bfin = 1;
		bfout = 0;
		hit.set( mini );
	} while ( 1 );

	if ( necount > 0 ) {
		printf("writing %d new links to \"%s\" and/or \"%s\"\n", necount, foname, poname );
		int32_t* newe = new int32_t[(sov.numEdges + necount) * 2];
		memcpy( newe, sov.edgeData, sizeof(int32_t)*(sov.numEdges * 2) );
		delete [] sov.edgeData;
		sov.edgeData = newe;
		while ( neroot != NULL ) {
			newe[sov.numEdges*2] = neroot->a;
			newe[sov.numEdges*2 + 1] = neroot->b;
			neroot = neroot->next;
			sov.numEdges++;
		}
	}
	if ( foname != NULL ) {
		sov.writeBin(foname);
	}
	if ( poname != NULL ) {
		sov.writeProtobuf(poname);
	}

	delete [] bfsearchq;

	return 0;
}
