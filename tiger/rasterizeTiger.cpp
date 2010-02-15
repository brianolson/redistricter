#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "mmaped.h"
#include "recordA.h"
#include "recordI.h"
#include "record1.h"
#include "record2.h"

#include "rasterizeTiger.h"

// record type I part
#define NOPOLY ((uint32_t)-1)

static char cendef[6] = "     ";

class point {
public:
	int32_t lon, lat;
	uint32_t tzid;
	point* next;
	point* prev;
	
	bool operator==(const point& b) {
		return (lon == b.lon) && (lat == b.lat) && (tzid == b.tzid);
	}
};
point* pool = NULL;
point* newpoint( int32_t lon = 0, int32_t lat = 0, point* prev = NULL, uint32_t tzid = 0 ) {
	if ( pool == NULL ) {
		pool = new point[1000];
		for ( int i = 0; i < 999; i++ ) {
			pool[i].next = pool + (i+1);
		}
		pool[999].next = NULL;
	}
	point* toret = pool;
	pool = pool->next;
	toret->lon = lon;
	toret->lat = lat;
	toret->next = NULL;
	toret->prev = prev;
	toret->tzid = tzid;
	return toret;
}
void delpoint( point* p ) {
	p->next = pool;
	pool = p;
}

// Taken from record I
class rtip {
public:
	// TIGER/Line ID, Permanent 1-Cell Number
	uint32_t tlid;

	// TIGER ID, Start, Permanent Zero-Cell Number
	uint32_t start;

	// TIGER ID, Eind, Permanent Zero-Cell Number
	uint32_t end;
	
	// Polygon Identification Code, Left
	uint32_t polyidl;
	
	// Polygon Identification Code, Right
	uint32_t polyidr;
	
	// Census File Identification Code, Left
	char cenidl[6];
	
	// Census File Identification Code, Right
	char cenidr[6];
	
	rtip() : polyidl(NOPOLY), polyidr(NOPOLY) {
		memcpy( cenidl, cendef, 6 );
		memcpy( cenidr, cendef, 6 );
	}
};

// Record 2
class rt2 {
public:
	// TIGER/Line ID, Permanent 1-Cell Number
	uint32_t tlid;
	
	// sequence number, in case line needs multiple 10-lon-lat-pair blocks
	uint32_t seq;
	
	// 10 pairs of lon-lat
	int32_t lonlat[20];
};

// "RTAPU" = record A + polygon + ubid
class rtaPolyUbid {
public:
	uint64_t ubid;
#if 0
	uint32_t polyid;
	char cenid[6];
#endif
	point* pts;
	rtaPolyUbid() : pts(NULL) {
		//memcpy( cenid, cendef, 6 );
	}
	void clear();
	~rtaPolyUbid();
};

PointOutput::~PointOutput() {}




PolyGroup::PolyGroup() : ifrootname( NULL ),
nameA( NULL ), nameI( NULL ), name1( NULL ), name2( NULL ),
numi( 0 ), numa( 0 ), num1( 0 ), num2( 0 ),
rtippool( NULL ),
shapes( NULL ),
rtapu( NULL ),
minlat(360.0), minlon(360.0), maxlat(-360.0), maxlon(-360.0),
minlatset(0), minlonset(0), maxlatset(0), maxlonset(0),
xpx(1280), ypx(960), maskpx(NULL),
maskgrey(0xff)
{}

#define pclear(p) if ( (p) != NULL ) { free( p ); p = NULL; }
#define aclear(a) if ( (a) != NULL ) { delete [] a; a = NULL; }

void PolyGroup::clear() {
	pclear( ifrootname );
	pclear( nameA );
	pclear( nameI );
	pclear( name1 );
	pclear( name2 );
	aclear( rtippool );
	aclear( shapes );
	if ( rtapu != NULL ) {
		for ( int i = 0; i < numa; i++ ) {
			rtapu[i].clear();
		}
		free( rtapu );
		rtapu = NULL;
	}
	numi = numa = num1 = num2 = 0;
}

int compare_rtip( const void* a, const void* b ) {
	return ((const rtip*)a)->tlid - ((const rtip*)b)->tlid;
}

FILE* verbose = NULL;

static int vf(const char* fmt, ... ) {
	va_list ap;
	int err;
	if ( verbose == NULL ) return 0;
	va_start(ap,fmt);
	err = vfprintf(verbose,fmt,ap);
	va_end(ap);
	return err;
}

void PolyGroup::buildRTI() {
	int i;
	mmaped mi;
	mi.open(nameI);
	recordI ri(mi.data);
	numi = mi.sb.st_size / recordI::size;
	vf("%s: %d records\n", nameI, numi );
	
	rtippool = new rtip[numi];
	vf("allocated %lu bytes for rtippool\n", sizeof(rtip)*numi );
	for ( i = 0; i < numi; i++ ) {
		rtip* r;
		const char* cl;
		const char* cr;
		r = rtippool + i;
		r->tlid = ri.TLID_longValue(i);
		cl = ri.CENIDL(i);
		if ( cl[0] != ' ' ) {
			memcpy( r->cenidl, cl, 5 );
			r->polyidl = ri.POLYIDL_longValue(i);
		}
		cr = ri.CENIDR(i);
		if ( cr[0] != ' ' ) {
			memcpy( r->cenidr, cr, 5 );
			r->polyidr = ri.POLYIDR_longValue(i);
		}
		r->start = ri.TZIDS_longValue(i);
		r->end = ri.TZIDE_longValue(i);
		//printf("tlid %d, %.5s%10d %.5s%10d\n", tlid, cl, l ? l->polyid : 0, cr, r ? r->polyid : 0 );
	}
	
	// sort on tlid
	qsort( rtippool, numi, sizeof(rtip), compare_rtip );
	
	mi.close();
}

rtip* PolyGroup::rtipForTLID( uint32_t tlid ) {
	int lo = 0;
	int hi = numi-1;
	int mid;
	
	while ( hi >= lo ) {
		mid = (hi+lo)/2;
		if ( rtippool[mid].tlid == tlid ) {
			return rtippool + mid;
		} else if ( rtippool[mid].tlid > tlid ) {
			hi = mid - 1;
		} else {
			lo = mid + 1;
		}
	}
	return NULL;
}

int compare_rt2( const void* ai, const void* bi ) {
	const rt2* a = (const rt2*)ai;
	const rt2* b = (const rt2*)bi;
	
	if ( a->tlid == b->tlid ) {
		return a->seq - b->seq;
	} else {
		return a->tlid - b->tlid;
	}
}

void PolyGroup::buildShapes() {
	int i;
	mmaped m2;
	m2.open(name2);
	num2 = m2.sb.st_size / record2::size;
	record2	r2(m2.data);
	vf("%s: %d records\n", name2, num2 );

	shapes = new rt2[num2];
	vf("allocated %lu bytes for shapes\n", sizeof(rt2)*num2 );
	for ( i = 0; i < num2; i++ ) {
		rt2* i2;
		i2 = shapes + i;
		i2->tlid = r2.TLID_longValue(i);
		i2->seq = r2.RTSQ_longValue(i);
		i2->lonlat[0] = r2.LONG1_longValue(i);
		i2->lonlat[1] =  r2.LAT1_longValue(i);
		i2->lonlat[2] = r2.LONG2_longValue(i);
		i2->lonlat[3] =  r2.LAT2_longValue(i);
		i2->lonlat[4] = r2.LONG3_longValue(i);
		i2->lonlat[5] =  r2.LAT3_longValue(i);
		i2->lonlat[6] = r2.LONG4_longValue(i);
		i2->lonlat[7] =  r2.LAT4_longValue(i);
		i2->lonlat[8] = r2.LONG5_longValue(i);
		i2->lonlat[9] =  r2.LAT5_longValue(i);
		i2->lonlat[10] = r2.LONG6_longValue(i);
		i2->lonlat[11] =  r2.LAT6_longValue(i);
		i2->lonlat[12] = r2.LONG7_longValue(i);
		i2->lonlat[13] =  r2.LAT7_longValue(i);
		i2->lonlat[14] = r2.LONG8_longValue(i);
		i2->lonlat[15] =  r2.LAT8_longValue(i);
		i2->lonlat[16] = r2.LONG9_longValue(i);
		i2->lonlat[17] =  r2.LAT9_longValue(i);
		i2->lonlat[18] = r2.LONG10_longValue(i);
		i2->lonlat[19] =  r2.LAT10_longValue(i);
	}
	// sort on tlid, then sequence number
	qsort( shapes, num2, sizeof(rt2), compare_rt2 );
	m2.close();
}

int PolyGroup::shapeIndexForTLID( uint32_t tlid ) {
	int lo = 0;
	int hi = num2-1;
	int mid;
	
	while ( hi >= lo ) {
		mid = (hi+lo)/2;
		if ( shapes[mid].tlid == tlid ) {
			while ( (mid > 0) && (shapes[mid-1].tlid == tlid) ) {
				mid--;
			}
			return mid;
		} else if ( shapes[mid].tlid > tlid ) {
			hi = mid - 1;
		} else {
			lo = mid + 1;
		}
	}
	return -1;
}

void rtaPolyUbid::clear() {
	point* npt;
	point* cur = pts;
	while ( cur != NULL ) {
		npt = cur->next;
		delpoint( cur );
		cur = npt;
	}
	pts = NULL;
}
rtaPolyUbid::~rtaPolyUbid() {
	clear();
}


#if 0
/* scan line at y between line a-b and c-d */
static inline int scan( int py, double y, double* a, double* b, double* c, double* d, FILE* fout, int firstout, uint64_t ubid ) {
	int px;
	double x;
	double minx, maxx;
	// intersect left line and right line with y
	minx = (a[0] - b[0]) * ((y - b[1]) / (a[1] - b[1])) + b[0];
	maxx = (c[0] - d[0]) * ((y - d[1]) / (c[1] - d[1])) + d[0];
	if ( minx > maxx ) {
		double t = minx;
		minx = maxx;
		maxx = t;
	}
	px = pcenterRight( minx );
	if ( px < 0 ) {
		px = 0;
	}
	x = pcenterX( px );
	while ( x < maxx && px < xpx ) {
		if ( fout != NULL ) {
			if ( firstout ) {
				//fprintf(fout,"%lld", ubid );
				fwrite(&ubid,sizeof(uint64_t),1,fout);
				firstout = 0;
			}
			assert( px >= 0 );
			assert( px < 0x7fff );
			assert( py >= 0 );
			assert( py < 0x7fff );
			uint16_t xy[2];
			xy[0]=px;
			xy[1]=py;
			fwrite(xy,2,2,fout);
			//fprintf(fout," %d,%d", px, py );
		}
		if ( maskpx != NULL ) {
			maskpx[py*xpx + px] = maskgrey;
		}
#if 0
		if ( upix != NULL ) {
			upix[py*xpx + px] = ubid;
		}
#endif
		px++;
		x = pcenterX( px );
	}
	return firstout;
}
#endif

// intersect line from x1,y1 to x2,y2 at y
#if 0
static __inline double intersect( double x1, double y1, double x2, double y2, double y ) {
    double x;
    //printf("intersect y=%f on (%f,%f)-(%f,%f) = ", y, x1,y1,x2,y2 );
    if ( y1 < y2 ) {
	if ( (y < y1) || (y > y2) ) {
	    //printf("out of y range\n");
	    return NAN;
	}
    } else if ( y1 > y2 ) {
	if ( (y > y1) || (y < y2) ) {
	    //printf("out of y range\n");
	    return NAN;
	}
    } else {
	if ( y != y1 ) {
	    //printf("!= y\n");
	    return NAN;
	}
    }
    x = (x1-x2) * ((y-y2) / (y1-y2)) + x2;
    if ( x1 < x2 ) {
	if ( (x < x1) || (x > x2) ) {
	    //printf("out of x range\n");
	    return NAN;
	}
    } else if ( x1 > x2 ) {
	if ( (x > x1) || (x < x2) ) {
	    //printf("out of x range\n");
	    return NAN;
	}
    } else {
	if ( x != x1 ) {
	    //printf("!= x\n");
	    return NAN;
	}
    }
    //printf("%f\n", x );
    return x;
}
#endif
#define INTERSECT_MACRO( x1,y1,x2,y2,y,x, failtarg )     if ( y1 < y2 ) {\
	if ( (y < y1) || (y > y2) ) {\
	    goto failtarg;\
	}\
    } else if ( y1 > y2 ) {\
	if ( (y > y1) || (y < y2) ) {\
	    goto failtarg;\
	}\
    } else {\
	if ( y != y1 ) {\
	    goto failtarg;\
	}\
    }\
    x = (x1-x2) * ((y-y2) / (y1-y2)) + x2;\
    if ( x1 < x2 ) {\
	if ( (x < x1) || (x > x2) ) {\
	    goto failtarg;\
	}\
    } else if ( x1 > x2 ) {\
	if ( (x > x1) || (x < x2) ) {\
	    goto failtarg;\
	}\
    } else {\
	if ( x != x1 ) {\
	    goto failtarg;\
	}\
    }

// called from reconcileRTAPUChunks
int PolyGroup::rasterizePointList( PointOutput* fout, uint64_t ubid, point* plist ) {
	int len = 0;
	int numx;
	double miny = HUGE_VAL, maxy = -HUGE_VAL;
	int i;
	point* cur = plist;
	double* points;
	double* xs;
	int py;
	double y;
	int firstout = 1;
	while ( cur != NULL ) {
		len++;
		cur = cur->next;
		assert(cur != plist);
	}
	points = new double[3*len];
	xs = points + (2*len);
	i = 0;
	cur = plist;
	while ( cur != NULL ) {
		points[i*2  ] = cur->lon / 1000000.0;
		points[i*2+1] = cur->lat / 1000000.0;
		if ( points[i*2+1] > maxy ) {
			maxy = points[i*2+1];
		}
		if ( points[i*2+1] < miny ) {
			miny = points[i*2+1];
		}
		i++;
		cur = cur->next;
	}
	assert(i==len);
	py = pcenterBelow( maxy );
	if ( py < 0 ) {
		py = 0;
	}
	y = pcenterY( py );
	//printf("rasterizing %d points, y=[%f,%f]\n", len, miny,maxy);
	while ( y >= miny && py < ypx ) {
		double t;
		numx = 0;
		for ( i = 0; i < len-1; i++ ) {
			int ni = i + 1;
#if 1
			INTERSECT_MACRO( points[i*2], points[i*2+1], points[ni*2], points[ni*2+1], y, t, loopfail );
			xs[numx] = t;
			numx++;
		    loopfail:
			;
#else
			t = intersect( points[i*2], points[i*2+1], points[ni*2], points[ni*2+1], y );
			if ( ! isnan( t ) ) {
				xs[numx] = t;
				numx++;
			}
#endif
		}
		i = len-1;
#if 1
		INTERSECT_MACRO( points[i*2], points[i*2+1], points[0], points[1], y, t, lastfail );
		xs[numx] = t;
		numx++;
	    lastfail:
#else
		t = intersect( points[i*2], points[i*2+1], points[0], points[1], y );
		if ( ! isnan( t ) ) {
			xs[numx] = t;
			numx++;
		}
#endif
		//printf("intersect y=%f (%d) %d\n", y, py, numx );
		assert( numx > 0 );
		assert( numx % 2 == 0 );
		{
		    int notdone;
		    do {
			notdone = 0;
			for ( i = 0; i < numx-1; i++ ) {
			    if ( xs[i] > xs[i+1] ) {
				t = xs[i];
				xs[i] = xs[i+1];
				xs[i+1] = t;
				notdone = 1;
			    }
			}
		    } while ( notdone );
		}
		for ( i = 0; i < numx; i += 2 ) {
		    int px;
		    double x;
		    px = pcenterRight( xs[i] );
		    if ( px < 0 ) {
			px = 0;
		    }
		    x = pcenterX( px );
		    while ( x < xs[i+1] && px < xpx ) {
			if ( fout != NULL ) {
#if 01
				fout->writePoint( ubid, px, py );
#else
			    if ( firstout ) {
				//fprintf(fout,"%lld", ubid );
				fwrite(&ubid,sizeof(uint64_t),1,fout);
				firstout = 0;
			    }
			    assert( px >= 0 );
			    assert( px < 0x7fff );
			    assert( py >= 0 );
			    assert( py < 0x7fff );
			    uint16_t xy[2];
			    xy[0]=px;
			    xy[1]=py;
			    fwrite(xy,2,2,fout);
			    //fprintf(fout," %d,%d", px, py );
#endif
			}
			if ( maskpx != NULL ) {
			    maskpx[py*xpx + px] = maskgrey;
			}
#if 0
			if ( upix != NULL ) {
			    upix[py*xpx + px] = ubid;
			}
#endif
			px++;
			x = pcenterX( px );
		    }
		}
		py++;
		y = pcenterY( py );
	}
#if 0
	if ( ! firstout ) {
		static uint16_t end[2] = {
			0xffff,0xffff
		};
		fwrite(end,2,2,fout);
		//fprintf(fout,"\n");
	} else {
		vf("no pixels\n");
	}
#endif
	delete [] points;
	return !firstout;
}

// called from reconcileRTAPUChunks
int PolyGroup::printPointList( PointOutput* fout, uint64_t ubid, point* plist ) {
	point* cur = plist;
	do {
		fout->writePoint( ubid, cur->lon, cur->lat );
		cur = cur->next;
	} while ( (cur != NULL) && (cur != plist) );
	return 0;
}

// called from reconcileRTAPUChunks
int PolyGroup::pointListNOP( PointOutput* fout, uint64_t ubid, point* plist ) {
	return 0;
}

int compare_rtapu( const void* ai, const void* bi ) {
	const rtaPolyUbid* a = (const rtaPolyUbid*)ai;
	const rtaPolyUbid* b = (const rtaPolyUbid*)bi;
	
	if ( a->ubid > b->ubid ) {
		return 1;
	} else if ( a->ubid == b->ubid ) {
		return 0;
	} else {
		return -1;
	}
}

rtaPolyUbid* PolyGroup::rtapuForUbid( uint64_t ubid ) {
	int lo = 0;
	int hi = numa-1;
	int mid;
	
	while ( hi >= lo ) {
		mid = (hi+lo)/2;
		if ( rtapu[mid].ubid == ubid ) {
			return rtapu + mid;
		} else if ( rtapu[mid].ubid > ubid ) {
			hi = mid - 1;
		} else {
			lo = mid + 1;
		}
	}
	if ( numa == 0 ) {
		rtapu = (rtaPolyUbid*)malloc( sizeof(rtaPolyUbid) );
		assert(rtapu != NULL);
		rtapu->ubid = ubid;
		rtapu->pts = NULL;
		numa = 1;
		return rtapu;
	}
	numa++;
	rtaPolyUbid* np = (rtaPolyUbid*)realloc( rtapu, sizeof(rtaPolyUbid)*numa );
	if ( np == NULL ) {
	    vf("realloc fails size %lu\n", sizeof(rtaPolyUbid)*numa );
		exit(1);
	}
	rtapu = np;
	hi = numa-1;
	while ( (hi > 0) && (rtapu[hi-1].ubid > ubid) ) {
		rtapu[hi] = rtapu[hi-1];
		hi--;
	}
	rtapu[hi].ubid = ubid;
	rtapu[hi].pts = NULL;
	return rtapu + hi;
}


void printPointList( FILE* f, point* cur ) {
	point* start = cur;
	while ( cur != NULL ) {
		if ( cur->tzid != 0 ) {
			fprintf(f," %d,%d (%d)", cur->lon, cur->lat, cur->tzid );
		} else
		fprintf(f," %d,%d", cur->lon, cur->lat );
		
		cur = cur->next;
		if ( cur == start ) {
			fprintf(f," loop");
			break;
		}
	}
}

void printRTAPU( FILE* f, rtaPolyUbid* r ) {
	//fprintf(f,"%11llu %.5s%10u", r->ubid , r->cenid, r->polyid );
	fprintf(f,"%11llu", r->ubid );
	printPointList( f, r->pts );
	fprintf(f,"\n");
}

void PolyGroup::addpointstortapu( rtaPolyUbid* rp, int reverse, int si, uint32_t tlid,
					   uint32_t startlon, int32_t startlat, int32_t endlon, int32_t endlat,
					   uint32_t tzidstart, uint32_t tzidend ) {
	point* ns;
	point* tail;
	ns = newpoint( startlon, startlat, NULL, tzidstart );
	tail = ns;
	if ( si >= 0 ) {
		while ( shapes[si].tlid == tlid ) {
			int pi = 0;
			rt2* cs;
			cs = shapes+si;
			while ( (pi < 10) && ((cs->lonlat[pi*2] != 0) || (cs->lonlat[pi*2+1] != 0)) ) {
				//printf("%11llu %.5s%10u %d,%d\n", rp->ubid , rp->cenid, rp->polyid, cs->lonlat[pi*2], cs->lonlat[pi*2 + 1] );
				tail->next = newpoint( cs->lonlat[pi*2], cs->lonlat[pi*2 + 1], tail, 0 );
				tail = tail->next;
				pi++;
			}
			si++;
		}
	}
	tail->next = newpoint( endlon, endlat, tail, tzidend );
	assert( tail != tail->next );
	assert( tail->next != ns );
	tail = tail->next;
	if ( reverse ) {
		point* nn;
		ns = tail;
		nn = tail->prev;
		ns->prev = NULL;
		do {
			tail->next = nn;
			nn = nn->prev;
			tail->next->prev = tail;
			tail = tail->next;
		} while ( nn != NULL );
		tail->next = NULL;
	}
	assert( ns != tail );
	point* cur = ns;
	while ( cur != NULL ) {
		double lat, lon;
		lat = cur->lat / 1000000.0;
		lon = cur->lon / 1000000.0;
		if ( (! minlonset) && lon < minlon ) {
			minlon = lon;
		}
		if ( (! maxlonset ) && lon > maxlon ) {
			maxlon = lon;
		}
		if ( (! minlatset ) && lat < minlat ) {
			minlat = lat;
		}
		if ( (! maxlatset ) && lat > maxlat ) {
			maxlat = lat;
		}
		cur = cur->next;
	}
	tail->next = rp->pts;
	// don't link prev, as marker of chunk split
	rp->pts = ns;
	//printRTAPU( stderr, rp );
}

FILE* loopf = NULL;

void PolyGroup::reconcileRTAPUChunks( rtaPolyUbid* rp, PointOutput* fout,
		int (PolyGroup::* processPointList)(PointOutput*,uint64_t,point*) ) {
	int nc;
	point* cur;
	
	nc = 0;
	cur = rp->pts;
	while ( cur != NULL ) {
		if ( cur->prev == NULL ) {
			nc++;
		}
		cur = cur->next;
	}
	
	//vf("%11llu %.5s%10u %d chunks\n", rp->ubid , rp->cenid, rp->polyid, nc );
	vf("%11llu %d chunks\n", rp->ubid, nc );
	
	point** heads = new point*[nc];
	point** tails = new point*[nc];
	
	int i = 0;
	cur = rp->pts;
	if ( cur == NULL ) {
		fprintf(stderr," null list\n");
		return;
	}
	heads[i] = cur;
	point* prev;
	prev = cur;
	cur = cur->next;
	while ( cur != NULL ) {
		if ( cur->prev == NULL ) {
			prev->next = NULL;
			tails[i] = prev;
			i++;
			//vf("\n%d: ",i);
			heads[i] = cur;
		} else {
			//vf(" ");
		}
		//vf("%d,%d", cur->lon, cur->lat );
#if 0
		if ( cur->tzid != 0 ) {
			vf("(%d)",cur->tzid);
		}
#endif
		prev = cur;
		cur = cur->next;
	}
	assert(i== nc-1);
	prev->next = NULL;
	tails[i] = prev;
	//vf("\n");

	// merge line strips and points for this polygon
	for ( int last = nc-1; last >= 0; last-- ) {
		for ( i = 0; i < last; i++ ) {
			if ( *(heads[last]) == *(tails[i]) ) {
				//vf("merge %d head to %d tail\n", last, i);
				tails[i]->next = heads[last]->next;
				tails[i]->next->prev = tails[i];
				delpoint( heads[last] );
				tails[i] = tails[last];
				break;
			} else if ( *(tails[last]) == *(heads[i]) ) {
				//vf("merge %d head to %d tail\n", i, last );
				tails[last]->next = heads[i]->next;
				heads[i]->next->prev = tails[last];
				//heads[i]->prev = tails[last];
				heads[i] = heads[last];
				break;
			} else if ( *(heads[last]) == *(heads[i]) ) {
				vf("merge %d head to %d head\n", last, i);
				cur = heads[last]->next;
				// reverse last into front of i
				while ( cur != NULL ) {
					point* nh;
					nh = cur->next;
					cur->next = heads[i];
					heads[i]->prev = cur;
					cur->prev = NULL;
					heads[i] = cur;
					cur = nh;
				}
				break;
			} else if ( *(tails[last]) == *(tails[i]) ) {
				vf("merge %d tail to %d tail\n", last, i);
				// reverse last onto tail of i
				cur = tails[last]->prev;
				while ( cur != NULL ) {
					point* np;
					np = cur->prev;
					cur->prev = tails[i];
					tails[i]->next = cur;
					cur->next = NULL;
					tails[i] = cur;
					cur = np;
				}
				break;
			}
		}
		if ( i >= last ) {
#if 0
			for ( i = 0; i < last + 1; i++ ) {
				vf("%d:", i );
				cur = heads[i];
				while ( cur != NULL ) {
					vf(" %d,%d", cur->lon, cur->lat );
					if ( cur->tzid != 0 ) {
						vf("(%d)",cur->tzid);
					}
					cur = cur->next;
				}
				vf("\n");
			}
#endif
		} else {
			heads[last] = NULL;
#if 0
			cur = heads[i];
			vf("%d: %d,%d", i, cur->lon, cur->lat );
			if ( cur->tzid != 0 ) {
				vf("(%d)",cur->tzid);
			}
			printPointList( stderr, cur->next );
			vf("\n");
#endif
		}
		//assert(i<nc);
	}
	int countlines = 0;
	int countloops = 0;
	for ( i = 0; i < nc; i++ ) {
		if ( heads[i] == NULL ) {
			continue;
		}
		countlines++;
		if ( *(heads[i]) == *(tails[i]) ) {
			countloops++;
			if ( loopf != NULL ) {
				fprintf(loopf,"%11llu", rp->ubid );
				cur = heads[i];
				while ( cur != NULL ) {
					fprintf(loopf," %d,%d", cur->lon, cur->lat );
					cur = cur->next;
				}
				fprintf(loopf,"\n");
			}
		} else {
			fprintf(stderr,"%11llu line %d not a loop: ", rp->ubid, i );
			::printPointList(stderr,heads[i]);
			fprintf(stderr,"\n");
		}
		(this->*processPointList)(fout,rp->ubid,heads[i]);
	}
	vf("%d lines, %d loops\n", countlines, countloops );
	
	delete [] heads;
	delete [] tails;
}

uint64_t r1ubidL( const record1& r1, int i ) {
	uint64_t toret = r1.COUNTYL_longValue(i);
	toret *= 1000000;
	toret += r1.TRACTL_longValue(i);
	toret *= 10000;
	toret += r1.BLOCKL_longValue(i);
	return toret;
}
int r1validL( const record1& r1, int i ) {
	if ( r1.COUNTYL(i)[0] == ' ' ) {
		return 0;
	}
	if ( r1.TRACTL(i)[0] == ' ' ) {
		return 0;
	}
	if ( r1.BLOCKL(i)[0] == ' ' ) {
		return 0;
	}
	return 1;
}

uint64_t r1ubidR( const record1& r1, int i ) {
	uint64_t toret = r1.COUNTYR_longValue(i);
	toret *= 1000000;
	toret += r1.TRACTR_longValue(i);
	toret *= 10000;
	toret += r1.BLOCKR_longValue(i);
	return toret;
}
int r1validR( const record1& r1, int i ) {
	if ( r1.COUNTYR(i)[0] == ' ' ) {
		return 0;
	}
	if ( r1.TRACTR(i)[0] == ' ' ) {
		return 0;
	}
	if ( r1.BLOCKR(i)[0] == ' ' ) {
		return 0;
	}
	return 1;
}

#if NEED_STRDUP
inline char* strdup( const char* in ) {
    int len = 0;
    const char* p = in;
    if ( in == NULL ) return NULL;
    while ( *p != '\0' ) {
	len++;
	p++;
    }
    len++;
    char* toret = (char*)malloc( len );
    assert(toret != NULL);
    memcpy( toret, in, len );
    return toret;
}
#endif

void PolyGroup::setRootName( char* n ) 	{
	ifrootname = strdup( n );
	assert(ifrootname != NULL);
	int rootlen = strlen( ifrootname );
	nameA = (char*)malloc( rootlen + 5 );
	assert(nameA != NULL);
	strcpy( nameA, ifrootname );
	strcat( nameA, ".RTA" );
	nameI = (char*)malloc( rootlen + 5 );
	assert(nameI != NULL);
	strcpy( nameI, ifrootname );
	strcat( nameI, ".RTI" );
	name1 = (char*)malloc( rootlen + 5 );
	assert(name1 != NULL);
	strcpy( name1, ifrootname );
	strcat( name1, ".RT1" );
	name2 = (char*)malloc( rootlen + 5 );
	assert(name2 != NULL);
	strcpy( name2, ifrootname );
	strcat( name2, ".RT2" );
}

void PolyGroup::processR1() {
	int i;
	mmaped m1;
	m1.open(name1);
	record1 r1(m1.data);
	num1 = m1.sb.st_size / record1::size;
	vf("%s: %d records\n", name1, num1 );
	
	for ( i = 0; i < num1; i++ ) {
		uint32_t tlid;
		rtip* ii;
		int si;
		int32_t startlon, startlat, endlon, endlat;
		uint64_t tubidl, tubidr;
		
		tlid = r1.TLID_longValue(i);
		ii = rtipForTLID(tlid);
		si = shapeIndexForTLID(tlid);
		
		if ( ii == NULL ) {
			printf("tlid %10d --------------- --------------- si %d\n",
				   tlid, si );
			continue;
		} else if ( 0 ) {
			printf("tlid %10d %.5s%10d %.5s%10d si %d\n",
				   tlid, ii->cenidl, ii->polyidl, ii->cenidr, ii->polyidr, si );
		}
		
		startlon = r1.FRLONG_longValue(i);
		startlat = r1.FRLAT_longValue(i);
		endlon = r1.TOLONG_longValue(i);
		endlat = r1.TOLAT_longValue(i);
		
		rtaPolyUbid* rp;
		//rp =  rtapuForPoly( ii->polyidl, ii->cenidl );
		tubidl = r1ubidL( r1, i );
		tubidr = r1ubidR( r1, i );
		if ( tubidl == tubidr ) {
			continue;
		}
		
		if ( r1validL( r1, i ) ) {
			rp = rtapuForUbid( tubidl );
			if ( rp != NULL ) {
				addpointstortapu( rp, 0, si, tlid, startlon, startlat, endlon, endlat, ii->start, ii->end );
			}
		}
		if ( r1validR( r1, i ) ) {
			rp = rtapuForUbid( tubidr );
			if ( rp != NULL ) {
				addpointstortapu( rp, 1, si, tlid, startlon, startlat, endlon, endlat, ii->start, ii->end );
			}
		}
	}
	m1.close();
}

void PolyGroup::reconcile( PointOutput* fout,
		int (PolyGroup::* processPointList)(PointOutput*,uint64_t,point*) ) {
	int i;
	for ( i = 0; i < numa; i++ ) {
		if ( rtapu[i].pts == NULL ) {
			continue;
		}
		maskgrey = ((random() & 0x1f) << 2) + 0x7f;
		reconcileRTAPUChunks( rtapu + i, fout, processPointList );
	}
}

void PolyGroup::updatePixelSize() {
	if ( maxlatset && minlatset ) {
		pixelHeight = calcPixelHeight();
	}
	if ( maxlonset && minlonset ) {
		pixelWidth = calcPixelWidth();
	}
}
void PolyGroup::updatePixelSize2() {
	if ( ! (maxlatset && minlatset) ) {
		pixelHeight = calcPixelHeight();
		maxlatset = minlatset = 1;
	}
	if ( ! (maxlonset && minlonset) ) {
		pixelWidth = calcPixelWidth();
		maxlonset = minlonset = 1;
	}
	pixelHeight = calcPixelHeight();
	pixelWidth = calcPixelWidth();
}


#if WITH_PNG

#include "png.h"

static png_voidp user_error_ptr = 0;
static void user_error_fn( png_structp png_ptr, const char* str ) {
	fprintf( stderr, "error: %s", str );
}
static void user_warning_fn( png_structp png_ptr, const char* str ) {
	fprintf( stderr, "warning: %s", str );
}

void doMaskPNG( char* outname, uint8_t* px, int width, int height ) {
	uint8_t** rows = new uint8_t*[height];
	FILE* fout;
	
    fout = fopen( outname, "wb");
    if ( fout == NULL ) {
		perror( outname );
		exit(1);
    }
	
	for ( int i = 0; i < height; i++ ) {
		rows[i] = px + (width*i);
	}
    
    png_structp png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING,
												   (png_voidp)user_error_ptr, user_error_fn, user_warning_fn );
    if (!png_ptr) {
		fclose(fout);
		exit( 2 );
    }
	
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
		png_destroy_write_struct( &png_ptr, (png_infopp)NULL );
		fclose(fout);
		exit( 2 );
    }
	
    if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fout);
		exit( 2 );
    }
	
    png_init_io(png_ptr, fout);
	
    /* set the zlib compression level */
    png_set_compression_level(png_ptr,
							  Z_BEST_COMPRESSION);
	
    png_set_IHDR(png_ptr, info_ptr, width, height,
				 /*bit_depth*/8,
				 /*color_type*/PNG_COLOR_TYPE_GRAY,
				 /*interlace_type*/PNG_INTERLACE_NONE,
				 /*compression_type*/PNG_COMPRESSION_TYPE_DEFAULT,
				 /*filter_method*/PNG_FILTER_TYPE_DEFAULT);
	
	png_write_info(png_ptr, info_ptr);
    png_write_image( png_ptr, rows );
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose( fout );
}

#endif // WITH_PNG
