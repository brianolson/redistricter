#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "place2k.h"

double greatCircleDistance(double lat1,double long1,double lat2,double long2);

typedef struct {
	double lat, lon;
} LatLong;

int main( int argc, char** argv ) {
	int fd;
	struct stat sb;
	caddr_t data;
	int err;
	int i, j;
	char buf[128];
	int numZips;
	LatLong* pos;
	int* closest;
	double min;
	
	fd = open( "data/zcta5.txt", O_RDONLY );
	if ( fd <= 0 ) {
		perror("open");
		exit(1);
	}
	err = fstat( fd, &sb );
	data = mmap( NULL, sb.st_size, PROT_READ, MAP_FILE, fd, 0 );
	if ( (signed long)data == -1 ) {
		perror("fstat");
		exit(1);
	}
	numZips = sb.st_size / sizeof_ZCTA;
	printf("there are %d zips\n", numZips );
	pos = (LatLong*)malloc( sizeof(LatLong) * numZips );
	closest = (int*)malloc( sizeof(int) * numZips );
	buf[5] = '\0';
	for ( i = 0; i < numZips; i++ ) {
		//char* nameptr;
		//nameptr = (char*)(((unsigned long)data) + sizeof_ZCTA*i + ZCTA_NAME_OFFSET);
		//memcpy( buf, nameptr, 5 );
		//printf("%5s\n", buf );
		//buf[10] = '\0';
		copyLatitude( buf, data, i );
		pos[i].lat = atof( buf );
		copyLongitude( buf, data, i );
		pos[i].lon = atof( buf );
	}
	for ( i = 0; i < numZips; i++ ) {
		closest[i] = -1;
		min = 100000;
		for ( j = 0; j < numZips; j++ ) if ( i != j ) {
			double cur;
			cur = greatCircleDistance( pos[i].lat, pos[i].lon, pos[j].lat, pos[j].lon );
			if ( cur < min ) {
				min = cur;
				closest[i] = j;
			}
		}
	}
		
	for ( i = 0; i < numZips; i++ ) {
		printf("%-6d%-6d\n",i, closest[i]);
	}
	err = munmap( data, sb.st_size );
	if ( err != 0 ) {
		perror("munmap");
		exit(1);
	}
	err = close( fd );
	if ( err != 0 ) {
		perror("close");
		exit(1);
	}
}
