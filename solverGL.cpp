#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>
#include "Solver.h"
#include <OpenGL/gl.h>
#include "GrabIntermediateStorage.h"
#include "District2GLDebug.h"
#include "NodeColoring.h"

extern const unsigned char* colors;
extern const int numColors;

extern "C" void glDrawSolver( void* s );

void glDrawSolver( void* s ) {
	((Solver*)s)->drawGL();
}

inline long long getTimeOfDayLLuSec() {
	struct timeval tv;
	long long toret;
	if ( gettimeofday( &tv, NULL ) != 0 ) {
		return -1;
	}
	toret = tv.tv_sec;
	toret *= 1000000;
	return toret + tv.tv_usec;
}

extern "C" void glutStrokeString( char* str, int align );

#define NON_EDGE_RATE (0.8 / 255.0)
#define LINK_RATE (0.7 / 255.0)

#if 01
#define glebail(n) n
#else
#define glebail(n) do { (n); GLenum gle = glGetError(); if ( gle != GL_NO_ERROR ) { printf("%s:%d gl error %d (0x%x) doing \"%s\"\n", __FILE__, __LINE__, gle, gle, #n ); }} while(0)
#endif

#if 0
static void drawLinksOld( int numEdges, long* edgeData, GeoData* gd, double zoom, double glminx, double glmaxx, double glminy, double glmaxy, POPTYPE* winner ) {
	glBegin(GL_LINES);
	for ( int j = 0; j < numEdges; j++ ) {
		int ea, eb;
		ea = edgeData[j*2  ];
		eb = edgeData[j*2+1];
		double vax, vay;
		vax = gd->pos[ea*2];
		vay = gd->pos[ea*2+1];
		double vbx, vby;
		vbx = gd->pos[eb*2];
		vby = gd->pos[eb*2+1];
		// do some of my own clipping
		if ( (zoom <= 1.9) || 
			 ((vax >= glminx) && (vax <= glmaxx) && (vay >= glminy) && (vay <= glmaxy)) ||
			 ((vbx >= glminx) && (vbx <= glmaxx) && (vby >= glminy) && (vby <= glmaxy)) ) {
			int da, db;
			const unsigned char* color;
			da = winner[ea];
			db = winner[eb];
			if ( da == db ) {
				color = colors + ((da % numColors) * 3);
			} else {
				static unsigned char white[3] = {
					255,255,255
				};
				color = white;
			}
			glColor3d( color[0] * LINK_RATE, color[1] * LINK_RATE, color[2] * LINK_RATE );
			glVertex2d( vax, vay );
			glVertex2d( vbx, vby );
		}
	}
	glEnd();
}

static void drawLinksArrayA( int numEdges, long* edgeData, GeoData* gd, double zoom, double glminx, double glmaxx, double glminy, double glmaxy, POPTYPE* winner, GLfloat** varrayP ) {
	GLfloat* varray = *varrayP;
	if ( varray == NULL ) {
		*varrayP = varray = (GLfloat*)malloc( sizeof(GLfloat) * numEdges * 2 * 6 );
		assert( varray );
		for ( int j = 0; j < numEdges; j++ ) {
			int ea, eb;
			ea = edgeData[j*2  ];
			eb = edgeData[j*2+1];
			double vax, vay;
			vax = gd->pos[ea*2];
			vay = gd->pos[ea*2+1];
			double vbx, vby;
			vbx = gd->pos[eb*2];
			vby = gd->pos[eb*2+1];
			varray[j*12 +  3] = vax;
			varray[j*12 +  4] = vay;
			varray[j*12 +  5] = 0;
			varray[j*12 +  9] = vbx;
			varray[j*12 + 10] = vby;
			varray[j*12 + 11] = 0;
		}
	}
	for ( int j = 0; j < numEdges; j++ ) {
		int ea, eb;
		ea = edgeData[j*2  ];
		eb = edgeData[j*2+1];
		// do some of my own clipping
		/*if ( (zoom <= 1.9) || 
			((vax >= glminx) && (vax <= glmaxx) && (vay >= glminy) && (vay <= glmaxy)) ||
			((vbx >= glminx) && (vbx <= glmaxx) && (vby >= glminy) && (vby <= glmaxy)) )*/
		{
			int da, db;
			const unsigned char* color;
			da = winner[ea];
			db = winner[eb];
#if 0
			if ( da == db ) {
				color = colors + ((da % numColors) * 3);
			} else {
				static unsigned char white[3] = {
					255,255,255
				};
				color = white;
			}
#endif
			// GL_C3F_V3F
			color = colors + ((da % numColors) * 3);
			varray[j*12 +  0] = color[0] * LINK_RATE;
			varray[j*12 +  1] = color[1] * LINK_RATE;
			varray[j*12 +  2] = color[2] * LINK_RATE;
			color = colors + ((db % numColors) * 3);
			varray[j*12 +  6] = color[0] * LINK_RATE;
			varray[j*12 +  7] = color[1] * LINK_RATE;
			varray[j*12 +  8] = color[2] * LINK_RATE;
		}
	}
	glInterleavedArrays( GL_C3F_V3F, sizeof(GLfloat) * 6, varray );
	glDrawArrays( GL_LINES, 0, numEdges*2 );
}

static void drawLinksArrayB( int numEdges, long* edgeData, GeoData* gd, double zoom, double glminx, double glmaxx, double glminy, double glmaxy, POPTYPE* winner, GLfloat** varrayP ) {
	GLfloat* varray = *varrayP;
	if ( varray == NULL ) {
		*varrayP = varray = (GLfloat*)malloc( sizeof(GLfloat) * gd->numPoints * 3 );
		assert( varray );
	}
	for ( int j = 0; j < gd->numPoints; j++ ) {
		int da;
		const unsigned char* color;
		da = winner[j];
		// GL_C3F_V3F
		color = colors + ((da % numColors) * 3);
		varray[j*3 +  0] = color[0] * LINK_RATE;
		varray[j*3 +  1] = color[1] * LINK_RATE;
		varray[j*3 +  2] = color[2] * LINK_RATE;
	}

#if READ_DOUBLE_POS
#define DLAB_VT GL_DOUBLE
#elif READ_INT_POS
#define DLAB_VT GL_INT
#else
#error what kind of pos?
#endif

	glebail(glVertexPointer( 2, DLAB_VT, 0, gd->pos ));
	glebail(glColorPointer( 3, GL_FLOAT, 0, varray ));
	glebail(glEnableClientState( GL_VERTEX_ARRAY ));
	glebail(glEnableClientState( GL_COLOR_ARRAY ));
	glebail(glDrawElements( GL_LINES, numEdges * 2, GL_UNSIGNED_INT, edgeData ));
	glebail(glDisableClientState( GL_VERTEX_ARRAY ));
	glebail(glDisableClientState( GL_COLOR_ARRAY ));
}

static void drawLinksArrayC( int numEdges, long* edgeData, GeoData* gd, double zoom, double glminx, double glmaxx, double glminy, double glmaxy, POPTYPE* winner, GLfloat** varrayP ) {
	GLuint* barray = (GLuint*)(*varrayP);
	GLfloat* varray = NULL;
	if ( barray == NULL ) {
		GLfloat* vd;
		GLuint* ed;
		barray = (GLuint*)malloc( sizeof(GLuint) * 3 );
		*varrayP = (GLfloat*)barray;
		glGenBuffersARB( 3, barray );
		/* Vertex position data */
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, barray[0] );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, sizeof(GLfloat)*4*gd->numPoints, NULL, GL_STATIC_DRAW_ARB );
		glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		glGetBufferPointervARB( GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&vd) );
		for ( int j = 0; j < gd->numPoints; j++ ) {
			vd[j*4+0] = gd->pos[j*2+0];
			vd[j*4+1] = gd->pos[j*2+1];
			vd[j*4+2] = 0;
			vd[j*4+3] = 1.0;
		}
		glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
		/* Color data (allocate, set per cycle) */
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, barray[1] );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, sizeof(GLfloat)*4*gd->numPoints, NULL, GL_DYNAMIC_DRAW_ARB );
		/* Edge list data */
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, barray[2] );
		glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLuint)*2*numEdges, NULL, GL_STATIC_DRAW_ARB );
		glMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		glGetBufferPointervARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&ed) );
		for ( int j = 0; j < numEdges * 2; j++ ) {
			ed[j] = edgeData[j];
		}
		glUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	} else {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, barray[1] );
	}
	if ( 1/*lastGenDrawn != gencount*/ ) {
		glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		glGetBufferPointervARB( GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&varray) );
		for ( int j = 0; j < gd->numPoints; j++ ) {
			int da;
			const unsigned char* color;
			da = winner[j];
			// GL_C3F_V3F
			color = colors + ((da % numColors) * 3);
			varray[j*4 +  0] = color[0] * LINK_RATE;
			varray[j*4 +  1] = color[1] * LINK_RATE;
			varray[j*4 +  2] = color[2] * LINK_RATE;
			varray[j*4 +  4] = 1.0;
		}
		glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
		//lastGenDrawn = gencount;
	}

#define BUFFER_OFFSET(i) ((char *)NULL + (i))
	glebail(glColorPointer( 4, GL_FLOAT, 0, BUFFER_OFFSET(0) ));
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, barray[0] );
	glebail(glVertexPointer( 4, GL_FLOAT, 0, BUFFER_OFFSET(0) ));
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	glebail(glEnableClientState( GL_VERTEX_ARRAY ));
	glebail(glEnableClientState( GL_COLOR_ARRAY ));
	glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, barray[2] );
	glebail(glDrawElements( GL_LINES, numEdges * 2, GL_UNSIGNED_INT, 0 ));
	glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	glebail(glDisableClientState( GL_VERTEX_ARRAY ));
	glebail(glDisableClientState( GL_COLOR_ARRAY ));
}
#endif

void Solver::initGL() {
	GLfloat* vd;
	GLuint* ed;
	GLuint barray[4];
	glGenBuffersARB( 4, barray );
	vertexBufferObject = barray[0];
	colorBufferObject = barray[1];
	linesBufferObject = barray[2];
	vertexIndexObject = barray[3];
	/* Vertex position data */
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBufferObject );
	glBufferDataARB( GL_ARRAY_BUFFER_ARB, sizeof(GLfloat)*4*gd->numPoints, NULL, GL_STATIC_DRAW_ARB );
	glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	glGetBufferPointervARB( GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&vd) );
	assert(vd != NULL);
	for ( int j = 0; j < gd->numPoints; j++ ) {
		vd[j*4+0] = gd->pos[j*2+0];
		vd[j*4+1] = gd->pos[j*2+1];
		vd[j*4+2] = 0;
		vd[j*4+3] = 1.0;
	}
	glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
	/* Vertex index data, to draw all points */
	glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, vertexIndexObject );
	glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLuint)*1*gd->numPoints, NULL, GL_STATIC_DRAW_ARB );
	glMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	glGetBufferPointervARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&ed) );
	assert(ed != NULL);
	for ( int j = 0; j < gd->numPoints; j++ ) {
		ed[j] = j;
	}
	glUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );
	glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	/* Color data (allocate, set per cycle) */
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, colorBufferObject );
	glBufferDataARB( GL_ARRAY_BUFFER_ARB, sizeof(GLfloat)*4*gd->numPoints, NULL, GL_DYNAMIC_DRAW_ARB );
	/* Edge list data */
	glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, linesBufferObject );
	glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLuint)*2*numEdges, NULL, GL_STATIC_DRAW_ARB );
	glMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	glGetBufferPointervARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&ed) );
	assert(ed != NULL);
	for ( unsigned int j = 0; j < numEdges * 2; j++ ) {
		ed[j] = edgeData[j];
	}
	glUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );
	glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
}

class ColorNodeByDistrict : public NodeColoring {
public:
	virtual ~ColorNodeByDistrict(){}
virtual void colorNode(const Solver* sov, int index, GLfloat* varray) {
	int da;
	const unsigned char* color;
	da = sov->winner[index];
	// GL_C3F_V3F
	if ( da == NODISTRICT ) {
		varray[0] = 0.2;
		varray[1] = 0.2;
		varray[2] = 0.2;
	} else {
		color = colors + ((da % numColors) * 3);
		varray[0] = color[0] * LINK_RATE;
		varray[1] = color[1] * LINK_RATE;
		varray[2] = color[2] * LINK_RATE;
	}
	varray[4] = 1.0;
}
virtual const char* name() {
	return "By District";
}
};

class ColorNodeByPopulation : public NodeColoring {
public:
	virtual ~ColorNodeByPopulation(){}
virtual void colorNode(const Solver* sov, int nodeIndex, GLfloat* varray) {
	int pop = sov->gd->pop[nodeIndex];
	//varray[0] = varray[1] = varray[2] = (pop * 1.0) / sov->gd->maxpop;
	varray[0] = varray[1] = varray[2] = log(pop) / log(sov->gd->maxpop);
	varray[3] = 1.0;
}
virtual const char* name() {
	return "By Population";
}
};

NodeColoring* colorings[] = {
	new ColorNodeByDistrict(),
	new ColorNodeByPopulation(),
	new ColorNodeByERD(),
	new ColorNodeByOERD(),
	new ColorNodeByNeighRat(),
	new ColorNodeByPopRat(),
	new ColorNodeByLockStrength(),
	new ColorNodeByPopField(),
	NULL
};
NodeColoring* coloring = colorings[0];

extern "C" const char* getNodeColoringName(int index);
extern "C" void setNodeColoringByIndex(int index);

const char* getNodeColoringName(int index) {
	for (int i = 0; i <= index; ++i) {
		if (colorings[i] == NULL) {
			return NULL;
		}
	}
	return colorings[index]->name();
}
void setNodeColoringByIndex(int index) {
	for (int i = 0; i <= index; ++i) {
		if (colorings[i] == NULL) {
			return;
		}
	}
	coloring = colorings[index];
}

void Solver::drawGL() {
	GLint oldMM;
	if ( vertexBufferObject == 0 ) {
		initGL();
	}
	glGetIntegerv(GL_MATRIX_MODE,&oldMM);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	double wLatScale = cos( dcy / (1000000*180/M_PI) );
	double width = (maxx - minx);
	double height = (maxy - miny);
	double stateRatio = (width * wLatScale) / height;
	double xscale = 1.0;
	double yscale = 1.0;
	if ( viewportRatio > stateRatio ) {
		// viewport is wider, de-widen display
		xscale = viewportRatio / stateRatio;
	} else {
		// viewport is taller, de-tallen
		yscale = stateRatio / viewportRatio;
	}
	//printf("stateRatio %lf (%lf/%lf)\nxscale=%lf yscale=%lf\n", stateRatio, width, height, xscale, yscale );
	double halfWidth = xscale * width / (2.0 * zoom);
	double halfHeight = yscale * height / (2.0 * zoom);
	double glminx = dcx - halfWidth;
	double glmaxx = dcx + halfWidth;
	double glminy = dcy - halfHeight;
	double glmaxy = dcy + halfHeight;
	glOrtho( glminx, glmaxx, glminy, glmaxy, -1.0, 1.0 );
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	if ( showLinks ) {
#if 01
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, colorBufferObject );
		if ( lastGenDrawn != gencount ) {
			GLfloat* varray = NULL;
			glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
			glGetBufferPointervARB( GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&varray) );
			for ( int j = 0; j < gd->numPoints; j++ ) {
#if 1
				coloring->colorNode(this, j, varray + (j*4));
#else
				int da;
				const unsigned char* color;
				da = winner[j];
				// GL_C3F_V3F
				color = colors + ((da % numColors) * 3);
				varray[j*4 +  0] = color[0] * LINK_RATE;
				varray[j*4 +  1] = color[1] * LINK_RATE;
				varray[j*4 +  2] = color[2] * LINK_RATE;
				varray[j*4 +  4] = 1.0;
#endif
			}
			glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
			lastGenDrawn = gencount;
		}
		
#define BUFFER_OFFSET(i) ((char *)NULL + (i))
		glebail(glColorPointer( 4, GL_FLOAT, 0, BUFFER_OFFSET(0) ));
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBufferObject );
		glebail(glVertexPointer( 4, GL_FLOAT, 0, BUFFER_OFFSET(0) ));
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		glebail(glEnableClientState( GL_VERTEX_ARRAY ));
		glebail(glEnableClientState( GL_COLOR_ARRAY ));
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, linesBufferObject );
		glebail(glDrawElements( GL_LINES, numEdges * 2, GL_UNSIGNED_INT, 0 ));
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
		glebail(glDisableClientState( GL_VERTEX_ARRAY ));
		glebail(glDisableClientState( GL_COLOR_ARRAY ));
#elif 0
		drawLinksOld( numEdges, edgeData, gd, zoom, glminx, glmaxx, glminy, glmaxy, winner );
#elif 0
		drawLinksArrayA( numEdges, edgeData, gd, zoom, glminx, glmaxx, glminy, glmaxy, winner, (GLfloat**)(&_link_draw_array) );
#elif 01
		drawLinksArrayC( numEdges, edgeData, gd, zoom, glminx, glmaxx, glminy, glmaxy, winner, (GLfloat**)(&_link_draw_array) );
#else
		drawLinksArrayB( numEdges, edgeData, gd, zoom, glminx, glmaxx, glminy, glmaxy, winner, (GLfloat**)(&_link_draw_array) );
#endif
	} else {
#if 01
		/* array based point drawing */
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, colorBufferObject );
		if ( lastGenDrawn != gencount ) {
			GLfloat* varray = NULL;
			glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
			glGetBufferPointervARB( GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, (void**)(&varray) );
			for ( int j = 0; j < gd->numPoints; j++ ) {
#if 1
				coloring->colorNode(this, j, varray + (j*4));
#else
				int da;
				const unsigned char* color;
				da = winner[j];
				// GL_C3F_V3F
				if ( da == NODISTRICT ) {
					varray[j*4 +  0] = 0.2;
					varray[j*4 +  1] = 0.2;
					varray[j*4 +  2] = 0.2;
				} else {
					color = colors + ((da % numColors) * 3);
					varray[j*4 +  0] = color[0] * LINK_RATE;
					varray[j*4 +  1] = color[1] * LINK_RATE;
					varray[j*4 +  2] = color[2] * LINK_RATE;
				}
				varray[j*4 +  4] = 1.0;
#endif
			}
			glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
			lastGenDrawn = gencount;
		}
		
//#define BUFFER_OFFSET(i) ((char *)NULL + (i))
		glebail(glColorPointer( 4, GL_FLOAT, 0, BUFFER_OFFSET(0) ));
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBufferObject );
		glebail(glVertexPointer( 4, GL_FLOAT, 0, BUFFER_OFFSET(0) ));
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		glebail(glEnableClientState( GL_VERTEX_ARRAY ));
		glebail(glEnableClientState( GL_COLOR_ARRAY ));
		//glBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBufferObject ); // maybe this, if next doesn't work
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, vertexIndexObject );
		glebail(glDrawElements( GL_POINTS, gd->numPoints, GL_UNSIGNED_INT, 0 ));
		//glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
		glebail(glDisableClientState( GL_VERTEX_ARRAY ));
		glebail(glDisableClientState( GL_COLOR_ARRAY ));
#else
		/* ! array based point drawing */
	for ( POPTYPE d = 0; d < districts; d++ ) {
		District2* cd;
		const unsigned char* color;
		cd = dists + d;
		color = colors + ((d % numColors) * 3);
		glBegin(GL_POINTS);
		glColor3d( color[0] * NON_EDGE_RATE, color[1] * NON_EDGE_RATE, color[2] * NON_EDGE_RATE );
		for ( int i = 0; i < gd->numPoints; i++ ) {
			if ( winner[i] == d ) {
				double vx, vy;
				vx = gd->pos[i*2];
				vy = gd->pos[i*2+1];
				// do some of my own clipping
				if ( (zoom <1.9) || ((vx >= glminx) && (vx <= glmaxx) && (vy >= glminy) && (vy <= glmaxy)) ) {
					glVertex2d( vx, vy );
				}
			}
		}
		glEnd();
#if USE_EDGE_LOOP
		glBegin(GL_LINE_STRIP);
		glColor3d( color[0] / 255.0, color[1] / 255.0, color[2] / 255.0 );
		District::EdgeNode* en = cd->edgelistRoot;
		do {
			int pi;
			pi = en->nodeIndex;
			glVertex2d( gd->pos[pi*2], gd->pos[pi*2+1] );
			en = en->next;
		} while ( en != cd->edgelistRoot );
		glEnd();
#else
		glBegin(GL_POINTS);
		glColor3d( color[0] / 255.0, color[1] / 255.0, color[2] / 255.0 );
		for ( int i = 0; i < cd->edgelistLen; i++ ) {
			int pi;
			pi = cd->edgelist[i];
			double vx, vy;
			vx = gd->pos[pi*2];
			vy = gd->pos[pi*2+1];
			glVertex2d( vx, vy );
		}
		glEnd();
#endif
	}
	/* ! array based point drawing */
#endif
	}
	//glBegin(GL_LINES);
	glColor3d( 1.0, 1.0, 1.0 );
	//glEnd();
	double charheight = (glmaxy - glminy) / 20;
	for ( POPTYPE d = 0; d < districts; d++ ) {
		AbstractDistrict* cd;
		char buf[4];
		cd = &((*dists)[d]);
		glPushMatrix();
		glTranslated( 1.0 * cd->centerX(), 1.0 * cd->centerY(), 0.0 );
		glScaled( charheight,charheight,charheight);
		snprintf( buf, sizeof(buf), "%d", d + 1 );
		glutStrokeString( buf, 0 );
		glPopMatrix();
	}		
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(oldMM);
	recordDrawTime();
}

