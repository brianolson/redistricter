#ifndef COUNTY_CITY_DISTRICTER_H
#define COUNTY_CITY_DISTRICTER_H

#include "AbstractDistrict.h"
#include "DistrictSet.h"
#include <map>

class Solver;
class CountyCityDistrict;

/** A city or a county */
class CountyCity {
public:
  uint32_t id; // 3 digit county or 5 digit place id
  uint32_t numBlocks;
  uint32_t* blockIndexes; // a pointer into a large block allocated elsewhere
  uint32_t pop;
  POPTYPE districtLock;

CountyCity()
    :id(0), numBlocks(0), blockIndexes(NULL), pop(0), districtLock(NODISTRICT)
  {}
};

class CountyCityDistricterSet : public DistrictSet {
public:
  CountyCityDistricterSet(Solver* sovIn);
  virtual ~CountyCityDistricterSet();

  virtual void alloc(int size);
  virtual void initNewRandomStart();
  virtual void initFromLoadedSolution();
  // return err { 0: ok, else err }
  virtual int step();

  virtual char* debugText();
	
  virtual void getStats(SolverStats* stats);
  virtual void print(const char* filename);

  virtual void fixupDistrictContiguity();

  virtual int numParameters();
  // Return NULL if index too high or too low.
  virtual const char* getParameterLabelByIndex(int index);
  virtual double getParameterByIndex(int index);
  virtual void setParameterByIndex(int index, double value);
	
  // how long should this run?
  virtual int defaultGenerations();

  // If county population is within this range of ideal, call it
  // good enough and lock off that county as a district. e.g. 0.05
  // on an ideal pop of 100,000 allows 95,000-105,000.
  static double countyCloseEnoughPopulationFraction;

private:
  std::map<uint32_t, uint32_t> countyIdToIndex;
  CountyCity* counties;
  unsigned int numCounties;
  uint32_t* countyBlockIndexes; // holds allocation of uint_32[numPoints]
  std::map<uint32_t, uint32_t> cityIdToIndex;
  CountyCity* cities;
  unsigned int numCities;
  uint32_t* cityBlockIndexes; // holds allocation of uint_32[numPoints]

  // uint8_t[numPoints], 0b1xxx_xxx hard lock or 0b0xxx_xxxx decaying thrash lock
  uint8_t* lock;
  // POPTYPE[numPoints]
  POPTYPE* solution;

  CountyCityDistrict* ccdstorage;
};

class CountyCityDistrict : public AbstractDistrict {
public:
  CountyCityDistrict();
  virtual ~CountyCityDistrict();
  virtual int add( Solver* sov, int n, POPTYPE dist );
  virtual int remove( Solver* sov, int n, POPTYPE dist, double x, double y, double npop );
  virtual double centerX();
  virtual double centerY();

  uint32_t pop;
  double popXSum;
  double popYSum;
};

#endif /* COUNTY_CITY_DISTRICTER_H */
