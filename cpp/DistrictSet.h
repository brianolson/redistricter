#ifndef DISTRICT_SET_H
#define DISTRICT_SET_H

class AbstractDistrict;
class Solver;
class SolverStats;

// Should be subclassed by a tightly bound district implementation
class DistrictSet {
public:
DistrictSet(Solver* sovIn) : districts(-1), they((AbstractDistrict**)0), sov(sovIn) {}
  virtual ~DistrictSet();

  // Allocate space for some number of per-district structs
  virtual void alloc(int numDistricts) = 0;

  virtual void initNewRandomStart() = 0;
  virtual void initFromLoadedSolution() = 0;

  // Iterate solver algorithm. Return 0==Ok; else error
  virtual int step() = 0;

  virtual char* debugText() = 0;

  virtual void getStats(SolverStats* stats) = 0;
  virtual void print(const char* filename);

  AbstractDistrict& operator[](int index) {
    return *(they[index]);
  }

  // *Parameter* functions here are part of a general purpose
  // configurability feature that was part of a MacOS native GUI that
  // has bitrotted away. Maybe bring it back some day as part of
  // command line or automated solution space exploration.
  virtual int numParameters() = 0;
  // Return NULL if index too high or too low.
  virtual const char* getParameterLabelByIndex(int index) = 0;
  virtual double getParameterByIndex(int index) = 0;
  virtual void setParameterByIndex(int index, double value) = 0;
	
  // how long should this run?
  virtual int defaultGenerations() = 0;

  // How many are they?
  int districts;
	
  // Probably pointers to a backing array of T[districts]
  // for some subclass T of AbstractDistrict
  AbstractDistrict** they;

  // Every DistrictSet is associated with some Solver
  Solver* sov;

  // not inline because it digs into Solver and GeoData
  int popTarget() const;
};

#endif /* DISTRICT_SET_H */
