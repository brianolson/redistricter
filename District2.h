#ifndef DISTRICT2_H
#define DISTRICT2_H

#include "districter.h"
//#include "flowdistricter.h"

class Solver;
class Node;
class GrabTaskParams;

#include "AbstractDistrict.h"
#include "DistrictSet.h"

class District2Set;

class District2 : public AbstractDistrict {
public:
	int numNodes;
	// edgelist : node indecies
	int* edgelist;
	int edgelistLen, edgelistCap;
	double edgeMeanR;
	
	double pop;
	double area;
	// weighted x and y, center is (x/pop,y/pop)
	double wx, wy;
	// x/pop, y/pop
	double distx, disty;
	double moment;
	
	District2();
	
	//void add( Node* nodes, POPTYPE* pit, int n, POPTYPE dist, double x, double y, double npop );
	/*!
		@param Solver* sov context
	 @param int n node index
	 @param POPTYPE dist this District's index
	 */
	virtual int add( Solver* sov, int n, POPTYPE dist );
	void addFirst( Solver* sov, int n, POPTYPE dist );
	virtual int remove( Solver* sov, int n, POPTYPE dist, double x, double y, double npop );
	
	void refresh( Node* nodes, POPTYPE* pit, double* xy, double* popRadii, POPTYPE dist );
	void calcMoment( Node* nodes, POPTYPE* pit, double* xy, double* popRadii, POPTYPE dist );

#ifndef GRAB_SCORE_DEBUG
#define GRAB_SCORE_DEBUG 0
#endif
#if GRAB_SCORE_DEBUG
	double grabScore( District2Set* sov, POPTYPE d, int nein, int gsdebug );
	inline double grabScore( District2Set* sov, POPTYPE d, int nein ) {
		return grabScore( sov, d, nein, 0 );
	}
	inline double grabScoreDebug( District2Set* sov, POPTYPE d, int nein ) {
		return grabScore( sov, d, nein, 1 );
	}
#else
	double grabScore( District2Set* sov, POPTYPE d, int nein );
	inline double grabScoreDebug( District2Set* sov, POPTYPE d, int nein ) {
		return grabScore( sov, d, nein );
	}
#endif
	int grab( District2Set* sov, POPTYPE d );
	int disown( Solver* sov, POPTYPE d );
	
	/* for multithreading within grab */
	void grabTask( GrabTaskParams* p );
	
	int write( int fd );
	int read( int fd, Node* nodes, POPTYPE* pit, double* xy, double* popRadii, POPTYPE dist );
	
	void addEdge( int n );
	void removeEdge( int n );
	
	void recalc( Solver* sov, POPTYPE dist );
	
	static double edgeRelativeDistanceFactor;
	static double odEdgeRelativeDistanceFactor;
	static double neighborRatioFactor;
	static double popRatioFactor;
	static double lockStrengthFactor;
	static double randomFactor;
	static double popFieldFactor;

	static double lastAvgPopField;
	static double nextPopFieldSum;
	static int numPopFieldsSummed;

	static void step();

	inline void _addFirstToCenter( double x, double y, uint32_t tarea ) {
	    if ( tarea == 0 ) {
		distx = wx = x;
		disty = wy = y;
	    } else {
		wx = tarea * x;
		wy = tarea * y;
		distx = wx / area;
		disty = wy / area;
	    }
	}
	inline void _addFirstToCenter( double x, double y ) {
	    distx = wx = x;
	    disty = wy = y;
	}
	inline void _addFirstToCenter( double x, double y, double npop ) {
	    if ( npop == 0.0 ) {
		//printf("adding block %d with zero pop", n );
		distx = wx = x;
		disty = wy = y;
	    } else {
		wx = npop * x;
		wy = npop * y;
		distx = wx / pop;
		disty = wy / pop;
	    }
	}

	/* AbstractDistrict implementation */
	virtual double centerX();
	virtual double centerY();
};

class GrabIntermediateStorage;

class District2Set : public DistrictSet {
public:
	District2Set(Solver* sovIn);
	District2* dists;
	virtual void alloc(int size);
	virtual void initNewRandomStart();
	virtual void initFromLoadedSolution();
	virtual int step();

	virtual char* debugText();

	virtual void getStats(SolverStats* stats);

	POPTYPE* sorti;
	unsigned char* lock;
	GrabIntermediateStorage* grabdebug;
};

#endif
