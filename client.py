#!/usr/bin/python

"""Get data from a server, send results back.

The server config page should have a ConfigParser compatible configuartion
block wrapped matching:
'<pre class="config">(.*)</pre>'
Data files should be linked to with links matching
'<a class="data" href="([^"]+)">([^<]+)</a>'
"""

import ConfigParser
import httplib
import logging
import os
import random
import re
try:
	import cStringIO as StringIO
except:
	import StringIO
import tarfile
import time
import traceback
import urllib
import urlparse

from newerthan import newerthan

HREF_SCRAPER_ = re.compile(r'<a class="data" href="([^"]+)">([^<]+)</a>', re.IGNORECASE)
CONFIG_BLOCK_ = re.compile(r'<pre class="config">(.*)</pre>', re.IGNORECASE|re.DOTALL|re.MULTILINE)

def yeildHrefFromData(raw):
	"""Yield (href, name) pairs."""
	for m in HREF_SCRAPER_.finditer(raw):
		yield (m.group(1), m.group(2))


def makeUrlAbsolute(url, base):
	"""Return absolute version of url, which may be relative to base."""
	# (scheme, machine, path, query, tag)
	pu = list(urlparse.urlsplit(url))
	puUpdated = False
	pb = urlparse.urlsplit(base)
	# scheme: http/https
	if not pu[0]:
		pu[0] = pb[0]
		puUpdated = True
	# host:port
	if not pu[1]:
		pu[1] = pb[1]
		puUpdated = True
	assert pu[2]
	if not pu[2].startswith('/'):
		# make relative path absolute
		if pb[2].endswith('/'):
			basepath = pb[2]
		else:
			basepath = pb[2].rsplit('/', 1)[0] + '/'
		pu[2] = basepath + pu[2]
		puUpdated = True
	if puUpdated:
		return urlparse.urlunsplit(pu)
	# no change, return as was
	return url


class InvalidArgument(Exception):
	pass

# TODO: put data at bdistricting.com and point these there
# TODO-way-long-term: crypto-signed auto updating of scripts and binaries
CLIENT_DEFAULTS_ = {
	'configurl': 'http://127.0.0.1:8111/config',
#	'dataurl': 'http://127.0.0.1:8111/data/',
	'submiturl': 'http://127.0.0.1:8111/submit',
	'configCacheSeconds': (3600 * 24),
}

def fileOlderThan(filename, seconds):
	st = os.stat(filename)
	return (st.st_mtime + seconds) < time.time()

class Client(object):
	def __init__(self, options):
		self.options = options
		# map from name of dataset to its download URL
		self.knownDatasets = {}
		self.config = ConfigParser.SafeConfigParser(CLIENT_DEFAULTS_)
		self.config.add_section('config')
		self.loadConfiguration()
	
	def configCachePath(self):
		return os.path.join(self.options.datadir, '.configCache')
	
	def datasetListPath(self):
		return os.path.join(self.options.datadir, '.datasetList')

	def loadConfiguration(self):
		cpath = self.configCachePath()
		needsConfigFetch = self.options.force_config_reload
		if not needsConfigFetch:
			try:
				cstat = os.stat(cpath)
				logging.info('reading cached config "%s"', cpath)
				self.config.read(cpath)
				if (cstat.st_mtime + self.config.getint('config', 'configCacheSeconds')) < time.time():
					needsConfigFetch = True
			except Exception, e:
				logging.info('stat("%s") failed: %s', cpath, e)
				needsConfigFetch = True
		if not needsConfigFetch:
			logging.info('config cache is new enough, not fetching')
			f = open(self.datasetListPath(), 'r')
			self.knownDatasets = cgi.parse_qs(f.read())
			f.close()
			return
		# Fetch config from server
		if self.options.server:
			configurl = self.options.server
		else:
			configurl = self.config.get('config', 'configurl')
		logging.info('fetching config from "%s"', configurl)
		f = urllib.urlopen(configurl)
		raw = f.read()
		if f.code != 200:
			logging.error('error fetching config from "%s": (%s)',
				configurl, f.code)
			logging.debug('error page contents="%s"', repr(raw))
			return
		f.close()
		for href, name in yeildHrefFromData(raw):
			self.knownDatasets[name] = makeUrlAbsolute(href, configurl)
			print name
		m = CONFIG_BLOCK_.search(raw)
		if m:
			sf = StringIO.StringIO(m.group(1))
			self.config.readfp(sf)
		self.saveState()

	def saveState(self):
		configCache = open(self.configCachePath(), 'wb')
		self.config.write(configCache)
		configCache.close()
		datasetList = open(self.datasetListPath(), 'w')
		datasetList.write(urllib.urlencode(self.knownDatasets))
		datasetList.close()

	def headLastModified(self, url):
		try:
			urlparts = urlparse.urlparse(url)
			conn = httplib.HTTPConnection(urlparts[1])
			conn.request('HEAD', urlparts[2], headers={
				'Host':urlparts[1].split(':')[0]})
			response = conn.getresponse()
			lastmodsrv = time.mktime(response.msg.getdate('last-modified'))
			nowsrv = time.mktime(response.msg.getdate('date'))
			nowhere = time.time()
			lastmod = lastmodsrv - nowsrv + nowhere
			return lastmod
		except Exception, e:
			logging.warning('failed to HEAD lastmod from "%s": %s', url, e)
			traceback.print_exc()
		return None

	def fetchIfServerCopyNewer(self, dataset, remoteurl):
		# TODO: use httplib to do HEAD requests for last-modified:
		# for now, never re-fetch
		localpath = os.path.join(self.options.datadir, dataset)
		#remoteurl = self.config.get('config', 'dataurl') + dataset
		if os.path.exists(localpath):
			st = os.stat(localpath)
			srvlastmod = self.headLastModified(remoteurl)
			if (srvlastmod is not None) and (srvlastmod < st.st_mtime):
				logging.info('local copy "%s" newer than server, not fetching "%s"', localpath, remoteurl)
				return localpath
		logging.info('fetch "%s" to "%s"', remoteurl, localpath)
		(filename, headers) = urllib.urlretrieve(remoteurl, localpath)
		# TODO: urlretrieve doesn't return code! Make something better
		# that reports code and doesn't clobber data on non-200.
		return localpath

	def archiveToDirName(self, archivename):
		suffix = '_runfiles.tar.gz'
		if archivename.endswith(suffix):
			return archivename[:-len(suffix)]
		raise InvalidArgument('cannot parse archive name "%s"' % archivename)

	def unpackArchive(self, archivename, dataurl):
		dirname = self.archiveToDirName(archivename)
		dirpath = os.path.join(self.options.datadir, dirname)
		archpath = self.fetchIfServerCopyNewer(archivename)
		markerPath = os.path.join(dirpath, '.unpackmarker')
		needsUnpack = newerthan(archpath, markerPath)
		if not needsUnpack:
			return
		tf = tarfile.open(archpath, 'r|gz')
		tf.extractall(self.options.datadir)
		tf.close()
		upm = open(markerPath, 'w')
		upm.write(str(time.time()) + '\n')
		upm.close()
		return dirpath
	
	def getDataForStu(self, stu):
		archivename = stu + '_runfiles.tar.gz'
		if archivename in self.knownDatasets:
			return self.unpackArchive(archivename, self.knownDatasets[archivename])
		return None
	
	def randomDatasetName(self):
		return random.choice(self.knownDatasets.keys())


if __name__ == '__main__':
	import optparse
	argp = optparse.OptionParser()
	argp.add_option('-d', '--data', '--datadir', dest='datadir', default='data')
	argp.add_option('--server', dest='server', default=None, help='url of config page on server from which to download data')
	argp.add_option('--force-config-reload', dest='force_config_reload', action='store_true', default=False)
	argp.add_option('--verbose', '-v', dest='verbose', action='store_true', default=False)
	(options, args) = argp.parse_args()
	if options.verbose:
		logging.getLogger().setLevel(logging.DEBUG)
	c = Client(options)
	c.loadConfiguration()
	for arch, url in c.knownDatasets.iteritems():
		c.unpackArchive(arch, url)
