#ifndef DISTRICT2_H
#define DISTRICT2_H

#include "AbstractDistrict.h"
#include "DistrictSet.h"

class Bitmap;
class District2Set;
class GrabTaskParams;
class Solver;
class Node;

class District2 : public AbstractDistrict {
public:
	int numNodes;
	// edgelist : node indecies
	int* edgelist;
	int edgelistLen, edgelistCap;
	double edgeMeanR;
	
	double pop;
	uint64_t area;
	double landCenterX, landCenterY;
	double popCenterX, popCenterY;
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
	virtual double centerX();
	virtual double centerY();
	
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
	
	static double edgeRelativeDistanceFactor;
	static double odEdgeRelativeDistanceFactor;
	static double neighborRatioFactor;
	static double popRatioFactor;
	static double lockStrengthFactor;
	static double randomFactor;
	static double popFieldFactor;
	
	static const char* parameterNames[];

	static double lastAvgPopField;
	static double nextPopFieldSum;
	static int numPopFieldsSummed;

	static void step();

#if 1
	inline void validate(const char* file = ((const char*)0), int line = -1) {}
#define NO_D2_VALIDATE 1
#else
	void validate(const char* file = ((const char*)0), int line = -1);
#endif
 protected:
	inline double cx() { return landCenterX / area; }
	inline double cy() { return landCenterY / area; }

	friend class District2Set;
};

class GrabIntermediateStorage;
class StatThing;

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

	virtual void fixupDistrictContiguity();
	void assignReposessedNodes(int* bfsearchq, int pointsRepod);

	virtual int numParameters();
	// Return NULL if index too high or too low.
	virtual const char* getParameterLabelByIndex(int index);
	virtual double getParameterByIndex(int index);
	virtual void setParameterByIndex(int index, double value);

	virtual int defaultGenerations();

	void recalc();

	POPTYPE* sorti;
	unsigned char* lock;
	GrabIntermediateStorage* grabdebug;
	StatThing* debugStats;
	// Returns malloc() allocated memory that caller must free(), or NULL.
	char* debugStatsText();
	
	double fixupFrequency;
	double fixupBucket;
	
	double currentRandomFactor;

	Bitmap* notContiguous;
};

#endif
