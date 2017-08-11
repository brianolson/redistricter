#ifndef SOLVER_H
#define SOLVER_H

#include <stdio.h>
#include "LinearInterpolate.h"
#include "DistrictSet.h"
#include "AbstractDistrict.h"

class Adjacency;
class GeoData;
class GrabIntermediateStorage;
class Node;
template<class T> class LastNMinMax;
class BinaryStatLogger;
class SolverStats;

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
	DistrictSet* _dists;
	DistrictSet* getDistricts(); // lazy allocating caching accessor
	
	const char* inputname;
	int generations;
	char* dumpname;
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
	BinaryStatLogger* pbStatLog;
	
	char* distfname;
	char* coordfname;
	
#if WITH_PNG
	char* pngname;
	int pngWidth, pngHeight;
#endif
	
	GeoData* gd;
	GeoData* (*geoFact)(const char*);
	
	uint32_t numEdges;
	int32_t* edgeData;
	
	//POPTYPE* sorti;
	
#define POS_TYPE int
	POS_TYPE minx, miny, maxx, maxy;
	double viewportRatio;
	
	int gencount;

	FILE* blaf;
	
	int runDutySeconds;
	int sleepDutySeconds;
	
	LastNMinMax<double>* recentKmpp;
	LastNMinMax<double>* recentSpread;
	int giveupSteps;
	double recentKmppGiveupFraction;
	double recentSpreadGiveupFraction;
	bool nonProgressGiveup() const;

	Solver();
	~Solver();
	
	int handleArgs( int argc, const char** argv );
	void load();
	// If filename is null, add ".links" to inputname
	void readLinksFile(const char* filename);
	bool readLinksFileData(const char* data, size_t len);
	void readLinksBin();
	/** Binary Format:
	[GeoData.h binary format for GeoData::writeBin(int,char*)
	uint32_t numEdges;
	int32_t edges[numEdges*2]; // pairs of indecies into GeoData
	*/
	int writeBin( const char* fname );
	int writeProtobuf( const char* fname );
	void initNodes();
	void allocSolution();
	int saveZSolution( const char* filename );
	int loadSolution();
	bool hasSolutionToLoad();  // did handleArgs get something?
	const char* getSolutionFilename() const;
	// immediate solution load .. probably for drend in animation mode
	int loadZSolution( const char* filename );
	int loadCsvSolution( const char* filename );
	void initSolution();
	void initSolutionFromOldCDs();
	void init();
	static const char* getSetFactoryName(int index);
	void setFactoryByIndex(int index);
	int megaInit();
	int step();
#if WITH_PNG
	void doPNG();
	void doPNG(POPTYPE* soln, const char* outname);
	//void doPNG_r( unsigned char* data, unsigned char** rows, int pngWidth, int pngHeight, const char* pngname );
#endif /* WITH_PNG */

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
	int main( int argc, const char** argv );

	int debugDistrictNumber;

	double maxSpreadFraction;
	double maxSpreadAbsolute;
	
	SolverStats* getDistrictStats();
	int getDistrictStats( char* str, int len );
	
	static const char* argHelp;

    private:
	enum {
	    DszFormat = 1,
	    CsvFormat = 2,
	    DetectFormat = 3,
	} loadFormat;
	char* loadname;

	// callbacks for arghanlding.h StringArgWithCallback
	void setCsvLoadname(void* context, const char* filename);
	void setDszLoadname(void* context, const char* filename);
	void setLoadname(void* context, const char* filename);
};

class SolverStats {
public:
	int generation;
	double avgPopDistToCenterOfDistKm;
	double kmppp; // km per person to population center of their district
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

int parseArgvFromFile(const char* filename, char*** argvP);

#endif /* SOLVER_H */
