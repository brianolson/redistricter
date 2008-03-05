#include <stdio.h>
#include <math.h>
#include <string.h>

/* Radius of the earth in miles */
const double EARTH_RADIUS = 3956;

#if 01
#define degreesToRads( n ) (( n * ( M_PI / 180.0 )))
#elif 01
inline double degreesToRads( double n ) { return n * ( M_PI / 180.0 ); }
#endif
double greatCircleDistance(double lat1,double long1,double lat2,double long2);

double greatCircleDistance(double lat1,double long1,double lat2,double long2) {
	double delta_long;// = 0;
	double delta_lat;// = 0;
	double temp;// = 0;
	double distance;// = 0;
		
	/* Convert all the degrees to radians */
	lat1 = degreesToRads(lat1);
	long1 = degreesToRads(long1);
	lat2 = degreesToRads(lat2);
	long2 = degreesToRads(long2);
	
	/* Find the deltas */
	delta_lat = lat2 - lat1;
	delta_long = long2 - long1;
	
	/* Find the GC distance */
	{
#if 01
		double sdla2 = sin(delta_lat / 2.0);
		double sdlo2 = sin(delta_long / 2.0);
		temp = sdla2*sdla2 + cos(lat1) * cos(lat2) * sdlo2*sdlo2;
#else
		temp = pow(sin(delta_lat / 2.0), 2) + cos(lat1) * cos(lat2) * pow(sin(delta_long / 2.0), 2);
#endif
	}
	
	distance = EARTH_RADIUS * 2 * atan2(sqrt(temp),sqrt(1 - temp));
	return (distance);
}
