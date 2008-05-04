#ifndef SOLVER_H
#define SOLVER_H

#include "District2.h"

#include <stdio.h>
#include "LinearInterpolate.h"

class Adjacency;
class DistrictSet;
class Node;
class GrabIntermediateStorage;

// TODO separate application state and solution state!
class Solver {
public:
	int districts;
	
	double totalpop;
	double districtPopTarget;
	
	Node* nodes;
	int* allneigh;
	POPTYPE* winner;
	DistrictSet* (*districtSetFactory)(Solver*);
	DistrictSet* dists;
	
	char* inputname;
	int generations;
	char* dumpname;
	char* loadname;
	enum initModeEnum {
		initWithNewDistricts = 1,
		initWithOldDistricts
	} initMode;
	
	char* solutionLogPrefix;
	int solutionLogInterval;
	int solutionLogCountdown;
	
#if WITH_PNG
	char* pngLogPrefix;
	int pngLogInterval;
	int pngLogCountdown;
#endif
	
	FILE* statLog;
	int statLogInterval;
	int statLogCountdown;
	
	char* distfname;
	char* coordfname;
	
#if WITH_PNG
	char* pngname;
	int pngWidth, pngHeight;
#endif
	
	GeoData* gd;
	GeoData* (*geoFact)(char*);
	
	uint32_t numEdges;
	int32_t* edgeData;
	
	//POPTYPE* sorti;
	
#if READ_INT_POS
	int minx, miny, maxx, maxy;
#define POS_TYPE int
#elif READ_DOUBLE_POS
	double minx, miny, maxx, maxy;
#define POS_TYPE double
#endif
	double viewportRatio;
	
	int gencount;

	FILE* blaf;

	Solver();
	~Solver();
	
	int handleArgs( int argc, char** argv );
	void load();
	void readLinksFile();
	void readLinksBin();
	/** Binary Format:
	[GeoData.h binary format for GeoData::writeBin(int,char*)
	uint32_t numEdges;
	int32_t edges[numEdges*2]; // pairs of indecies into GeoData
	*/
	int writeBin( const char* fname );
	void initNodes();
	void allocSolution();
	int saveZSolution( const char* filename );
	int loadZSolution( const char* filename );
	/* from loadname */
	int loadSolution( const char* loadname );
	void initSolution();
	void initSolutionFromOldCDs();
	void init();
	int megaInit();
	int step();
#if WITH_PNG
	void doPNG();
	void doPNG(POPTYPE* soln, const char* outname);
	void doPNG_r( unsigned char* data, unsigned char** rows, int pngWidth, int pngHeight, const char* pngname );
#endif
	void printDistricts(const char* filename);
	void printDistricts() {
		printDistricts(distfname);
	}
	/* to dumpname */
	int saveSolution();
	
	/* draw stuff */
	double dcx, dcy;
	double zoom;
	int showLinks;

	POPTYPE* renumber;

	void calculateAdjacency(Adjacency*);
	static void calculateAdjacency_r(Adjacency* it, int numPoints, int districts,
									 const POPTYPE* winner, const Node* nodes);
	void californiaRenumber();
	void nullRenumber();

	LinearInterpolate popRatioFactor;

	void* _point_draw_array;
	//void* _link_draw_array;
	int lastGenDrawn;
	unsigned long vertexBufferObject, colorBufferObject, linesBufferObject, vertexIndexObject;
#ifndef DRAW_HISTORY_LEN
#define DRAW_HISTORY_LEN 10
#endif
	int64_t usecDrawTimeHistory[DRAW_HISTORY_LEN];
	double fps;
	int drawHistoryPos;
	void recordDrawTime();
	
	void initGL();
	void drawGL();
	void nudgeLeft();
	void nudgeRight();
	void nudgeUp();
	void nudgeDown();
	void zoomIn();
	void zoomOut();
	void zoomAll();
	void centerOnPoint( int index );
	int main( int argc, char** argv );
		
	inline int setDist( POPTYPE d, int i ) {
		return (*dists)[(d)].add( this, (i), (d) );
	}
	
	int debugDistrictNumber;

	double maxSpreadFraction;
	double maxSpreadAbsolute;

	SolverStats* getDistrictStats();
	int getDistrictStats( char* str, int len );
};

class SolverStats {
public:
	int generation;
	double avgPopDistToCenterOfDistKm;
	double poptotal;
	double popavg;
	double popstd;
	double popmin;
	double popmax;
	double popmed;
	int mindist;
	int maxdist;
	int meddist;
	int nod;
	double nodpop;
	SolverStats* next;

	SolverStats();
	SolverStats( int geni, double pd, double pa, double ps, double pmi, double pma, double pme,
		int mid, int mad, int med, int noDist, double noDistPop, SolverStats* n = NULL );

	int toString( char* str, int len );
};

// describe which districts border which other districts for coloring purposes
class Adjacency {
public:
	// pairs of district indecies which border each other.
	POPTYPE* adjacency;
	// number of pairs in list
	int adjlen;
	// size of allocation in nuber of pairs
	int adjcap;
	
	Adjacency();
	~Adjacency();
};

DistrictSet* NearestNeighborDistrictSetFactory(Solver* sov);
DistrictSet* District2SetFactory(Solver* sov);

#endif /* SOLVER_H */
