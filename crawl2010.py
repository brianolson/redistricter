#!/usr/bin/python

# standard
import logging
import optparse
import os
import re
import states
import subprocess
import sys
import time
import urllib
import zipfile

# local
import generaterunconfigs
import linksfromedges
from newerthan import newerthan
import setupstatedata
import shapefile

TABBLOCK_URL = 'http://www2.census.gov/geo/tiger/TIGER2010/TABBLOCK/2010/'
FACES_URL = 'http://www2.census.gov/geo/tiger/TIGER2010/FACES/'
EDGES_URL = 'http://www2.census.gov/geo/tiger/TIGER2010/EDGES/'
COUNTY_URL = 'http://www2.census.gov/geo/tiger/TIGER2010/COUNTY/2010/'

# Three days
INDEX_CACHE_SECONDS = 3*24*3600

def cachedIndexNeedsFetch(path):
	try:
		st = os.stat(path)
		if (time.time() - st.st_mtime) < INDEX_CACHE_SECONDS:
			# exists and is new enough
			return False
	except OSError, e:
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
		return u'CensusTigerBundle(%s, %s, %s)' % (self.path, self.state_fips, self.county)


def getCensusTigerSetList(datadir, url, cachename, regex):
	cache_path = os.path.join(datadir, cachename)
	needs_fetch = cachedIndexNeedsFetch(cache_path)
	if needs_fetch:
		logging.debug('%s -> %s', url, cache_path)
		urllib.urlretrieve(url, cache_path)
	else:
		logging.debug('%s is new enough', cache_path)
	tigerSet = set()
	f = open(cache_path, 'r')
	for line in f:
		for match in regex.finditer(line):
			path = match.group(0)
			state_fips = int(match.group(1))
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


# tl_2010_01001_faces.zip
FACES_RE = re.compile(r'tl_2010_(\d\d)(\d\d\d)_faces.zip', re.IGNORECASE)


def getFacesSet(datadir):
	return getCensusTigerSetList(datadir, FACES_URL, 'faces_index.html', FACES_RE)


#tl_2010_01001_edges.zip
EDGES_RE = re.compile(r'tl_2010_(\d\d)(\d\d\d)_edges.zip', re.IGNORECASE)


def getEdgesSet(datadir):
	return getCensusTigerSetList(datadir, EDGES_URL, 'edges_index.html', EDGES_RE)


#tl_2010_01_tabblock10.zip
TABBLOCK_RE = re.compile(r'tl_2010_(\d\d)_tabblock10.zip', re.IGNORECASE)


def getTabblockSet(datadir):
	return getCensusTigerSetList(datadir, TABBLOCK_URL, 'tabblock_index.html', TABBLOCK_RE)


#tl_2010_01_county10.zip
COUNTY_RE = re.compile(r'tl_2010_(\d\d)_county10.zip', re.IGNORECASE)


def getCountySet(datadir):
	return getCensusTigerSetList(datadir, COUNTY_URL, 'county_index.html', COUNTY_RE)


# e.g. http://www2.census.gov/census_2010/01-Redistricting_File--PL_94-171/Virginia/va2010.pl.zip
# takes (state name, stl) like ('Virginia', 'va')
PL_ZIP_TEMPLATE = 'http://www2.census.gov/census_2010/01-Redistricting_File--PL_94-171/%s/%s2010.pl.zip'

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
						urllib.urlretrieve(url + it.path, dpath)
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
		logging.info('fetched %d and already had %d elements', self.fetchCount, self.alreadyCount)
	
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
					urllib.urlretrieve(url + it.path, dpath)
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
		logging.info('fetched %d and already had %d elements', self.fetchCount, self.alreadyCount)
	
	def getAllStulist(self):
		stulist = []
		for it in self.tabblock:
			stu = states.codeForFips(it.state_fips)
			assert(stu)
			if stu == 'DC':
				continue
			stulist.append(stu)
		return stulist


class ProcessGlobals(setupstatedata.ProcessGlobals):
	def __init__(self, options, crawley):
		super(ProcessGlobals, self).__init__(options)
		self.crawley = crawley
		self.tigerlatest = 'http://www2.census.gov/geo/tiger/TIGER2010/'
	
	def getState(self, name):
		return StateData(self, name, self.options)


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
		"""From xx2010.pl.zip get block level geo file."""
		plzip = os.path.join(self.dpath, 'zips', self.stl + '2010.pl.zip')
		geoblockspath = os.path.join(self.dpath, 'geoblocks')
		# TODO: maybe fetch
		if not os.path.exists(plzip):
			plzipurl = PL_ZIP_TEMPLATE % (self.name.replace(' ', '_'), self.stl)
			self.logf('%s -> %s', plzipurl, plzip)
			if not self.options.dryrun:
				urllib.urlretrieve(plzipurl, plzip)
		assert os.path.exists(plzip), "missing %s" % (plzip,)
		if not newerthan(plzip, geoblockspath):
			return
		self.logf('%s -> %s', plzip, geoblockspath)
		if self.options.dryrun:
			return
		fo = open(geoblockspath, 'w')
		zf = zipfile.ZipFile(plzip, 'r')
		raw = zf.read(self.stl + 'geo2010.pl')
		zf.close()
		filter = 'PLST  ' + self.stu + '750'
		for line in raw.splitlines(True):
			if line.startswith(filter):
				fo.write(line)
		fo.close()
	
	def compileBinaryData(self):
		geoblockspath = os.path.join(self.dpath, 'geoblocks')
		linkspath = os.path.join(self.dpath, self.stl + '101.uf1.links')
		binpath = os.path.join(self.options.bindir, 'linkfixup')
		outpath = os.path.join(self.dpath, self.stl + '.pb')
		cmd = [binpath, '--plgeo', geoblockspath, '-p', outpath]
		needsbuild = newerthan(geoblockspath, outpath)
		needsbuild = needsbuild or newerthan(linkspath, outpath)
		needsbuild = needsbuild or newerthan(binpath, outpath)
		if not needsbuild:
			return
		self.logf('cd %s && "%s"', self.dpath, '" "'.join(cmd))
		if self.options.dryrun:
			return
		start = time.time()
		status = subprocess.call(cmd, cwd=self.dpath)
		self.logf('data compile took %f seconds', time.time() - start)
		if status != 0:
			raise Exception('error (%d) executing: cd %s && "%s"' % (status, self.dpath,'" "'.join(cmd)))
	
	def dostate_inner(self):
		linkspath = self.processShapefile(self.dpath)
		if not linkspath:
			self.logf('processShapefile failed')
			return False
		self.getGeoBlocks()
		self.compileBinaryData()
		generaterunconfigs.run(
			datadir=self.options.datadir,
			stulist=[self.stu],
			dryrun=self.options.dryrun)
		if self.options.archive_runfiles:
			start = time.time()
			outname = self.archiveRunfiles()
			self.logf('wrote "%s" in %f seconds', outname, (time.time() - start))
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
		logging.getLogger().setLevel(logging.DEBUG)
		logging.debug('options=%r', options)
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
