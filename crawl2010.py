#!/usr/bin/python

import logging
import os
import re
import states
import sys
import time
import urllib

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
		urllib.urlretrieve(url, cache_path)
	tigerSet = set()
	f = open(cache_path, 'r')
	for line in f:
		for match in regex.finditer(line):
			path = match.group(0)
			state_fips = match.group(1)
			if match.lastindex == 2:
				county = match.group(2)
			else:
				county = None
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
			os.makedirs(destdir)
		self.fetchCount = 0
		self.alreadyCount = 0
		self._fetchSetForState(fips, self.tabblock, TABBLOCK_URL, destdir)
		self._fetchSetForState(fips, self.faces, FACES_URL, destdir)
		self._fetchSetForState(fips, self.edges, EDGES_URL, destdir)
		self._fetchSetForState(fips, self.county, COUNTY_URL, destdir)
		logging.info('fetched %d and already had %d elements', self.fetchCount, self.alreadyCount)


def main():
	# TODO: extract multiply copied code for bindir and datadir to one common source?
	default_bindir = os.environ.get('REDISTRICTER_BIN')
	if default_bindir is None:
		default_bindir = os.path.dirname(os.path.abspath(__file__))
	default_datadir = os.environ.get('REDISTRICTER_DATA')
	if default_datadir is None:
		default_datadir = os.path.join(default_bindir, 'data')
	import optparse
	argp = optparse.OptionParser()
	argp.add_option('-n', '--dry-run', action='store_true', dest='dryrun', default=False)
	argp.add_option('-d', '--data', dest='datadir', default=default_datadir)
	argp.add_option('--verbose', dest='verbose', action='store_true', default=False)
	(options, args) = argp.parse_args()
	if options.verbose:
		logging.getLogger().setLevel(logging.DEBUG)
	if not os.path.isdir(options.datadir):
		raise Exception('data dir "%s" does not exist' % options.datadir)
	crawley = Crawler(options)
	for arg in args:
		arg = arg.upper()
		logging.debug('starting %s', arg)
		crawley.getState(arg)
		

if __name__ == '__main__':
	main()
