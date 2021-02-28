#!/usr/bin/env python

# standard
import csv
import gzip
import logging
import optparse
import os
import re
import struct
import subprocess
import sys
import time
import urllib.request, urllib.parse, urllib.error
import zipfile

# local
import generaterunconfigs
import linksfromedges
from newerthan import newerthan
import setupstatedata
import shapefile
import states
from makePlaceBlockList import getTopNPlaceCodes, filterPlacesToUbidList

logger = logging.getLogger(__name__)

COUNTY_URL = 'https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/COUNTY/2020/'
# https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/COUNTY/2020/tl_2020_05_county20.zip
COUNTY_RE = re.compile(r'tl_2020_(\d\d)_county20.zip', re.IGNORECASE)

EDGES_URL = 'https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/EDGES/'
# https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/EDGES/tl_2020_05001_edges.zip
EDGES_RE = re.compile(r'tl_2020_(\d\d)(\d\d\d)_edges.zip', re.IGNORECASE)

FACES_URL = 'https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/FACES/'
# https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/FACES/tl_2020_05001_faces.zip
FACES_RE = re.compile(r'tl_2020_(\d\d)(\d\d\d)_faces.zip', re.IGNORECASE)

TABBLOCK_URL = 'https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/TABBLOCK/2020/'
# https://www2.census.gov/geo/tiger/TIGER2020PL/LAYER/TABBLOCK/2020/tl_2020_05_tabblock20.zip
TABBLOCK_RE = re.compile(r'tl_2020_(\d\d)_tabblock20.zip', re.IGNORECASE)

# Three days
INDEX_CACHE_SECONDS = 3*24*3600

def cachedIndexNeedsFetch(path):
  try:
    st = os.stat(path)
    if (time.time() - st.st_mtime) < INDEX_CACHE_SECONDS:
      # exists and is new enough
      return False
  except OSError as e:
    # Doesn't exist, ok, we'll fetch it.
    pass
  return True


class CensusTigerBundle(object):
  def __init__(self, path, state_fips, county):
    self.path = path
    self.state_fips = int(state_fips)
    if county is None:
      self.county = None
    else:
      self.county = int(county)

  def __eq__(self, it):
    return (self.path == it.path) and (self.state_fips == it.state_fips) and (self.county == it.county)

  def __hash__(self):
    return hash(self.path) ^ hash(self.state_fips) ^ hash(self.county)

  def __repr__(self):
    return str(self)

  def __str__(self):
    return 'CensusTigerBundle(%s, %s, %s)' % (self.path, self.state_fips, self.county)

  def __unicode__(self):
    return 'CensusTigerBundle(%s, %s, %s)' % (self.path, self.state_fips, self.county)


def getCensusTigerSetList(datadir, url, cachename, regex):
  cache_path = os.path.join(datadir, cachename)
  needs_fetch = cachedIndexNeedsFetch(cache_path)
  if needs_fetch:
    logging.debug('%s -> %s', url, cache_path)
    urllib.request.urlretrieve(url, cache_path)
  else:
    logging.debug('%s is new enough', cache_path)
  tigerSet = set()
  f = open(cache_path, 'r')
  for line in f:
    for match in regex.finditer(line):
      path = match.group(0)
      state_fips = int(match.group(1))
      if state_fips in states.ignored_fips:
        continue
      if match.lastindex == 2:
        county = int(match.group(2))
      else:
        county = None
      stname = states.nameForFips(state_fips)
      if not stname:
        logging.error('no state for fips=%r found as %s in %s', state_fips, match.group(0), cachename)
        continue
      else:
        logging.debug('found %s in %s', stname, cachename)
      tigerSet.add(CensusTigerBundle(path, state_fips, county))
  return tigerSet


def getFacesSet(datadir):
  return getCensusTigerSetList(datadir, FACES_URL, 'faces_index.html', FACES_RE)


def getEdgesSet(datadir):
  return getCensusTigerSetList(datadir, EDGES_URL, 'edges_index.html', EDGES_RE)


def getTabblockSet(datadir):
  return getCensusTigerSetList(datadir, TABBLOCK_URL, 'tabblock_index.html', TABBLOCK_RE)


def getCountySet(datadir):
  return getCensusTigerSetList(datadir, COUNTY_URL, 'county_index.html', COUNTY_RE)


# e.g. http://www2.census.gov/census_2020/01-Redistricting_File--PL_94-171/Virginia/va2020.pl.zip
# takes (state name, stl) like ('Virginia', 'va')
PL_ZIP_TEMPLATE = 'http://www2.census.gov/census_2020/01-Redistricting_File--PL_94-171/%s/%s2020.pl.zip'

class Crawler(object):
  def __init__(self, options):
    self.options = options
    self._faces = None
    self._edges = None
    self._tabblock = None
    self._county = None
    self.fetchCount = 0
    self.alreadyCount = 0

  @property
  def faces(self):
    if self._faces is None:
      self._faces = getFacesSet(self.options.datadir)
    return self._faces

  @property
  def edges(self):
    if self._edges is None:
      self._edges = getEdgesSet(self.options.datadir)
    return self._edges

  @property
  def tabblock(self):
    if self._tabblock is None:
      self._tabblock = getTabblockSet(self.options.datadir)
    return self._tabblock

  @property
  def county(self):
    if self._county is None:
      self._county = getCountySet(self.options.datadir)
    return self._county

  def _fetchSetForState(self, fips, tset, url, destdir):
    for it in tset:
      if it.state_fips == fips:
        dpath = os.path.join(destdir, it.path)
        if not os.path.exists(dpath):
          logging.info('%s -> %s', url + it.path, dpath)
          if not self.options.dryrun:
            urllib.request.urlretrieve(url + it.path, dpath)
          self.fetchCount += 1
        else:
          self.alreadyCount += 1

  def getState(self, stu):
    """Get all tabblock, edges, faces and county files for state."""
    fips = states.fipsForPostalCode(stu)
    if not fips:
      raise Exception('bogus postal code "%s"' % (stu,))
    destdir = os.path.join(self.options.datadir, stu, 'zips')
    logging.debug('fetching for %s (fips=%d) into %s', stu, fips, destdir)
    if not os.path.isdir(destdir):
      logging.debug('making destdir "%s"', destdir)
      if not self.options.dryrun:
        os.makedirs(destdir)
    self.fetchCount = 0
    self.alreadyCount = 0
    self._fetchSetForState(fips, self.tabblock, TABBLOCK_URL, destdir)
    self._fetchSetForState(fips, self.faces, FACES_URL, destdir)
    self._fetchSetForState(fips, self.edges, EDGES_URL, destdir)
    self._fetchSetForState(fips, self.county, COUNTY_URL, destdir)
    self.getPlaces(stu)
    self.getCountyNames(stu)
    logging.info('fetched %d and already had %d elements', self.fetchCount, self.alreadyCount)

  def getPlaces(self, stu):
    stl = stu.lower()
    fips = states.fipsForPostalCode(stl)
    destdir = os.path.join(self.options.datadir, stu)
    fname = 'st{fips:02d}_{stl}_places.txt'.format(fips=fips, stl=stl)
    fpath = os.path.join(destdir, fname)
    if os.path.exists(fpath):
      self.alreadyCount += 1
      return
    # http://www2.census.gov/geo/docs/reference/codes/files/st39_oh_places.txt
    url = 'http://www2.census.gov/geo/docs/reference/codes/files/st{fips:02d}_{stl}_places.txt'.format(fips=fips, stl=stl)
    urllib.request.urlretrieve(url, fpath)
    self.fetchCount += 1

  def getCountyNames(self, stu):
    stl = stu.lower()
    fips = states.fipsForPostalCode(stl)
    fname = 'st{fips:02d}_{stl}_cou.txt'.format(fips=fips, stl=stl)
    destdir = os.path.join(self.options.datadir, stu)
    fpath = os.path.join(destdir, fname)
    if os.path.exists(fpath):
      self.alreadyCount += 1
      return
    # http://www2.census.gov/geo/docs/reference/codes/files/st39_oh_cou.txt
    url = 'http://www2.census.gov/geo/docs/reference/codes/files/st{fips:02d}_{stl}_cou.txt'.format(fips=fips, stl=stl)
    urllib.request.urlretrieve(url, fpath)
    self.fetchCount += 1

  def _fetchAllSet(self, tset, url):
    for it in tset:
      stu = states.codeForFips(it.state_fips)
      assert stu, 'got stu=%r for fips=%r from it=%r' % (stu, it.state_fips, it)
      destdir = os.path.join(self.options.datadir, stu, 'zips')
      if not os.path.isdir(destdir):
        logging.debug('making destdir "%s"', destdir)
        if not self.options.dryrun:
          os.makedirs(destdir)
      dpath = os.path.join(destdir, it.path)
      if not os.path.exists(dpath):
        logging.info('%s -> %s', url + it.path, dpath)
        if not self.options.dryrun:
          urllib.request.urlretrieve(url + it.path, dpath)
        self.fetchCount += 1
      else:
        self.alreadyCount += 1

  def getAllStates(self):
    self.fetchCount = 0
    self.alreadyCount = 0
    self._fetchAllSet(self.tabblock, TABBLOCK_URL)
    self._fetchAllSet(self.faces, FACES_URL)
    self._fetchAllSet(self.edges, EDGES_URL)
    self._fetchAllSet(self.county, COUNTY_URL)
    for stu in self.getAllStulist():
      self.getPlaces(stu)
      self.getCountyNames(stu)
    logging.info('fetched %d and already had %d elements', self.fetchCount, self.alreadyCount)

  def getAllStulist(self):
    stulist = []
    for it in self.tabblock:
      stu = states.codeForFips(it.state_fips)
      assert(stu)
      # if stu == 'DC' or stu == 'PR':
      #   continue
      stulist.append(stu)
    return stulist


class ProcessGlobals(setupstatedata.ProcessGlobals):
  def __init__(self, options, crawley):
    super(ProcessGlobals, self).__init__(options)
    self.crawley = crawley
    # I think tigerlatest is mostly set non-None to block old code from running
    self.tigerlatest = 'https://www2.census.gov/geo/tiger/TIGER2020PL/'

  def getState(self, name):
    return StateData(self, name, self.options)


def pl94_171_2020_ubid(line):
  """From a line of a PL94-171 (2020) file, return the 'unique block id' used by Redistricter system.
  {state}{county}{tract}{block}
  This winds up being a 15 digit decimal number that fits in a 64 bit unsigned int."""
  return line[27:32] + line[54:60] + line[61:65]

def readPlaceNames(placeNamesPath):
  placeNames = {}
  with open(placeNamesPath) as ppin:
    for line in ppin:
      if not line:
        continue
      line = line.strip()
      if not line:
        continue
      if line[0] == '#':
        continue
      parts = line.split('|')
      place = parts[2] # int(parts[2])
      name = parts[3]
      oldname = placeNames.get(place)
      if (oldname is not None) and (oldname != name):
        logging.warn('differing names for place %d: %r %r', place, oldname, name)
      else:
        placeNames[place] = name
  logging.debug('read %d names from %r', len(placeNames), placeNamesPath)
  return placeNames


class GeoBlocksPlaces(object):
  """
  Receives lines from {stl}2020.pl.zip/{stl}geo2020.pl filtered at '750' summary level.
        Gather place:block-ubid mappings.
        Write to binary or text format.
        Also collect place:population summary; write to csv.
  """
  ACTIVE_INCORPORATED_PLACE_CODES = ('C1', 'C2', 'C5', 'C6', 'C7', 'C8')
  def __init__(self):
    self.places = {}
    self.placePops = {}

  def pl2020line(self, line):
    """accumulate a line of a PL94-171 (2020) file
                should have been filtered to summary level 750 which has
                one record for each block"""
    placecode = line[50:52]
    if placecode in self.ACTIVE_INCORPORATED_PLACE_CODES:
      place = line[45:50]
      ubid = pl94_171_2020_ubid(line)
      placelist = self.places.get(place)
      if placelist is None:
        placelist = [ubid]
        self.places[place] = placelist
      else:
        placelist.append(ubid)
      pop = int(line[318:327])
      self.placePops[place] = self.placePops.get(place,0) + pop

  def writePlaceUbidMap(self, out):
    # TODO: unused
    for place, placelist in self.places.items():
      out.write(place)
      out.write('\x1d') # group sep
      out.write('\x1c'.join(placelist)) # field sep
      out.write('\n') # this will make it easier to `less` the file
      #out.write('\x1e') # record sep

  def writeUbidPlaceMap(self, out):
    """Write (ubid uint64, place uint64) file.
    Header (version=1 uint64, num records uint64)"""
    they = []
    for place, placelist in self.places.items():
      for place_ubid in placelist:
        placei = int(place)
        assert placei != 0
        they.append( (int(place_ubid), placei) )
    # version, number of records
    out.write(struct.pack('=QQ', 1, len(they)))
    # sort so that result can be loaded into a block of memory and binary searched on ubid
    they.sort()
    for place_ubid, place in they:
      # two uint64
      # uint64 is overkill for the 5-digit-decimal 'place', but it makes the whole thing mmap-able and keeps everything 64 bit aligned nicely.
      out.write(struct.pack('=QQ', place_ubid, place))

  def writePlacePops(self, out, placeNamesPath):
    placeNames = readPlaceNames(placeNamesPath)
    bypop = [(pop,place) for place,pop in self.placePops.items()]
    bypop.sort(reverse=True)
    writer = csv.writer(out)
    for pop, place in bypop:
      name = placeNames.get(place, '')
      writer.writerow([pop, place, name])


class StateData(setupstatedata.StateData):
  def __init__(self, pg, st, options):
    super(StateData, self).__init__(pg, st, options)

  def downloadTigerZips(self, dpath):
    """Return a list of tabblock*zip files."""
    out = []
    for it in self.pg.crawley.tabblock:
      if it.state_fips == self.fips:
        out.append(it.path)
    if not out:
      sys.stderr.write('%s (%d) not in: %r' % (self.stu, self.fips, self.pg.crawley.tabblock))
    return out

  def getGeoBlocks(self):
    """From xx2020.pl.zip get block level geo file.
        -> geoblocks filtered file of per-block data
        -> geoblocks.places binary ubid:block map
        -> placespops.csv summary placeid:pop csv"""
    plzip = os.path.join(self.dpath, 'zips', self.stl + '2020.pl.zip')
    geoblockspath = os.path.join(self.dpath, 'geoblocks')
    placespath = geoblockspath + '.places'
    placePopPath = os.path.join(self.dpath, 'placespops.csv')
    if not os.path.exists(plzip):
      plzipurl = PL_ZIP_TEMPLATE % (self.name.replace(' ', '_'), self.stl)
      logger.info('%s -> %s', plzipurl, plzip)
      if not self.options.dryrun:
        urllib.request.urlretrieve(plzipurl, plzip)
    assert os.path.exists(plzip), "missing %s" % (plzip,)
    needsbuild = newerthan(plzip, geoblockspath)
    needsbuild = needsbuild or newerthan(plzip, placespath)
    needsbuild = needsbuild or newerthan(plzip, placePopPath)
    if not needsbuild:
      return

    # process a file within the zip into a filtered geo info file and two derivitive files about places (cities)
    logger.info('%s -> %s , %s , %s', plzip, geoblockspath, placespath, placePopPath)
    if self.options.dryrun:
      return
    places = GeoBlocksPlaces()
    fo = open(geoblockspath, 'w')
    zf = zipfile.ZipFile(plzip, 'r')
    raw = zf.read(self.stl + 'geo2020.pl').decode()
    zf.close()
    filter = 'PLST  ' + self.stu + '750'
    for line in raw.splitlines(True):
      if line.startswith(filter):
        fo.write(line)
        places.pl2020line(line)
    fo.close()
    with gzip.open(placespath, 'wb') as placefile:
      places.writeUbidPlaceMap(placefile)
      placefile.flush()
    placesNamesPath = os.path.join(self.dpath, 'st{fips:02d}_{stl}_places.txt'.format(fips=self.fips, stl=self.stl))
    with open(placePopPath, 'w') as pp:
      places.writePlacePops(pp, placesNamesPath)

  def compileBinaryData(self):
    geoblockspath = os.path.join(self.dpath, 'geoblocks')
    #linkspath = os.path.join(self.dpath, self.stl + '101.uf1.links')
    binpath = os.path.join(self.options.bindir, 'linkfixup')
    outpath = os.path.join(self.dpath, self.stl + '.pb')
    cmd = [binpath, '--plgeo', geoblockspath, '-p', outpath]
    needsbuild = newerthan(geoblockspath, outpath)
    #needsbuild = needsbuild or newerthan(linkspath, outpath)
    needsbuild = needsbuild or newerthan(binpath, outpath)
    if not needsbuild:
      return
    logger.info('"' + '" "'.join(cmd) + '"')
    if self.options.dryrun:
      return
    start = time.time()
    logger.debug('compile binary data: %s', ' '.join(['"' + x + '"' for x in cmd]))
    status = subprocess.call(cmd)
    logger.info('data compile took %f seconds', time.time() - start)
    if status != 0:
      raise Exception('error (%d) executing: cd %s && "%s"' % (status, self.dpath,'" "'.join(cmd)))

  def placelist(self):
    """placespops.csv -> highlightPlaces.txt short list of place ids"""
    placelistPath = os.path.join(self.dpath, 'highlightPlaces.txt')
    if not os.path.exists(placelistPath):
      placePopPath = os.path.join(self.dpath, 'placespops.csv')
      logger.info('%s -> %s', placePopPath, placelistPath)
      if self.options.dryrun:
        return None
      places = getTopNPlaceCodes(placePopPath)
      with open(placelistPath, 'w') as fout:
        fout.write(' '.join(places))
        fout.write('\n')
      return list(map(int, places))

    with open(placelistPath, 'r') as fin:
      line = next(fin)
      line = line.strip()
      return list(map(int, line.split(' ')))

  def buildHighlightBlocklist(self):
    """highlightPlaces.txt + geoblocks.places -> highlight.ubidz binary ubid list"""
    places = self.placelist()
    placesPath = os.path.join(self.dpath, 'geoblocks.places')
    highlightBlocklistPath = os.path.join(self.dpath, 'highlight.ubidz')
    logger.info('%s[%r] -> %s', placesPath, places, highlightBlocklistPath)
    if self.options.dryrun:
      return
    filterPlacesToUbidList(placesPath, places, highlightBlocklistPath)

  def dostate_inner(self):
    linkspath = self.processShapefile(self.dpath)
    if not linkspath:
      logger.info('processShapefile failed')
      return False
    csvpath = os.path.join(os.path.dirname(__file__), 'legislatures2020.csv')
    generaterunconfigs.run(
      datadir=self.options.datadir,
      stulist=[self.stu],
      dryrun=self.options.dryrun,
      csvpath=csvpath,
    )
    self.getGeoBlocks()
    self.compileBinaryData()
    self.buildHighlightBlocklist()
    if self.options.archive_runfiles:
      start = time.time()
      outname = self.archiveRunfiles()
      logger.info('wrote "%s" in %f seconds', outname, (time.time() - start))
    return True


def strbool(x):
  if (not x) or (x.lower() == 'false'):
    return False
  return True


def main():
  argp = setupstatedata.getOptionParser()
  argp.add_option('--download', dest='download', default='True', action='store_true')
  argp.add_option('--no-download', dest='download', action='store_false')
  (options, args) = argp.parse_args()
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)
    logging.debug('options=%r', options)
  else:
    logging.basicConfig(level=logging.INFO)
  assert options.shapefile
  if not os.path.isdir(options.datadir):
    raise Exception('data dir "%s" does not exist' % options.datadir)
  stulist = []
  doall = False
  crawley = Crawler(options)
  for arg in args:
    arg = arg.upper()
    if arg == 'ALL':
      doall = True
      stulist = []
      break
    stulist.append(arg)
  if doall and stulist:
    logging.error('specified ALL and some things which appear to be states: %r', stulist)
    sys.exit(2)
  if options.download:
    if doall:
      crawley.getAllStates()
    else:
      for arg in stulist:
        logging.debug('starting %s', arg)
        crawley.getState(arg)
  if doall:
    stulist = crawley.getAllStulist()
  pg = ProcessGlobals(options, crawley)
  setupstatedata.runMaybeThreaded(stulist, pg, options)


if __name__ == '__main__':
  main()
