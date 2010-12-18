#!/usr/bin/python

__author__ = "Brian Olson"

"""THIS SCRIPT MAY DOWNLOAD A LOT OF DATA

Some states may have 6 Gigabytes of data to download.
Don't all swamp the Census servers at once now.

Make a directory 'data' under the build directory.

Now run this script (from the installation directory, containing data/) with
a two letter postal abbreviation for a state:
./setupstatedata.py ny
"""

# standard
import glob
import logging
import optparse
import os
import cPickle as pickle
import re
import string
import subprocess
import sys
import tarfile
import threading
import time
import traceback
import urllib
import zipfile

# local
import generaterunconfigs
import linksfromedges
import makelinks
from newerthan import newerthan
import shapefile
import solution

from states import *

sf1IndexName = 'SF1index.html'
sf1url = 'http://ftp2.census.gov/census_2000/datasets/Summary_File_1/'
tigerbase = 'http://www2.census.gov/geo/tiger/'

def cdFromUf1Line(line):
	"""Return (cd, congress number) or (-1, None)."""
	for start,congress_number in [
		(142, 110),
		(140, 109),
		(138, 108),
		(136, 106)]:
		cd = line[start:start+2]
		if cd != '  ':
			try:
				icd = int(cd)
				assert icd >= 1
				assert icd < 250
				icd -= 1
				return (icd, congress_number)
			except:
				pass
	return (-1, None)

CURSOL_RE_ = re.compile(r'(..)(\d\d\d).dsz')

def linkBestCurrentSolution(dpath):
	alldsz = glob.glob(dpath + '/*dsz')
	currentLink = None
	bestDsz = None
	bestN = None
	stu = None
	for x in alldsz:
		if x.endswith('current.dsz'):
			assert os.path.islink(x)
			currentLink = os.readlink(x)
			continue
		fname = os.path.basename(x)
		m = CURSOL_RE_.match(fname)
		assert m is not None, ('failed to parse %s' % fname)
		nstu = m.group(1).upper()
		if stu is None:
			stu = nstu
		assert nstu == stu
		nth = int(m.group(2))
		if (bestDsz is None) or (nth > bestN):
			bestN = nth
			bestDsz = fname
	if bestDsz is None:
		return None
	currentDsz = os.path.join(dpath, stu + 'current.dsz')
	if os.path.islink(currentDsz):
		odsz = os.readlink(currentDsz)
		if odsz == bestDsz:
			return bestDsz
		os.unlink(currentDsz)
	os.symlink(bestDsz, currentDsz)
	return bestDsz

basic_make_rules_ = """
-include ${dpath}/makedefaults
-include ${dpath}/makeoptions

${stu}_all:	${dpath}/${stu}_start_sm.jpg ${dpath}/${stu}_sm.mppb ${dpath}/${stu}_start_sm.png

${dpath}/${stu}_start_sm.jpg:	${dpath}/${stu}_start.png
	convert ${dpath}/${stu}_start.png -resize 150x150 ${dpath}/${stu}_start_sm.jpg

${dpath}/${stu}_start.png:	${bindir}/drend ${dpath}/${stu}.mppb ${dpath}/${stu}current.dsz
	${bindir}/drend -B ${dpath}/${stu_bin} $${${stu}DISTOPT} --mppb ${dpath}/${stu}.mppb --pngout ${dpath}/${stu}_start.png --loadSolution ${dpath}/${stu}current.dsz > ${dpath}/${stu}_start_stats

${dpath}/${stu}_start_sm.png:	${bindir}/drend ${dpath}/${stu}_sm.mppb ${dpath}/${stu}current.dsz
	${bindir}/drend -B ${dpath}/${stu_bin} $${${stu}DISTOPT} --mppb ${dpath}/${stu}_sm.mppb --pngout ${dpath}/${stu}_start_sm.png --loadSolution ${dpath}/${stu}current.dsz
"""

# don't need these, compileBinaryData does it
compileBinaryData_rules_ = """
${dpath}/${stu}.gbin:	${dpath}/.uf1 linkfixup
	./linkfixup -U ${dpath}/.uf1 -o ${dpath}/${stu}.gbin

${dpath}/${stu}.pb:	${dpath}/.uf1 linkfixup
	./linkfixup -U ${dpath}/.uf1 -p ${dpath}/${stu}.pb
"""

# we don't need these when building from shapefile
old_makepolys_rules_ = ("""
${dpath}/${stu}.mpout:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys -o ${dpath}/${stu}.mpout $${${stu}LONLAT} $${${stu}PNGSIZE} --maskout ${dpath}/${stu}mask.png ${dpath}/raw/*.RT1

${dpath}/${stu}_sm.mpout:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys -o ${dpath}/${stu}_sm.mpout $${${stu}LONLAT} $${${stu}PNGSIZE_SM} --maskout ${dpath}/${stu}mask_sm.png ${dpath}/raw/*.RT1

${dpath}/${stu}_large.mpout:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys -o ${dpath}/${stu}_large.mpout $${${stu}LONLAT} $${${stu}PNGSIZE_LARGE} --maskout ${dpath}/${stu}mask_large.png ${dpath}/raw/*.RT1

${dpath}/${stu}_huge.mpout:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys -o ${dpath}/${stu}_huge.mpout $${${stu}LONLAT} $${${stu}PNGSIZE_HUGE} --maskout ${dpath}/${stu}mask_huge.png ${dpath}/raw/*.RT1

${dpath}/${stu}.mppb:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys --protobuf -o ${dpath}/${stu}.mppb $${${stu}LONLAT} $${${stu}PNGSIZE} --maskout ${dpath}/${stu}mask.png ${dpath}/raw/*.RT1

${dpath}/${stu}_sm.mppb:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys --protobuf -o ${dpath}/${stu}_sm.mppb $${${stu}LONLAT} $${${stu}PNGSIZE_SM} --maskout ${dpath}/${stu}mask_sm.png ${dpath}/raw/*.RT1

${dpath}/${stu}_large.mppb:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys --protobuf -o ${dpath}/${stu}_large.mppb $${${stu}LONLAT} $${${stu}PNGSIZE_LARGE} --maskout ${dpath}/${stu}mask_large.png ${dpath}/raw/*.RT1

${dpath}/${stu}_huge.mppb:	tiger/makepolys ${dpath}/raw/*.RT1
	time ./tiger/makepolys --protobuf -o ${dpath}/${stu}_huge.mppb $${${stu}LONLAT} $${${stu}PNGSIZE_HUGE} --maskout ${dpath}/${stu}mask_huge.png ${dpath}/raw/*.RT1
""")

makefile_fragment_template = string.Template(basic_make_rules_)

class ProcessGlobals(object):
	def __init__(self, options):
		self.options = options
		self.geourls = None
		self.tigerlatest = None
		self.bestYear = None
		self.bestYearEdition = None

	def getSF1Index(self):
		"""Returns whole sf1 index html file in one buffer."""
		sf1ipath = os.path.join(self.options.datadir, sf1IndexName)
		if not os.path.isfile(sf1ipath):
			if self.options.dryrun:
				logging.info('should fetch "%s"', sf1url)
				return ''
			uf = urllib.urlopen(sf1url)
			sf1data = uf.read()
			uf.close()
			fo = open(sf1ipath, 'w')
			fo.write(sf1data)
			fo.close()
		else:
			uf = open(sf1ipath, 'r')
			sf1data = uf.read()
			uf.close()
		return sf1data
	
	def getGeoUrls(self):
		"""Returns list of urls to ..geo_uf1.zip files"""
		if self.geourls is not None:
			return self.geourls
		geopath = os.path.join(self.options.datadir, 'geo_urls')
		if os.path.isfile(geopath):
			guf = open(geopath, 'r')
			raw = guf.read()
			guf.close()
			self.geourls = raw.splitlines()
			return self.geourls
		else:
			sf1index = self.getSF1Index()
			subdirpat = re.compile(r'href="([A-Z][^"]+)"')
			geouf1pat = re.compile(r'href="(..geo_uf1.zip)"')
			geo_urls = []
			for line in sf1index.splitlines():
				m = subdirpat.search(line)
				if m:
					subdir = m.group(1)
					surl = sf1url + subdir
					logging.info('surl=%s', surl)
					uf = urllib.urlopen(surl)
					ud = uf.read()
					uf.close()
					gum = geouf1pat.search(ud)
					if gum:
						gu = surl + gum.group(1)
						logging.info('gu=%s', gu)
						geo_urls.append(gu)
			if self.options.dryrun:
				logging.info('would write "%s"', geopath)
			else:
				fo = open(geopath, 'w')
				for gu in geo_urls:
					fo.write(gu + '\n')
				fo.close()
			self.geourls = geo_urls
			return geo_urls
	
	def getGeoUrl(self, stl):
		"""Return the url to xxgeo_uf1.zip for state xx."""
		geourls = self.getGeoUrls()
		uf1zip = stl + 'geo_uf1.zip'
		for x in geourls:
			if uf1zip in x:
				return x
		return None
	
	def getTigerLatestShapefileEdition(self, raw):
		"""New shapefile data lives in directories of this pattern.
		ex: http://www2.census.gov/geo/tiger/TIGER2009/
		"""
#TODO: 2010 data has a new layout. Write new code to crawl it.
#http://www2.census.gov/geo/tiger/TIGER2010/TABBLOCK/2010/tl_2010_34_tabblock10.zip
#http://www2.census.gov/geo/tiger/TIGER2010/FACES/tl_2010_22001_faces.zip
#http://www2.census.gov/geo/tiger/TIGER2010/EDGES/tl_2010_22001_edges.zip
#http://www2.census.gov/geo/tiger/TIGER2010/COUNTY/2010/tl_2010_22_county10.zip

		bestyear = None
		editions = re.compile(r'href="TIGER(\d\d\d\d)/')
		for m in editions.finditer(raw):
			year = int(m.group(1))
			if ((bestyear is None) or (year > bestyear)) and (year <= 2009):
				bestyear = year
		if bestyear is None:
			raise Exception('found no tiger editions at "%s"' % tigerbase)
		self.bestYear = bestyear
		return '%sTIGER%04d/' % (tigerbase, bestyear)

	def getTigerLatestLineEdition(self, raw):
		"""Older census-special ascii line-file releases have this pattern."""
		editions = re.compile(r'href="tiger(\d\d\d\d)(.)e/')
		bestyear = None
		bested = None
		edmap = { 'f': 1, 's': 2, 't': 3 }
		for m in editions.finditer(raw):
			year = int(m.group(1))
			ed = m.group(2)
			if (bestyear is None) or (year > bestyear):
				bestyear = year
				bested = ed
			elif (year == bestyear) and (edmap[ed] > edmap[bested]):
				bested = ed
		if bestyear is None:
			raise Exception('found no tiger editions at "%s"' % tigerbase)
		self.bestYear = bestyear
		self.bestYearEdition = bested
		# reconstruct absolute url to edition
		return '%stiger%s%se/' % (tigerbase, bestyear, bested)
	
	def getTigerLatest(self):
		"""For either shapefile or old line-file, return latest base URL."""
		if self.tigerlatest is not None:
			return self.tigerlatest
		if self.options.dryrun:
			logging.info('would fetch "%s"', tigerbase)
			raw = ''
		else:
			uf = urllib.urlopen(tigerbase)
			raw = uf.read()
			uf.close()
		if self.options.shapefile:
			self.tigerlatest = self.getTigerLatestShapefileEdition(raw)
		else:
			self.tigerlatest = self.getTigerLatestLineEdition(raw)
		return self.tigerlatest


COUNTY_RE = re.compile(r'([0-9]{5})_(.*)')


def filterMinSize(seq, minSize=100):
	out = []
	for x in seq:
		if os.path.getsize(x) > minSize:
			out.append(x)
	return out


def checkZipFile(path):
	try:
		zf = zipfile.ZipFile(path, 'r')
		nl = zf.namelist()
		zf.close()
		return bool(nl)
	except zipfile.BadZipfile, e:
		return False


class StateData(object):
	def __init__(self, globals, st, options):
		self.pg = globals
		self.stl = st.lower()
		self.stu = st.upper()
		self.fips = fipsForPostalCode(self.stu)
		self.bestYear = None
		self.turl = None
		self.turl_time = None
		self.ziplist = None
		self.geom = None
		self.options = options
		self.dpath = os.path.join(self.options.datadir, self.stu)
		self.setuplog = open(os.path.join(self.dpath, 'setuplog'), 'a')
		self._zipspath = os.path.join(self.dpath, 'zips')
	
	def logf(self, fmt, *args):
		if args:
			msg = (fmt % args) + '\n'
		else:
			msg = fmt + '\n'
		sys.stdout.write(self.stu + ' ' + msg)
		self.setuplog.write(msg)
	
	def mkdir(self, path, options):
		if not os.path.isdir(path):
			if options.dryrun:
				self.logf('mkdir %s', path)
			else:
				os.mkdir(path)
	
	def maybeUrlRetrieve(self, url, localpath, contenttype=None):
		if os.path.exists(localpath):
			return localpath
		if self.options.dryrun:
			self.logf('would fetch "%s" -> "%s"', url, localpath)
			return localpath
		logging.info('fetch "%s" -> "%s"', url, localpath)
		(filename, info) = urllib.urlretrieve(url, localpath)
		logging.debug('%s info: %s', url, info)
		if (contenttype is not None) and (info['content-type'] != contenttype):
			logging.error('%s came back with wrong content-type %s, wanted %s. if content is OK:\nmv %s_bad %s\nOR skip with:touch %s\n',
				url, info['content-type'], contenttype, localpath, localpath, localpath)
			os.rename(localpath, localpath + '_bad')
			#os.unlink(filename)
			if self.options.strict:
				raise Exception('download failed: ' + url + ' if this is OK, touch ' + localpath)
			return None
		return localpath
	
	def makeUf101(self, geozip, uf101, dpath=None):
		"""From xxgeo_uf1.zip, distill the 101 (block level) geopgraphic
		summary file xx101.uf1 ."""
		if dpath is None:
			dpath = self.dpath
		self.maybeUrlRetrieve(self.pg.getGeoUrl(self.stl), geozip)
		fo = None
		cds = None
		if newerthan(geozip, uf101):
			self.logf('%s -> %s', geozip, uf101)
			if not self.options.dryrun:
				fo = open(uf101, 'w')
		currentDsz = os.path.join(dpath, self.stu + 'current.dsz')
		if newerthan(geozip, currentDsz):
			self.logf('%s -> %s', geozip, currentDsz)
			if not self.options.dryrun:
				cds = []
		if (fo is None) and (cds is None):
			# nothing to do
			return
		zf = zipfile.ZipFile(geozip, 'r')
		raw = zf.read(self.stl + 'geo.uf1')
		zf.close()
		filter = 'uSF1  ' + self.stu + '101'
		congress_number = None  # enforce consistency
		for line in raw.splitlines(True):
			if line.startswith(filter):
				if fo is not None:
					fo.write(line)
				if cds is not None:
					cd, cong_num = cdFromUf1Line(line)
					if congress_number is None:
						congress_number = cong_num
					if cong_num is not None:
						assert cong_num == congress_number
					cds.append(cd)
		if fo is not None:
			fo.close()
		if congress_number is None:
			# not one line had a valid cd,congress_number; bail
			return
		if cds is not None:
			cdvals = {}
			for x in cds:
				if x != -1:
					cdvals[x] = 1
			self.logf(
				'found congressional districts from %dth congress: %s',
				congress_number, repr(cdvals.keys()))
			cnDsz = os.path.join(dpath, '%s%d.dsz' % (self.stu, congress_number))
			currentSolution = open(cnDsz, 'wb')
			currentSolution.write(solution.makeDsz(cds))
			currentSolution.close()
			linkBestCurrentSolution(dpath)
			makedefaults = open(os.path.join(dpath, 'makedefaults'), 'w')
			makedefaults.write('%sDISTOPT ?= -d %d\n' % (
				self.stu, len(cdvals)))
			makedefaults.close()
	
	def getTigerBase(self, dpath=None):
		"""Determine the base URL for shapefile data for this state.
		Cached in 'data/XX/tigerurl'.
		example url:
		http://www2.census.gov/geo/tiger/TIGER2009/25_MASSACHUSETTS/
		"""
		if self.turl is not None:
			return self.turl
		if dpath is None:
			dpath = self.dpath
		turlpath = os.path.join(dpath, 'tigerurl')
		if not os.path.isfile(turlpath):
			tigerlatest = self.pg.getTigerLatest()
			self.bestYear = self.pg.bestYear
			if self.options.shapefile:
				turl = '%s%02d_%s/' % (
					tigerlatest, self.fips,
					nameForPostalCode(self.stu).upper().replace(' ', '_'))
			else:
				turl = tigerlatest + self.stu + '/'
			self.logf('guessing tiger data source "%s", if this is wrong, edit "%s"', turl, turlpath)
			if self.options.dryrun:
				return turl
			self.turl_time = time.time()
			fo = open(turlpath, 'w')
			fo.write(turl)
			fo.write('\n')
			fo.close()
			self.turl = turl
			return turl
		else:
			fi = open(turlpath, 'r')
			turl = fi.read()
			fi.close()
			m = re.search(r'tiger(\d\d\d\d)', turl, re.IGNORECASE)
			self.bestYear = int(m.group(1))
			self.turl = turl.strip()
			return self.turl
	
	def getTigerZipIndexHtml(self, dpath):
		"""Return raw html of zip index listing. (maybe cached)"""
		zipspath = self.zipspath(dpath)
		indexpath = os.path.join(zipspath, 'index.html')
		if not os.path.isfile(indexpath):
			self.mkdir(zipspath, self.options)
			turl = self.getTigerBase(dpath)
			uf = urllib.urlopen(turl)
			raw = uf.read()
			uf.close()
			if self.options.dryrun:
				self.logf('would write "%s"', indexpath)
			else:
				of = open(indexpath, 'w')
				of.write(raw)
				of.close()
		else:
			f = open(indexpath, 'r')
			raw = f.read()
			f.close()
		return raw
	
	def getTigerZipIndex(self, dpath):
		"""Return list of basic zip files to download."""
		if self.ziplist is not None:
			return self.ziplist
		raw = self.getTigerZipIndexHtml(dpath)
		self.ziplist = []
		if self.options.shapefile:
			# TODO: in the future, get tabblock only but not tabblock00
			for m in re.finditer(r'href="([^"]*tabblock00[^"]*.zip)"', raw, re.IGNORECASE):
				self.ziplist.append(m.group(1))
		else:
			for m in re.finditer(r'href="([^"]+.zip)"', raw, re.IGNORECASE):
				self.ziplist.append(m.group(1))
		return self.ziplist
	
	def getCountyPaths(self):
		"""Return list of relative href values for county dirs."""
		raw = self.getTigerZipIndexHtml(self.dpath)
		# NV, VA has some city regions not part of county datasets
		# AK has "Borough", "Census_Area", "Municipality"
		# LA has "Parish"
		re_string = 'href="(%02d\\d\\d\\d_[^"]+(?:County|city|Municipality|Census_Area|Borough|Parish)/?)"' % (fipsForPostalCode(self.stu))
		return re.findall(re_string, raw, re.IGNORECASE)
	
	def goodZip(self, localpath, url):
		if not os.path.exists(localpath):
			return False
		if os.path.getsize(localpath) == 0:
			# empty files are for skipping
			return True
		ok = checkZipFile(localpath)
		if not ok:
			badpath = localpath + '_bad'
			if os.path.exists(badpath):
				os.unlink(badpath)
			os.rename(localpath, badpath)
			self.logf('bad zip file "%s" moved aside to "%s". (from url %s) to skip: ` rm "%s" && touch "%s" `', localpath, badpath, url, badpath, localpath)
		return ok
	
	def getEdges(self):
		# http://www2.census.gov/geo/tiger/TIGER2009/25_MASSACHUSETTS/25027_Worcester_County/tl_2009_25027_edges.zip
		counties = self.getCountyPaths()
		base = self.getTigerBase()
		for co in counties:
			m = COUNTY_RE.match(co)
			filename = 'tl_%4d_%s_edges.zip' % (self.bestYear, m.group(1))
			localpath = os.path.join(self.zipspath(), filename)
			url = base + co + filename
			self.maybeUrlRetrieve(url, localpath, 'application/zip')
			ok = self.goodZip(localpath, url)
			if not ok:
				return False
		return True
	
	def getFaces(self):
		# http://www2.census.gov/geo/tiger/TIGER2009/25_MASSACHUSETTS/25027_Worcester_County/tl_2009_25027_faces.zip
		counties = self.getCountyPaths()
		base = self.getTigerBase()
		for co in counties:
			m = COUNTY_RE.match(co)
			filename = 'tl_%4d_%s_faces.zip' % (self.bestYear, m.group(1))
			localpath = os.path.join(self.zipspath(), filename)
			url = base + co + filename
			self.maybeUrlRetrieve(url, localpath, 'application/zip')
			ok = self.goodZip(localpath, url)
			if not ok:
				return False
		return True
	
	def zipspath(self, dpath=None):
		if dpath is None:
			return self._zipspath
		return os.path.join(dpath, 'zips')
	
	def downloadTigerZips(self, dpath):
		"""Download basic data as needed to XX/zips/"""
		turl = self.getTigerBase(dpath)
		ziplist = self.getTigerZipIndex(dpath)
		zipspath = self.zipspath(dpath)
		for z in ziplist:
			zp = os.path.join(zipspath, z)
			self.maybeUrlRetrieve(turl + z, zp)
		return ziplist
	
	def processShapefile(self, dpath):
		"""Build .links and .mppb rasterization from shapefile."""
		bestzip = None
		ziplist = self.downloadTigerZips(dpath)
		for zname in ziplist:
			if shapefile.betterShapefileZip(zname, bestzip):
				bestzip = zname
		if bestzip is None:
			self.logf('found no best zipfile to use')
			return None
		zipspath = self.zipspath(dpath)
		assert zipspath is not None
		bestzip = os.path.join(zipspath, bestzip)
		facesPaths = glob.glob(os.path.join(zipspath, '*faces*zip'))
		edgesPaths = glob.glob(os.path.join(zipspath, '*edges*zip'))
		facesPaths = filterMinSize(facesPaths, 100)
		edgesPaths = filterMinSize(edgesPaths, 100)
		linksname = os.path.join(dpath, self.stl + '101.uf1.links')
		mppb_name = os.path.join(dpath, self.stu + '.mppb')
		mask_name = os.path.join(dpath, self.stu + 'mask.png')
		mppbsm_name = os.path.join(dpath, self.stu + '_sm.mppb')
		masksm_name = os.path.join(dpath, self.stu + 'mask_sm.png')
		linksargs = None
		renderArgs = None
		commands = []
		if not os.path.exists(linksname):
			self.logf('need %s', linksname)
			if edgesPaths and facesPaths:
				lecmd = linksfromedges.makeCommand(facesPaths + edgesPaths + ['--links', linksname], self.options.bindir, self.options.strict)
				commands.append(lecmd)
			else:
				linksargs = ['--links', linksname]
		if not os.path.exists(mppb_name):
			self.logf('need %s', mppb_name)
			renderArgs = ['--rast', mppb_name, '--mask', mask_name,
				'--boundx', '1920', '--boundy', '1080',
				'--rastgeom', os.path.join(dpath, 'rastgeom')]
		if renderArgs:
			if linksargs:
				if not facesPaths:
					command = shapefile.makeCommand(
						linksargs + renderArgs + [bestzip], self.options.bindir, self.options.strict)
					commands.append(command)
				else:
					command = shapefile.makeCommand(
						linksargs + [bestzip], self.options.bindir, self.options.strict)
					commands.append(command)
					command = shapefile.makeCommand(
						renderArgs + facesPaths, self.options.bindir, self.options.strict)
					commands.append(command)
			else:
				if facesPaths:
					command = shapefile.makeCommand(
						renderArgs + facesPaths, self.options.bindir, self.options.strict)
					commands.append(command)
				else:
					command = shapefile.makeCommand(
						renderArgs + [bestzip], self.options.bindir, self.options.strict)
					commands.append(command)
		elif linksargs:
				command = shapefile.makeCommand(
					linksargs + [bestzip], self.options.bindir, self.options.strict)
				commands.append(command)
		if not os.path.exists(mppbsm_name):
			self.logf('need %s', mppbsm_name)
			smargs = ['--rast', mppbsm_name, '--mask', masksm_name,
				'--boundx', '640', '--boundy', '480']
			if facesPaths:
				smargs = smargs + facesPaths
			else:
				smargs.append(bestzip)
			command = shapefile.makeCommand(smargs,
				self.options.bindir, self.options.strict)
			commands.append(command)
		for command in commands:
			self.logf('command: %s', ' '.join(command))
			if not self.options.dryrun:
				status = subprocess.call(command, shell=False, stdin=None)
				if status != 0:
					raise Exception('error (%d) executing: "%s"' % (status, ' '.join(command)))
		return linksname
	
	def makelinks(self, dpath):
		"""Deprecated. Use shapefile bundle."""
		if self.options.shapefile:
			return self.processShapefile(dpath)
		logging.warning('Deprecated. Use shapefile bundle.')
		linkspath = os.path.join(dpath, self.stl + '101.uf1.links')
		zipspath = self.zipspath(dpath)
		rawpath = os.path.join(dpath, 'raw')
		ziplist = self.downloadTigerZips(dpath)
		needlinks = False
		if not os.path.isfile(linkspath):
			self.logf('no %s', linkspath)
			needlinks = True
		if (not needlinks) and newerthan(makelinks.__file__, linkspath):
			self.logf('%s > %s', makelinks.__file__, linkspath)
			needlinks = True
		if not needlinks:
			for z in ziplist:
				zp = os.path.join(zipspath, z)
				if newerthan(zp, linkspath):
					self.logf('%s > %s', zp, linkspath)
					needlinks = True
					break
		if needlinks:
			self.logf('%s/{%s} -> %s', zipspath, ','.join(ziplist), linkspath)
			if self.options.dryrun:
				return linkspath
			start = time.time()
			linker = makelinks.linker()
			for z in ziplist:
				zp = os.path.join(zipspath, z)
				linker.processZipFilename(zp)
			f = open(linkspath, 'wb')
			linker.writeText(f)
			f.close()
			self.logf('makelinks took %f seconds', time.time() - start)
		return linkspath
	
	def compileBinaryData(self, dpath):
		uf1path = os.path.join(dpath, self.stl + '101.uf1')
		linkspath = uf1path + '.links'
		outpath = None
		binpath = os.path.join(self.options.bindir, 'linkfixup')
		cmd = [binpath, '-U', self.stl + '101.uf1']
		if self.options.protobuf:
			cmd.append('-p')
			outpath = self.stl + '.pb'
			cmd.append(outpath)
		else:
			cmd.append('-o')
			outpath = self.stl + '.gbin'
			cmd.append(outpath)
		outpath = os.path.join(dpath, outpath)
		needsbuild = not os.path.isfile(outpath)
		if (not needsbuild) and newerthan(uf1path, outpath):
			self.logf('%s > %s', uf1path, outpath)
			needsbuild = True
		if (not needsbuild) and newerthan(linkspath, outpath):
			self.logf('%s > %s', linkspath, outpath)
			needsbuild = True
		if (not needsbuild) and newerthan(binpath, outpath):
			self.logf('%s > %s', binpath, outpath)
			needsbuild = True
		if not needsbuild:
			return
#		if not (newerthan(uf1path, outpath) or newerthan(linkspath, outpath)):
#			return
		self.logf('cd %s && "%s"', dpath, '" "'.join(cmd))
		if self.options.dryrun:
			return
		start = time.time()
		status = subprocess.call(cmd, cwd=dpath)
		self.logf('data compile took %f seconds', time.time() - start)
		if status != 0:
			raise Exception('error (%d) executing: cd %s && "%s"' % (status, dpath,'" "'.join(cmd)))
	
	def writeMakeFragment(self, dpath=None):
		if dpath is None:
			dpath = self.dpath
		mfpath = os.path.join(dpath, '.make')
		if not newerthan(__file__, mfpath):
			return mfpath
		self.logf('-> %s', mfpath)
		if self.options.protobuf:
			stl_bin = self.stl + '.pb'
		else:
			stl_bin = self.stl + '.gbin'
		if self.options.dryrun:
			self.logf('would write "%s"', mfpath)
			return mfpath
		out = open(mfpath, 'w')
		out.write(makefile_fragment_template.substitute({
			'dpath': dpath,
			'stu': self.stu,
			'stl': self.stl,
			'stu_bin': stl_bin,
			'bindir': self.options.bindir}))
		out.close()
		return mfpath
	
	def getextras(self, extras=None):
		"""Get extra data like 00001 series data."""
		if extras is None:
			extras = self.options.extras
		geourl = self.pg.getGeoUrl(self.stl)
		zipspath = self.zipspath(self.dpath)
		self.mkdir(zipspath, self.options)
		for x in extras:
			xurl = geourl.replace('geo_uf1', x + '_uf1')
			xpath = os.path.join(zipspath, self.stl + x + '_uf1.zip')
			self.maybeUrlRetrieve(xurl, xpath)
	
	def acceptArchivePart(self, dirpath, fname):
		flower = fname.lower()
		for oksuffix in ['.pb', '.mppb', '.png', '.jpg', '_stats', '.html']:
			if flower.endswith(oksuffix):
				return True
		if dirpath.endswith('config'):
			return True
		return False
	
	def archiveRunfiles(self):
		"""Bundle key data as XX_runfiles.tar.gz"""
		if ((not os.path.isdir(self.options.archive_runfiles)) or 
			(not os.access(self.options.archive_runfiles, os.X_OK|os.W_OK))):
			sys.stderr.write('error: "%s" is not a writable directory\n' % self.options.archive_runfiles)
			return None
		destpath = os.path.join(self.options.archive_runfiles, self.stu + '_runfiles.tar.gz')
		partpaths = []
		needsupdate = False
		dpath = os.path.join(self.options.datadir, self.stu)
		for (dirpath, dirnames, filenames) in os.walk(dpath):
			partpaths.append(dirpath)
			if 'zips' in dirnames:
				dirnames.remove('zips')
			for fname in filenames:
				if self.acceptArchivePart(dirpath, fname):
					fpath = os.path.join(dirpath, fname)
					if (not needsupdate) and newerthan(fpath, destpath):
						needsupdate = True
					partpaths.append(fpath)
		if not needsupdate:
			return destpath
		out = tarfile.open(destpath, 'w|gz')
		out.posix = True
		for part in partpaths:
			arcname = part
			if part.startswith(self.options.datadir):
				arcname = part[len(self.options.datadir):]
				while arcname[0] == '/':
					arcname = arcname[1:]
			out.add(part, arcname, False)
		out.close()
		return destpath
	
	def clean(self):
		for (dirpath, dirnames, filenames) in os.walk(self.dpath):
			for fname in filenames:
				fpath = os.path.join(dirpath, fname)
				if fname.lower().endswith('.zip'):
					logging.debug('not cleaning "%s"', fpath)
					continue
				if self.options.dryrun or self.options.verbose:
					self.logf('rm %s', fpath)
				if not self.options.dryrun:
					os.remove(fpath)
	
	def dostate(self):
		start = time.time()
		self.logf('start at %s\n', time.ctime(start))
		ok = False
		try:
			ok = self.dostate_inner()
		except:
			errmsg = traceback.format_exc() + ('\n%s error running %s\n' % (self.stu, self.stu))
			self.logf(errmsg)
		self.logf('ok=%s after %f seconds\n', ok, time.time() - start)
		self.setuplog.flush()
		sys.stdout.flush()
	
	def dostate_inner(self):
		if self.options.clean:
			self.clean()
			return True
		self.mkdir(self.dpath, self.options)
		if self.options.extras:
			self.getextras()
			if self.options.extras_only:
				return True
		geozip = os.path.join(self.dpath, self.stl + 'geo_uf1.zip')
		uf101 = os.path.join(self.dpath, self.stl + '101.uf1')
		self.makeUf101(geozip, uf101)
		if self.options.getFaces:
			ok = self.getFaces()
			if not ok:
				return False
		if self.options.getEdges:
			ok = self.getEdges()
			if not ok:
				return False
		linkspath = self.makelinks(self.dpath)
		if not linkspath:
			self.logf('makelinks failed')
			return False
		self.compileBinaryData(self.dpath)
		handargspath = os.path.join(self.dpath, 'handargs')
		if not os.path.isfile(handargspath):
			if self.options.dryrun:
				self.logf('would write "%s"', handargspath)
			else:
				ha = open(handargspath, 'w')
				ha.write('-g 10000\n')
				ha.close
		makefile = self.writeMakeFragment()
		generaterunconfigs.run(
			datadir=self.options.datadir,
			stulist=[self.stu],
			dryrun=self.options.dryrun)
		makecmd = ['make', '-k', self.stu + '_all', '-f', makefile]
		if self.options.dryrun:
			self.logf('would run "%s"', ' '.join(makecmd))
		else:
			start = time.time()
			status = subprocess.call(makecmd)
			if status != 0:
				sys.stderr.write(
					'command "%s" failed with %d\n' % (' '.join(makecmd), status))
			self.logf('final make took %f seconds', time.time() - start)
		if self.options.archive_runfiles:
			start = time.time()
			outname = self.archiveRunfiles()
			self.logf('wrote "%s" in %f seconds', outname, (time.time() - start))
		return True


def getOptions():
	default_bindir = os.environ.get('REDISTRICTER_BIN')
	if default_bindir is None:
		default_bindir = os.path.dirname(os.path.abspath(__file__))
	default_datadir = os.environ.get('REDISTRICTER_DATA')
	if default_datadir is None:
		default_datadir = os.path.join(default_bindir, 'data')
	argp = optparse.OptionParser()
# commented out options aren't actually used.
#	argp.add_option('-m', '--make', action='store_true', dest='domaake', default=False)
#	argp.add_option('--nopng', '--without_png', dest='png', action='store_false', default=True)
#	argp.add_option('--unpackall', action='store_true', dest='unpackall', default=False)
	argp.add_option('-n', '--dry-run', action='store_true', dest='dryrun', default=False)
	argp.add_option('--gbin', action='store_false', dest='protobuf', default=True)
	argp.add_option('-d', '--data', dest='datadir', default=default_datadir)
	argp.add_option('--bindir', dest='bindir', default=default_bindir)
	argp.add_option('--getextra', dest='extras', action='append', default=[])
	argp.add_option('--extras_only', dest='extras_only', action='store_true', default=False)
	argp.add_option('--faces', dest='getFaces', action='store_true', default=False, help='fetch and process detailed faces shapefile data.')
	argp.add_option('--edges', dest='getEdges', action='store_true', default=False, help='fetch and process detailed edges shapefile data.')
	argp.add_option('--shapefile', dest='shapefile', action='store_true', default=True)
	argp.add_option('--noshapefile', dest='shapefile', action='store_false')
	argp.add_option('--clean', dest='clean', action='store_true', default=False)
	argp.add_option('--verbose', dest='verbose', action='store_true', default=False)
	argp.add_option('--strict', dest='strict', action='store_true', default=False)
	argp.add_option('--archive-runfiles', dest='archive_runfiles', default=None, help='directory path to store tar archives of run file sets into')
	argp.add_option('--datasets', dest='archive_runfiles', help='directory path to store tar archives of run file sets into')
	argp.add_option('--threads', dest='threads', type='int', default=1, help='number of threads to run')
	return argp.parse_args()


class SyncrhonizedIteratorWrapper(object):
	def __init__(self, it):
		self.it = it.__iter__()
		self.lock = threading.Lock()
	
	def __iter__(self):
		return self
	
	def next(self):
		self.lock.acquire()
		try:
			out = self.it.next()
		except:
			self.lock.release()
			raise
		self.lock.release()
		return out


def runloop(states, globals, options):
	for a in states:
		start = time.time()
		try:
			sd = StateData(globals, a, options)
			sd.dostate()
		except:
			traceback.print_exc()
			errmsg = traceback.format_exc() + ('\n%s error running %s\n' % (a, a))
			sys.stdout.write(errmsg)
			sd.logf(errmsg)
		sys.stdout.write('%s took %f seconds\n' % (a, time.time() - start))
		sys.stdout.flush()


def main(argv):
	(options, args) = getOptions()
	if options.verbose:
		logging.getLogger().setLevel(logging.DEBUG)
	if not os.path.isdir(options.datadir):
		raise Exception('data dir "%s" does not exist' % options.datadir)
	
	if not options.shapefile:
		makefile_fragment_template = string.Template(
			basic_make_rules_ + old_makepolys_rules_)
	pg = ProcessGlobals(options)
	
	if options.threads == 1:
		runloop(args, pg, options)
	else:
		tlist = []
		statelist = SyncrhonizedIteratorWrapper(args)
		for x in xrange(0, options.threads):
			threadLabel = 't%d' % x
			tlist.append(threading.Thread(target=runloop, args=(statelist, pg, options), name=threadLabel))
		for x in tlist:
			x.start()
		for x in tlist:
			x.join()

if __name__ == '__main__':
	main(sys.argv)
