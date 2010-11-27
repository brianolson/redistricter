#!/usr/bin/python

"""Get data from a server, send results back.

The server config page should have a ConfigParser compatible configuartion
block wrapped matching:
'<pre class="config">(.*)</pre>'
Data files should be linked to with links matching
'<a class="data" href="([^"]+)">([^<]+)</a>'
"""

import anydbm
import ConfigParser
#import email.message  # doesn't really work for posting multipart/form-data
import httplib
import logging
import os
import random
import re
try:
	import cStringIO as StringIO
except:
	import StringIO
import random
import sys
import tarfile
import time
import traceback
import urllib
import urllib2
import urlparse

from newerthan import newerthan

rand = random.Random()
HREF_SCRAPER_ = re.compile(r'<a class="data" href="([^"]+)">([^<]+)</a>', re.IGNORECASE)
CONFIG_BLOCK_ = re.compile(r'<pre class="config">(.*?)</pre>', re.IGNORECASE|re.DOTALL|re.MULTILINE)
RUNOPTS_BLOCK_ = re.compile(r'<pre class="runopts">(.*?)</pre>', re.IGNORECASE|re.DOTALL|re.MULTILINE)
LAST_MODIFIED_COMMENT_ = re.compile(r'<!-- last-modified: (.*) -->', re.IGNORECASE)

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


def getIfNewer(url, path, lastModifiedString):
	try:
		req = urllib2.Request(url, data=None, headers={'If-Modified-Since': lastModifiedString})
		uf = urllib2.urlopen(req)
		raw = uf.read()
		info = uf.info()
		fout = open(path, 'wb')
		fout.write(raw)
		fout.close()
		return (info['last-modified'], raw)
	except urllib2.HTTPError, he:
		if he.code == 304:
			return (lastModifiedString, None)
		raise

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

EXAMPLE_CONFIG_ = """[config]
configurl: http://127.0.0.1:8111/config
submiturl: http://127.0.0.1:8111/submit
"""

def fileOlderThan(filename, seconds):
	st = os.stat(filename)
	return (st.st_mtime + seconds) < time.time()

class Client(object):
	def __init__(self, options):
		self.options = options
		# map from name of dataset to its download URL
		self.knownDatasets = {}
		# 'last-modified' headers for downloaded files.
		# Sent back in 'if-modified-since' GETs
		self.lastmods = anydbm.open(os.path.join(self.options.datadir, '.lastmod.db'), 'c')
		# Data to be parsed by runallstates.py runallstates.applyConfigOverrideLine
		self.runoptsraw = None
		
		self.config = ConfigParser.SafeConfigParser(CLIENT_DEFAULTS_)
		self.config.add_section('config')
		self.loadConfiguration()
	
	def configCachePath(self):
		return os.path.join(self.options.datadir, '.configCache')
	
	def configUrl(self):
		if self.options.server:
			return self.options.server
		return self.config.get('config', 'configurl')
	
	def runOptionLines(self):
		if not self.runoptsraw:
			return []
		return self.runoptsraw.splitlines()
	
	def loadConfiguration(self):
		"""Load config from server or cache.
		Maybe read cache, maybe refetch config and store it to cache.
		"""
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
		if needsConfigFetch and self.options.dry_run and os.path.exists(cpath):
			logging.info('using cached config for dry-run')
			needsConfigFetch = False
		if not needsConfigFetch:
			logging.info('config cache is new enough, not fetching')
			f = open(cpath, 'rb')
			self.parseHtmlConfigPage(f.read())
			f.close()
			return
		GET_headers = {}
		if_modified_since = self.lastmods.get('.configCache')
		if if_modified_since:
			GET_headers['If-Modified-Since'] = if_modified_since
		# Fetch config from server
		configurl = self.configUrl()
		logging.info('fetching config from "%s"', configurl)
		if self.options.dry_run:
			logging.info('skipping fetch for dry-run')
			return
		didFetch = False
		lastModified = None
		raw = None
		try:
			req = urllib2.Request(configurl, data=None, headers=GET_headers)
			f = urllib2.urlopen(req)
			raw = f.read()
			info = f.info()
			assert (not hasattr(info, 'code')) or info.code == 200
			lastModified = info['last-modified']
			f.close()
			didFetch = True
		except urllib2.HTTPError, he:
			if he.code == 304:
				# Not modified. Load cached copy.
				f = open(cpath, 'rb')
				raw = f.read()
				f.close()
			else:
				raise
		assert raw
		ok = self.parseHtmlConfigPage(raw)
		if ok and didFetch:
			# write out to cached copy
			fout = open(cpath, 'wb')
			fout.write(raw)
			fout.close()
			if lastModified:
				self.lastmods['.configCache'] = lastModified
				if hasattr(self.lastmods, 'sync'):
					self.lastmods.sync()
	
	def parseHtmlConfigPage(self, raw):
		for href, name in yeildHrefFromData(raw):
			self.knownDatasets[name] = makeUrlAbsolute(href, self.configUrl())
			logging.debug('client config dataset "%s"', name)
		m = CONFIG_BLOCK_.search(raw)
		if m:
			sf = StringIO.StringIO(m.group(1))
			self.config.readfp(sf)
		m = RUNOPTS_BLOCK_.search(raw)
		if m:
			self.runoptsraw = m.group(1)
		return True
	
	def fetchIfServerCopyNewer(self, dataset, remoteurl):
		localpath = os.path.join(self.options.datadir, dataset)
		if self.options.dry_run:
			logging.info('fetchIfServerCopyNewer "%s" -> "%s"', remoteurl, localpath)
			return localpath
		lastmod = self.lastmods.get(dataset)
		GET_headers = {}
		if lastmod:
			GET_headers['If-Modified-Since'] = lastmod
		try:
			logging.info('fetch "%s" to "%s"', remoteurl, localpath)
			req = urllib2.Request(remoteurl, data=None, headers=GET_headers)
			uf = urllib2.urlopen(req)
			raw = uf.read()
			info = uf.info()
			assert (not hasattr(info, 'code')) or info.code == 200
			lastModified = info['last-modified']
			fout = open(localpath, 'wb')
			fout.write(raw)
			fout.close()
			if lastModified:
				self.lastmods[dataset] = lastModified
				if hasattr(self.lastmods, 'sync'):
					self.lastmods.sync()
		except urllib2.HTTPError, he:
			if he.code == 304:
				# Not modified.
				pass
			else:
				raise
		return localpath
	
	def archiveToDirName(self, archivename):
		suffix = '_runfiles.tar.gz'
		if archivename.endswith(suffix):
			return archivename[:-len(suffix)]
		raise InvalidArgument('cannot parse archive name "%s"' % archivename)
	
	def unpackArchive(self, archivename, dataurl=None):
		"""Download (with caching) and unpack a dataset."""
		if dataurl is None:
			dataurl = self.knownDatasets[archivename]
			assert dataurl is not None
		dirname = self.archiveToDirName(archivename)
		dirpath = os.path.join(self.options.datadir, dirname)
		archpath = self.fetchIfServerCopyNewer(archivename, dataurl)
		markerPath = os.path.join(dirpath, '.unpackmarker')
		needsUnpack = newerthan(archpath, markerPath)
		if not needsUnpack:
			return
		if self.options.dry_run:
			logging.info('would unpack "%s" to "%s"', archpath, self.options.datadir)
			return
		tf = tarfile.open(archpath, 'r|gz')
		tf.extractall(self.options.datadir)
		tf.close()
		upm = open(markerPath, 'w')
		upm.write(str(time.time()) + '\n')
		upm.close()
		return dirpath
	
	def getDataForStu(self, stu):
		"""Download (with caching) and unpack a dataset."""
		archivename = stu + '_runfiles.tar.gz'
		if archivename in self.knownDatasets:
			return self.unpackArchive(archivename, self.knownDatasets[archivename])
		else:
			sys.stderr.write('dataset "%s" not known on server (known=%r)\n' % (archivename, self.knownDatasets.keys()))
		return None
	
	def randomDatasetName(self):
		"""Called to pick a random dataset known to the server to download and work on."""
		# TODO: find out what the server would prefer us to work on.
		return random.choice(self.knownDatasets.keys())
	
	def sendResultDir(self, resultdir, vars=None):
		# It kinda sucks that I have to reimplement MIME composition here
		# but the standard library is really designed for email and not http.
		submiturl = self.config.get('config', 'submiturl')
		if not submiturl:
			sys.stderr.write('cannot send to server because no submiturl is configured\n')
			return
		sentmarker = os.path.join(resultdir, 'sent')
		if os.path.exists(sentmarker):
			sys.stderr.write('found "%s", already sent dir %s, not sending again\n' % (sentmarker, resultdir))
			return
		bkpath = os.path.join(resultdir, 'bestKmpp.dsz')
		if not os.path.exists(bkpath):
			sys.stderr.write('trying to send result dir "%s" but there is no bestKmpp.dsz\n' % resultdir)
			return
		def partMsg(parent, path, mtype, name):
			try:
				raw = open(path, 'rb').read()
			except:
				sys.stderr.write('failed to read file "%s"\n' % path)
				return
			if not raw:
				return
			x = MyMimePart(name=name, mtype=mtype, filename=name, value=raw)
			parent.append(x)
		outer = []
		partMsg(outer, bkpath, 'application/octet-stream', 'solution')
		#partMsg(outer, os.path.join(resultdir, 'statlog.gz'), 'application/gzip', 'statlog')
		partMsg(outer, os.path.join(resultdir, 'binlog'), 'application/octet-stream', 'binlog')
		partMsg(outer, os.path.join(resultdir, 'statsum'), 'text/plain', 'statsum')
		if vars:
			outer.append(MyMimePart(name='vars', mtype='text/plain', value=urllib.urlencode(vars)))
		boundary = '===============%d%d==' % (
			rand.randint(1000000000,2000000000), rand.randint(1000000000,2000000000))
		body = StringIO.StringIO()
		ddboundary = '--' + boundary
		ddbcrlf = ddboundary + '\r\n'
		for part in outer:
			body.write(ddbcrlf)
			part.write(body)
		body.write(ddboundary)
		body.write('--\r\n\r\n')
		sbody = body.getvalue()
		GET_headers = {
			'Content-Type': 'multipart/form-data; charset="utf-8"; boundary=' + boundary,
			'Content-Length': str(len(sbody))}
		print 'sending to ' + submiturl
		req = urllib2.Request(submiturl, data=sbody, headers=GET_headers)
		try:
			uf = urllib2.urlopen(req)
			retval = uf.read()
			print retval
			# mark dir as sent so we don't re-send it
			if retval.startswith('ok'):
				tf = open(sentmarker, 'w')
				tf.write(time.ctime() + '\n')
				tf.close()
		except Exception, e:
			print 'send result failed for %s due to %r' % (resultdir, e)
		print 'Done'


class MyMimePart(object):
	def __init__(self, name=None, path=None, mtype=None, filename=None, value=None):
		self.name = name
		self.path = path
		self.mtype = mtype
		self.filename = filename
		self.value = value
		assert self.path or self.value
	
	def write(self, out):
		if self.filename:
			out.write('Content-Disposition: form-data; name="%s"; filename="%s"\r\n' % (self.name, self.filename))
		else:
			out.write('Content-Disposition: form-data; name="%s"\r\n' % (self.name))
		if self.mtype:
			out.write('Content-Type: ')
			out.write(self.mtype)
			out.write('\r\n')
		if not self.value:
			assert self.path
			self.value = open(self.path, 'rb').read()
		out.write('Content-Length: %d\r\n' % len(self.value))
		out.write('\r\n')
		out.write(self.value)
		out.write('\r\n')


def main():
	import optparse
	argp = optparse.OptionParser()
	argp.add_option('-d', '--data', '--datadir', dest='datadir', default='data')
	argp.add_option('--server', dest='server', default=None, help='url of config page on server from which to download data')
	argp.add_option('--force-config-reload', dest='force_config_reload', action='store_true', default=False)
	argp.add_option('--verbose', '-v', dest='verbose', action='store_true', default=False)
	argp.add_option('--send', dest='send', default=None, help='path to directory of results to send')
	(options, args) = argp.parse_args()
	if options.verbose:
		logging.getLogger().setLevel(logging.DEBUG)
	c = Client(options)
	if options.send:
		c.sendResultDir(options.send, {'localpath':options.send})
		return
	for arch, url in c.knownDatasets.iteritems():
		c.unpackArchive(arch, url)

if __name__ == '__main__':
	main()
