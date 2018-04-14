#include "CountyCityDistricter.h"
#include "GeoData.h"
#include "Solver.h"
#include "placefile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <map>

using std::map;

class CCDistrict {
 public:
};

CountyCityDistricterSet::CountyCityDistricterSet(Solver* sovIn)
    : DistrictSet(sovIn) {
  if (sov->gd->place == NULL) {
    fprintf(stderr, "error cannot run CountyCity solver without loaded places (cities)\n");
    exit(1);
  }
  auto numPoints = sov->gd->numPoints;
  uint32_t minIndex = 0, maxIndex = 0;
  uint64_t* ubidLut = sov->gd->makeUbidLUT(&minIndex, &maxIndex);
#define countyOfIndex(i) ubidCounty(ubidLut[(i) - minIndex])
  countyBlockIndexes = new uint32_t[numPoints];
  cityBlockIndexes = new uint32_t[numPoints];
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

  fprintf(stderr, "CC %zd counties %zd cities\n", countyIdBlockCount.size(), cityIdBlockCount.size());
  numCounties = countyIdBlockCount.size();
  counties = new CountyCity[numCounties];
  int ci = 0;
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

  numCities = cityIdBlockCount.size();
  cities = new CountyCity[numCities];
  ci = 0;
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
    cityIdBlockCount[place] += 1;
  }

  delete [] ubidLut;

  lock = new uint8_t[numPoints];
  solution = new POPTYPE[numPoints];
  for (int i = 0; i < numPoints; i++) {
    lock[i] = 0;
    solution[i] = NODISTRICT;
  }
  //memset(solution, NODISTRICT, sizeof(POPTYPE[numPoints]));
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
void CountyCityDistricterSet::initNewRandomStart() {
  int dpop = popTarget();
  POPTYPE di = 0;
  int minCountyDistrictPop = dpop * (1.0 - countyCloseEnoughPopulationFraction);
  int maxCountyDistrictPop = dpop * (1.0 + countyCloseEnoughPopulationFraction);
  for (int ci = 0; ci < numCounties; ci++) {
    if (counties[ci].pop > minCountyDistrictPop && counties[ci].pop < maxCountyDistrictPop) {
      assert(di < districts);
      fprintf(stderr, "set district %d to county %d\n", di, counties[ci].id);
      counties[ci].districtLock = di;
      for (int bi = 0; bi < counties[ci].numBlocks; bi++) {
        auto b = counties[ci].blockIndexes[bi];
        lock[b] = COUNTY_LOCK;
        solution[b] = di;
      }
      di++;
    }
  }
  // start remaining districts seeded with some core random city
  while (di < districts) {
    int runawayLimit = 1000;
    while (runawayLimit > 0) {
      auto xc = random() % numCities;
      if (cities[xc].districtLock != NODISTRICT) {
        runawayLimit--;
        continue;
      }
      bool cityOk = true;
      for (int bi = 0; bi < cities[xc].numBlocks; bi++) {
        if (solution[cities[xc].blockIndexes[bi]] != NODISTRICT) {
          cityOk = false;
          break;
        }
      }
      if (!cityOk) {
        runawayLimit--; continue;
      }
      cities[xc].districtLock = di;
      for (int bi = 0; bi < cities[xc].numBlocks; bi++) {
        auto b = cities[xc].blockIndexes[bi];
        lock[b] = CITY_LOCK;
        solution[b] = di;
      }
      break;
    }
    if (runawayLimit <= 0) {
      fprintf(stderr, "could not find a city for district %d\n", di);
      exit(1);
    }
    di++;
  }
}
void CountyCityDistricterSet::initFromLoadedSolution() {
  assert(false); // TODO: WRITEME
}
// return err { 0: ok, else err }
int CountyCityDistricterSet::step() {
  int numPoints = sov->gd->numPoints;
  assert(false); // TODO: WRITEME


  // decay locks
  for (int i = 0; i < numPoints; i++) {
    uint8_t l = lock[i];
    if ((l != 0) && ((l & 0x80) == 0)) {
      lock[i] = l - 1;
    }
  }
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

void CountyCityDistricterSet::fixupDistrictContiguity() {
  assert(false); // TODO: WRITEME
}

int CountyCityDistricterSet::numParameters() {
  assert(false); // TODO: WRITEME
}
// Return NULL if index too high or too low.
const char* CountyCityDistricterSet::getParameterLabelByIndex(int index) {
  assert(false); // TODO: WRITEME
  return NULL;
}
double CountyCityDistricterSet::getParameterByIndex(int index) {
  assert(false); // TODO: WRITEME
  return 0.0;
}
void CountyCityDistricterSet::setParameterByIndex(int index, double value) {
  assert(false); // TODO: WRITEME
}
	
// how long should this run?
int CountyCityDistricterSet::defaultGenerations() {
  assert(false); // TODO: WRITEME
  return 0;
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
