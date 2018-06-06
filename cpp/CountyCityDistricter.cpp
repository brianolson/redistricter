/*
 * Automatic disticting that tries to produce compact solutions while
 * keeping cities and counties whole as much as possible.
 *
 * Should be capable of producing plans compatible with 2015 Ohio law
 * about drawing state legislature districts.
 *
 * 1. allocate counties within 5% of ideal district population and lock them in (required by OH)
 * 2a. allocate cities within 5% of ideal district population and lock them in (not required by OH, but makes sense to me)
 * 2b. allocate cities with less than district population as 'super blocks', do block-allocation style optimization at this level for a few ticks
 * 3. allocate non-city blocks and optimize for equal population and compactness, run a bunch of ticks
 * 4. (if equal-population goal not met) break some cities and counties and continue to optimizie for equal population and compactness.
 */
#include "CountyCityDistricter.h"
#include "GeoData.h"
#include "Solver.h"
#include "placefile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <map>
#include <memory>

using std::map;
using std::unique_ptr;

CountyCityDistricterSet::CountyCityDistricterSet(Solver* sovIn)
    : DistrictSet(sovIn) {
  if (sov->gd->place == NULL) {
    fprintf(stderr, "error cannot run CountyCity solver without loaded places (cities)\n");
    exit(1);
  }
  auto numPoints = sov->gd->numPoints;
  uint32_t minIndex = 0, maxIndex = 0;
  unique_ptr<uint64_t[]> ubidLut(sov->gd->makeUbidLUT(&minIndex, &maxIndex));
#define countyOfIndex(i) ubidCounty(ubidLut[(i) - minIndex])

  // Gather the Counties (from county id in primary data) and Cities (from 'place' data) and count the number of blocks in each.
  map<uint32_t, uint32_t> cityIdBlockCount;
  map<uint32_t, uint32_t> countyIdBlockCount;
  for(int i = 0; i < numPoints; i++) {
    countyIdBlockCount[countyOfIndex(i)] += 1;
    uint32_t place = sov->gd->place[i];
    if (place == PlaceMap::INVALID_PLACE || place == 0) {
      continue;
    }
    cityIdBlockCount[place] += 1;
  }
  numCounties = countyIdBlockCount.size();
  numCities = cityIdBlockCount.size();
  fprintf(stderr, "CC %u counties %u cities\n", numCounties, numCities);

  // Allocate county block list, set pointer in each county
  counties = new CountyCity[numCounties];
  int ci = 0;
  countyBlockIndexes = new uint32_t[numPoints];
  uint32_t* cbip = countyBlockIndexes;
  for (map<uint32_t, uint32_t>::const_iterator it = countyIdBlockCount.begin(); it != countyIdBlockCount.end(); ++it) {
    uint32_t countyId = it->first;
    uint32_t blockCount = it->second;
    countyIdToIndex[countyId] = ci;
    counties[ci].id = countyId;
    counties[ci].blockIndexes = cbip;
    cbip += blockCount;
    ci++;
  }

  // Allocate city block list, set pointer in each city
  cities = new CountyCity[numCities];
  ci = 0;
  cityBlockIndexes = new uint32_t[numPoints];
  cbip = cityBlockIndexes;
  for (map<uint32_t, uint32_t>::const_iterator it = cityIdBlockCount.begin(); it != cityIdBlockCount.end(); ++it) {
    uint32_t id = it->first;
    uint32_t blockCount = it->second;
    cityIdToIndex[id] = ci;
    counties[ci].id = id;
    counties[ci].blockIndexes = cbip;
    cbip += blockCount;
    ci++;
  }

  // for each block, append it to appropriate county and city block list
  for(int i = 0; i < numPoints; i++) {
    uint32_t county = countyOfIndex(i);
    int ci = countyIdToIndex[county];
    counties[ci].blockIndexes[counties[ci].numBlocks] = i;
    counties[ci].numBlocks++;
    counties[ci].pop += sov->gd->pop[i];

    uint32_t place = sov->gd->place[i];
    if (place == PlaceMap::INVALID_PLACE || place == 0) {
      continue;
    }
    ci = cityIdToIndex[place];
    CountyCity* city = &(cities[ci]);
    city->blockIndexes[city->numBlocks] = i;
    city->numBlocks++;
    city->pop += sov->gd->pop[i];
  }

  lock = new uint8_t[numPoints];
  memset(lock, 0, sizeof(uint8_t[numPoints]));
  solution = new POPTYPE[numPoints];
  memset(solution, NODISTRICT, sizeof(POPTYPE[numPoints]));
}

CountyCityDistricterSet::~CountyCityDistricterSet() {
  delete [] lock;
  delete [] solution;
  delete [] counties;
  delete [] cities;
  delete [] countyBlockIndexes;
  delete [] cityBlockIndexes;
  delete [] they;
}

// TODO: make countyCloseEnoughPopulationFraction configurable
// 0.05 is based on the Ohio 2015 law about state legislature redistricting.
double CountyCityDistricterSet::countyCloseEnoughPopulationFraction = 0.05;
static const uint8_t COUNTY_LOCK = 0x8f;
static const uint8_t CITY_LOCK = 0x8e;

void CountyCityDistricterSet::alloc(int numDistricts) {
  DistrictSet::alloc(numDistricts);
  ccdstorage = new CountyCityDistrict[numDistricts];
  for (int i = 0; i < numDistricts; i++) {
    they[i] = ccdstorage + i;
  }
}

// search counties for whole counties close enough to target population
POPTYPE CountyCityDistricterSet::grabWholeCounties(POPTYPE di) {
  int dpop = popTarget();
  unsigned int minCountyDistrictPop = dpop * (1.0 - countyCloseEnoughPopulationFraction);
  unsigned int maxCountyDistrictPop = dpop * (1.0 + countyCloseEnoughPopulationFraction);
  for (unsigned int ci = 0; ci < numCounties; ci++) {
    if (counties[ci].pop > minCountyDistrictPop && counties[ci].pop < maxCountyDistrictPop) {
      assert(di < districts);
      fprintf(stderr, "set district %d to county %d\n", di, counties[ci].id);
      counties[ci].districtLock = di;
      for (unsigned int bi = 0; bi < counties[ci].numBlocks; bi++) {
        auto b = counties[ci].blockIndexes[bi];
        lock[b] = COUNTY_LOCK;
        solution[b] = di;
      }
      di++;
    }
  }
  return di;
}

POPTYPE CountyCityDistricterSet::grabWholeCities(POPTYPE di) {
  // search cities which match district population and lock-allocate them
  // TODO: make this so it can be disabled, or randomized so it only happens to some of the eligible cities.
  int dpop = popTarget();
  unsigned int minCountyDistrictPop = dpop * (1.0 - countyCloseEnoughPopulationFraction);
  unsigned int maxCountyDistrictPop = dpop * (1.0 + countyCloseEnoughPopulationFraction);
  for (unsigned int ci = 0; ci < numCities; ci++) {
    if (cities[ci].pop > minCountyDistrictPop && cities[ci].pop < maxCountyDistrictPop) {
      assert(di < districts);
      fprintf(stderr, "set district %d to county %d\n", di, cities[ci].id);
      cities[ci].districtLock = di;
      for (unsigned int bi = 0; bi < cities[ci].numBlocks; bi++) {
        auto b = cities[ci].blockIndexes[bi];
        if (lock[b] != 0) {
          // county must have gotten here first, unwind this city lock
          for (; bi >= 0; bi--) {
            b = cities[ci].blockIndexes[bi];
            if (lock[b] == CITY_LOCK) {
              lock[b] = 0;
              solution[b] = NODISTRICT;
            }
          }
          cities[ci].districtLock = NODISTRICT;
          goto nextcity; // break+continue
        }
        lock[b] = CITY_LOCK;
        solution[b] = di;
      }
      di++;
    }
 nextcity:
    ;
  }
  return di;
}

void CountyCityDistricterSet::initNewRandomStart() {
  // next district id to assign to
  POPTYPE di = 0;

  di = grabWholeCounties(di);

  di = grabWholeCities(di);

  // start remaining districts seeded with some core random city
  while (di < districts) {
    int runawayLimit = 1000;
 try_a_city:
    while (runawayLimit > 0) {
      auto xc = random() % numCities;
      if (cities[xc].districtLock != NODISTRICT) {
        // random city already taken, try again
        runawayLimit--;
        continue;
      }
      for (unsigned int bi = 0; bi < cities[xc].numBlocks; bi++) {
        if (solution[cities[xc].blockIndexes[bi]] != NODISTRICT) {
          // random city partially taken, try again
          runawayLimit--;
          goto try_a_city;
        }
      }
      cities[xc].districtLock = di;
      for (unsigned int bi = 0; bi < cities[xc].numBlocks; bi++) {
        auto b = cities[xc].blockIndexes[bi];
        lock[b] = CITY_LOCK;
        solution[b] = di;
      }
      goto initialized_city;
    }
    if (runawayLimit <= 0) {
      fprintf(stderr, "could not find a city for district %d\n", di);
      exit(1);
    }
 initialized_city:
    di++;
  }
}
void CountyCityDistricterSet::initFromLoadedSolution() {
  assert(false); // TODO: WRITEME
  // This _might_ just be loading the block assignment and leaving locks blank.
  // Maybe it should detect whole county/city assignment and set some kind of lock on those?
}

// return err { 0: ok, else err }
int CountyCityDistricterSet::step() {
  int numPoints = sov->gd->numPoints;
  int dpop = popTarget();

  // decay locks
  for (int i = 0; i < numPoints; i++) {
    uint8_t l = lock[i];
    // lock = 0 is done and unlocked, don't decrement and wraparound.
    // lock & 0x80 is a non-decaying lock set at startup on a city or county.
    if ((l != 0) && ((l & 0x80) == 0)) {
      lock[i] = l - 1;
    }
  }

  // Strategy? Counties were allocated (if any) in setup. Allocate the
  // cities as whole blocksto get near some solution, then start
  // splitting cities and allocating their blocks and unincorporated
  // blocks like Districter2 compactness grabbing.
  assert(false); // TODO: WRITEME
  return 0;
}

char* CountyCityDistricterSet::debugText() {
  assert(false); // TODO: WRITEME
  return NULL;
}
	
void CountyCityDistricterSet::getStats(SolverStats* stats) {
  assert(false); // TODO: WRITEME
}
void CountyCityDistricterSet::print(const char* filename) {
  assert(false); // TODO: WRITEME
}

// *Parameter* functions part of obsolete API; don't rush to implement.
int CountyCityDistricterSet::numParameters() {
  return 0; // TODO: WRITEME
}
// Return NULL if index too high or too low.
const char* CountyCityDistricterSet::getParameterLabelByIndex(int index) {
  return NULL; // TODO: WRITEME
}
double CountyCityDistricterSet::getParameterByIndex(int index) {
  return 0.0; // TODO: WRITEME
}
void CountyCityDistricterSet::setParameterByIndex(int index, double value) {
  assert(false); // TODO: WRITEME
}
	
// how long should this run?
int CountyCityDistricterSet::defaultGenerations() {
  // Just a guess copied from District2
  return sov->gd->numPoints * 10 / districts;
}


CountyCityDistrict::CountyCityDistrict()
    : AbstractDistrict() {
}
CountyCityDistrict::~CountyCityDistrict() {
}
int CountyCityDistrict::add( Solver* sov, int n, POPTYPE dist ) {
  // TODO: unused?
  assert(false); // TODO: WRITEME
  return 0;
}
int CountyCityDistrict::remove( Solver* sov, int n, POPTYPE dist, double x, double y, double npop ) {
  // TODO: unused?
  assert(false); // TODO: WRITEME
  return 0;
}
double CountyCityDistrict::centerX() {
  assert(false); // TODO: WRITEME
  return 0.0;
}
double CountyCityDistrict::centerY() {
  assert(false); // TODO: WRITEME
  return 0.0;
}
