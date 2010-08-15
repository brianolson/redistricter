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

# TODO: write a --clean mode to delete all generated files.
# Downloaded data may remain.

import glob
import logging
import optparse
import os
import cPickle as pickle
import re
import string
import subprocess
import sys
import time
import urllib
import zipfile

import generaterunconfigs
import measureGeometry
import makelinks
import shapefile
import solution

from states import *

sf1IndexName = 'SF1index.html'
sf1url = 'http://ftp2.census.gov/census_2000/datasets/Summary_File_1/'
tigerbase = 'http://www2.census.gov/geo/tiger/'

def newerthan(a, b):
	"""Return true if a is newer than b, or a exists and b doesn't."""
	try:
		sa = os.stat(a)
	except:
		return False
	try:
		sb = os.stat(b)
	except:
		return True
	return sa.st_mtime > sb.st_mtime

def mkdir(path, options):
	if not os.path.isdir(path):
		if options.dryrun:
			print 'mkdir ' + path
		else:
			os.mkdir(path)

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
	os.symlink(bestDsz, os.path.join(dpath, stu + 'current.dsz'))
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
		self.tigerStateDirUrls = None

	def getSF1Index(self):
		"""Returns whole sf1 index html file in one buffer."""
		sf1ipath = os.path.join(self.options.datadir, sf1IndexName)
		if not os.path.isfile(sf1ipath):
			if self.options.dryrun:
				print 'should fetch "%s"' % (sf1url)
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
					print surl
					uf = urllib.urlopen(surl)
					ud = uf.read()
					uf.close()
					gum = geouf1pat.search(ud)
					if gum:
						gu = surl + gum.group(1)
						print gu
						geo_urls.append(gu)
			if self.options.dryrun:
				print 'would write "%s"' % (geopath)
			else:
				fo = open(geopath, 'w')
				for gu in geo_urls:
					fo.write(gu + '\n')
				fo.close()
			self.geourls = geo_urls
			return geo_urls
	
	def getGeoUrl(self, stl):
		geourls = self.getGeoUrls()
		uf1zip = stl + 'geo_uf1.zip'
		for x in geourls:
			if uf1zip in x:
				return x
		return None
	
	def getTigerLatestShapefileEdition(self, raw):
		bestyear = None
		editions = re.compile(r'href="TIGER(\d\d\d\d)/')
		for m in editions.finditer(raw):
			year = int(m.group(1))
			if (bestyear is None) or (year > bestyear):
				bestyear = year
		if bestyear is None:
			raise Error('found no tiger editions at "%s"' % tigerbase)
		return '%sTIGER%04d/' % (tigerbase, bestyear)

	def getTigerLatestLineEdition(self, raw):
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
			raise Error('found no tiger editions at "%s"' % tigerbase)
		# reconstruct absolute url to edition
		return '%stiger%s%se/' % (tigerbase, bestyear, bested)
	
	def getTigerLatest(self):
		if self.tigerlatest is not None:
			return self.tigerlatest
		if self.options.dryrun:
			print 'would fetch "%s"' % (tigerbase)
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
	
	def getTigerShapefileDirUrls(self, fileOb=None, path=None, raw=None):
		if raw is None:
			if fileOb is not None:
				raw = fileOb.read()
			elif path is not None:
				fileOb = open(path, 'rb')
				raw = fileOb.read()
				fileOb.close()
		# dirUrl = 'http://www2.census.gov/geo/tiger/TIGER2009/'
		dirUrl = self.getTigerLatest()
		tigerRelease = re.search('TIGER\d\d\d\d', dirUrl).group()
		rawCachePath = os.path.join(self.options.datadir, tigerRelease)
		if raw is None:
			if self.tigerStateDirUrls is not None:
				return self.tigerStateDirUrls
			if os.path.isfile(rawCachePath):
				fileOb = open(rawCachePath, 'rb')
				raw = fileOb.read()
				fileOb.close()
		if raw is None:
			if self.options.dryrun:
				print 'would fetch "%s" -> %s' % (self.getTigerLatest(), rawCachePath)
				raw = ''
			else:
				uf = urllib.urlopen(self.getTigerLatest())
				raw = uf.read()
				uf.close()
				of = open(rawCachePath, 'wb')
				of.write(raw)
				of.close()
		assert raw is not None
		stateHrefRe = re.compile(r'href=\"(\d\d_[A-Z_/]+)\"', re.MULTILINE)
		ms = stateHrefRe.findall(raw)
		self.tigerStateDirUrls = {}
		for state in self.states:
			ucstate = state[0].upper().replace(' ','_')
			for path in ms:
				if ucstate in path:
					self.tigerStateDirUrls[state[1]] = dirUrl + path
		return self.tigerStateDirUrls


class StateData(object):
	def __init__(self, globals, st, options):
		self.pg = globals
		self.stl = st.lower()
		self.stu = st.upper()
		self.turl = None
		self.turl_time = None
		self.ziplist = None
		self.geom = None
		self.options = options
	
	def makeUf101(self, geozip, uf101, dpath):
		if not os.path.isfile(geozip):
			gurl = self.pg.getGeoUrl(self.stl)
			print 'fetching %s -> %s' % (gurl, geozip)
			if not self.options.dryrun:
				urllib.urlretrieve(gurl, geozip)
		fo = None
		cds = None
		if newerthan(geozip, uf101):
			print geozip + ' -> ' + uf101
			if not self.options.dryrun:
				fo = open(uf101, 'w')
		currentDsz = os.path.join(dpath, self.stu + 'current.dsz')
		if newerthan(geozip, currentDsz):
			print geozip + ' -> ' + currentDsz
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
		if cds is not None:
			cdvals = {}
			for x in cds:
				if x != -1:
					cdvals[x] = 1
			print 'found congressional districts from %dth congress: %s' % (
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
	
	def getTigerBase(self, dpath):
		if self.turl is not None:
			return self.turl
		turlpath = os.path.join(dpath, 'tigerurl')
		if not os.path.isfile(turlpath):
			tigerlatest = self.pg.getTigerLatest()
			if self.options.shapefile:
				turl = '%s%02d_%s/' % (
					tigerlatest, fipsForPostalCode(self.stu),
					nameForPostalCode(self.stu).upper().replace(' ', '_'))
			else:
				turl = tigerlatest + self.stu + '/'
			print 'guessing tiger data source "%s", if this is wrong, edit "%s"' % (turl, turlpath)
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
			self.turl = turl.strip()
			return self.turl
	
	def getTigerZipIndex(self, dpath):
		if self.ziplist is not None:
			return self.ziplist
		zipspath = self.zipspath(dpath)
		indexpath = os.path.join(zipspath, 'index.html')
		if not os.path.isfile(indexpath):
			mkdir(zipspath, self.options)
			turl = self.getTigerBase(dpath)
			uf = urllib.urlopen(turl)
			raw = uf.read()
			uf.close()
			if self.options.dryrun:
				print 'would write "%s"' % (indexpath)
			else:
				of = open(indexpath, 'w')
				of.write(raw)
				of.close()
		else:
			f = open(indexpath, 'r')
			raw = f.read()
			f.close()
		self.ziplist = []
		if self.options.shapefile:
			# TODO: in the future, get tabblock only but not tabblock00
			for m in re.finditer(r'href="([^"]*tabblock00[^"]*.zip)"', raw, re.IGNORECASE):
				self.ziplist.append(m.group(1))
		else:
			for m in re.finditer(r'href="([^"]+.zip)"', raw, re.IGNORECASE):
				self.ziplist.append(m.group(1))
		return self.ziplist
	
	def zipspath(self, dpath):
		return os.path.join(dpath, 'zips')
	
	def downloadTigerZips(self, dpath):
		turl = self.getTigerBase(dpath)
		ziplist = self.getTigerZipIndex(dpath)
		zipspath = self.zipspath(dpath)
		for z in ziplist:
			zp = os.path.join(zipspath, z)
			if not os.path.isfile(zp):
				print 'fetching: ' + turl + z
				if not self.options.dryrun:
					urllib.urlretrieve(turl + z, zp)
		return ziplist
	
	def processShapefile(self, dpath):
		bestzip = None
		ziplist = self.downloadTigerZips(dpath)
		for zname in ziplist:
			if shapefile.betterShapefileZip(zname, bestzip):
				bestzip = zname
		zipspath = self.zipspath(dpath)
		assert zipspath is not None
		assert bestzip is not None
		bestzip = os.path.join(zipspath, bestzip)
		linksname = os.path.join(dpath, self.stl + '101.uf1.links')
		mppb_name = os.path.join(dpath, self.stu + '.mppb')
		mask_name = os.path.join(dpath, self.stu + 'mask.png')
		mppbsm_name = os.path.join(dpath, self.stu + '_sm.mppb')
		masksm_name = os.path.join(dpath, self.stu + 'mask_sm.png')
		args1 = []
		if not os.path.exists(linksname):
			print 'need %s' % linksname
			args1 += ['--links', linksname]
		if not os.path.exists(mppb_name):
			print 'need %s' % mppb_name
			args1 += ['--rast', mppb_name, '--mask', mask_name,
				'--boundx', '1920', '--boundy', '1080']
		if args1:
			args1.append(bestzip)
			command = shapefile.makeCommand(args1, self.options.bindir)
			print ' '.join(command)
			if not self.options.dryrun:
				status = subprocess.call(command, shell=False, stdin=None)
				if status != 0:
					raise Exception('error (%d) executing: "%s"' % (status, ' '.join(command)))
		if not os.path.exists(mppbsm_name):
			print 'need %s' % mppbsm_name
			command = shapefile.makeCommand([
				'--rast', mppbsm_name, '--mask', masksm_name,
				'--boundx', '640', '--boundy', '480', bestzip],
				self.options.bindir)
			print ' '.join(command)
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
			print 'no ' + linkspath
			needlinks = True
		if (not needlinks) and newerthan(makelinks.__file__, linkspath):
			print makelinks.__file__ + ' > ' + linkspath
			needlinks = True
		if not needlinks:
			for z in ziplist:
				zp = os.path.join(zipspath, z)
				if newerthan(zp, linkspath):
					print zp + ' > ' + linkspath
					needlinks = True
					break
		if needlinks:
			print '%s/{%s} -> %s' % (zipspath, ','.join(ziplist), linkspath)
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
			print 'makelinks took %f seconds' % (time.time() - start)
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
			print uf1path, '>', outpath
			needsbuild = True
		if (not needsbuild) and newerthan(linkspath, outpath):
			print linkspath, '>', outpath
			needsbuild = True
		if (not needsbuild) and newerthan(binpath, outpath):
			print binpath, '>', outpath
			needsbuild = True
		if not needsbuild:
			return
#		if not (newerthan(uf1path, outpath) or newerthan(linkspath, outpath)):
#			return
		print 'cd %s && "%s"' % (dpath, '" "'.join(cmd))
		if self.options.dryrun:
			return
		start = time.time()
		status = subprocess.call(cmd, cwd=dpath)
		print 'data compile took %f seconds' % (time.time() - start)
		if status != 0:
			raise Exception('error (%d) executing: cd %s && "%s"' % (status, dpath,'" "'.join(cmd)))
	
	def processShapefileBundle(self, dpath):
		zipspath = self.zipspath(dpath)
	
	def measureGeometryParasite(self, dpath, fname, zf, name, raw):
		ln = name.lower()
		if (ln.endswith('.rt1') or
				ln.endswith('.rt2') or
				ln.endswith('.rta') or
				ln.endswith('.rti')):
			rawpath = os.path.join(dpath, 'raw', name)
			if not newerthan(fname, rawpath):
				return
			if raw is None:
				raw = zf.read(name)
			if self.options.dryrun:
				print 'would extract "%s"' % (rawpath)
				return
			out = file(rawpath, 'wb')
			out.write(raw)
			out.close()
	
	def measureGeometry(self, dpath):
		if self.geom is not None:
			return self.geom
		zipspath = self.zipspath(dpath)
		ziplist = self.downloadTigerZips(dpath)
		pgeompath = os.path.join(dpath, 'geometry.pickle')
		needsmeasure = False
		rawdir = os.path.join(dpath, 'raw')
		needsmeasure = False
		if not os.path.isfile(pgeompath):
			needsmeasure = True
		if (not needsmeasure) and newerthan(measureGeometry.__file__, pgeompath):
			needsmeasure = True
		if not needsmeasure:
			for z in ziplist:
				if newerthan(os.path.join(zipspath, z), pgeompath):
					needsmeasure = True
					break
		if needsmeasure:
			print '%s/{%s} -> %s/{geometry.pickle,meausure,makedefaults}' % (zipspath, ','.join(ziplist), dpath)
		if needsmeasure and not self.options.dryrun:
			start = time.time()
			g = measureGeometry.geom()
			mkdir(rawdir, self.options)
			for z in ziplist:
				zpath = os.path.join(zipspath, z)
				g.checkZip(zpath,
					lambda z,n,r: self.measureGeometryParasite(dpath,zpath,z,n,r))
			g.calculate()
			pout = open(pgeompath, 'wb')
			pickle.dump(g, pout, protocol=2)
			pout.close()
			mout = open(os.path.join(dpath, 'measure'), 'w')
			g.writeMeasure(mout)
			mout.close()
			dout = open(os.path.join(dpath, 'makedefaults'), 'w')
			g.makedefaults(dout, self.stu)
			dout.close()
			print 'measureGeometry took %f seconds' % (time.time() - start)
		else:
			fin = open(pgeompath, 'rb')
			g = pickle.load(fin)
			fin.close()
		self.geom = g
		return self.geom
	
	def writeMakeFragment(self, dpath):
		mfpath = os.path.join(dpath, '.make')
		if not newerthan(__file__, mfpath):
			return mfpath
		print '->', mfpath
		if self.options.protobuf:
			stl_bin = self.stl + '.pb'
		else:
			stl_bin = self.stl + '.gbin'
		if self.options.dryrun:
			print 'would write "%s"' % (mfpath)
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

	def getextras(self, dpath, extras):
		geourl = self.pg.getGeoUrl(self.stl)
		zipspath = self.zipspath(dpath)
		mkdir(zipspath, self.options)
		for x in extras:
			xurl = geourl.replace('geo_uf1', x + '_uf1')
			xpath = os.path.join(zipspath, self.stl + x + '_uf1.zip')
			if not os.path.isfile(xpath):
				print '%s -> %s' % (xurl, xpath)
				if not self.options.dryrun:
					urllib.urlretrieve(xurl, xpath)
	
	def dostate(self):
		dpath = os.path.join(self.options.datadir, self.stu)
		mkdir(dpath, self.options)
		if self.options.extras:
			self.getextras(dpath, self.options.extras)
			if self.options.extras_only:
				return
		geozip = os.path.join(dpath, self.stl + 'geo_uf1.zip')
		uf101 = os.path.join(dpath, self.stl + '101.uf1')
		self.makeUf101(geozip, uf101, dpath)
		self.makelinks(dpath)
		self.compileBinaryData(dpath)
		#geom = self.measureGeometry(dpath)
		handargspath = os.path.join(dpath, 'handargs')
		if not os.path.isfile(handargspath):
			if self.options.dryrun:
				print 'would write "%s"' % (handargspath)
			else:
				ha = open(handargspath, 'w')
				ha.write('-g 10000\n')
				ha.close
		makefile = self.writeMakeFragment(dpath)
		generaterunconfigs.run(
			datadir=self.options.datadir,
			stulist=[self.stu],
			dryrun=self.options.dryrun)
		makecmd = ['make', '-k', self.stu + '_all', '-f', makefile]
		if self.options.dryrun:
			print 'would run "%s"' % (' '.join(makecmd))
		else:
			start = time.time()
			status = subprocess.call(makecmd)
			if status != 0:
				sys.stderr.write(
					'command "%s" failed with %d\n' % (' '.join(makecmd), status))
			print 'final make took %f seconds' % (time.time() - start)
		

def main(argv):
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
	argp.add_option('--shapefile', dest='shapefile', action='store_true', default=True)
	argp.add_option('--noshapefile', dest='shapefile', action='store_false')
	(options, args) = argp.parse_args()

	if not os.path.isdir(options.datadir):
		raise Error('data dir "%s" does not exist' % options.datadir)

	if not options.shapefile:
		makefile_fragment_template = string.Template(
			basic_make_rules_ + old_makepolys_rules_)
	pg = ProcessGlobals(options)

	for a in args:
		print a
		start = time.time()
		sd = StateData(pg, a, options)
		sd.dostate()
		print '%s took %f seconds' % (a, time.time() - start)

if __name__ == '__main__':
	main(sys.argv)
