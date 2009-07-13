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

import measureGeometry
import makelinks

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


makefile_fragment_template = string.Template("""
-include ${dpath}/makedefaults
-include ${dpath}/makeoptions

${dpath}/${stl}109.dsz:	${dpath}/raw/*.RTA rta2dsz
	./rta2dsz -B ${dpath}/${stl_bin} $${${stu}DISTOPT} ${dpath}/raw/*.RTA -o ${dpath}/${stl}109.dsz

${dpath}/${stl_bin}:	${dpath}/.uf1 linkfixup
	./linkfixup -U ${dpath}/.uf1 -o ${dpath}/${stl_bin}

${dpath}/${stl}.pbin:	${dpath}/.uf1 linkfixup
	./linkfixup -U ${dpath}/.uf1 -p ${dpath}/${stl}.pbin
	
${stl}_all:	${dpath}/${stu}_start_sm.jpg ${dpath}/${stl}_sm.mpout ${dpath}/${stu}_start_sm.png

${dpath}/${stu}_start_sm.jpg:	${dpath}/${stu}_start.png
	convert ${dpath}/${stu}_start.png -resize 150x150 ${dpath}/${stu}_start_sm.jpg

${dpath}/${stu}_start.png:	drend ${dpath}/${stl}.mpout ${dpath}/${stl}109.dsz
	./drend -B ${dpath}/${stl_bin} $${${stu}DISTOPT} -px ${dpath}/${stl}.mpout --pngout ${dpath}/${stu}_start.png --loadSolution ${dpath}/${stl}109.dsz > ${dpath}/${stu}_start_stats

${dpath}/${stu}_start_sm.png:	drend ${dpath}/${stl}_sm.mpout ${dpath}/${stl}109.dsz
	./drend -B ${dpath}/${stl_bin} $${${stu}DISTOPT} -px ${dpath}/${stl}_sm.mpout --pngout ${dpath}/${stu}_start_sm.png --loadSolution ${dpath}/${stl}109.dsz

${dpath}/${stl}.mpout:	tiger/makepolys
	time ./tiger/makepolys -o ${dpath}/${stl}.mpout $${${stu}LONLAT} $${${stu}PNGSIZE} --maskout ${dpath}/${stl}mask.png ${dpath}/raw/*.RT1

${dpath}/${stl}_sm.mpout:	tiger/makepolys
	time ./tiger/makepolys -o ${dpath}/${stl}_sm.mpout $${${stu}LONLAT} $${${stu}PNGSIZE_SM} --maskout ${dpath}/${stl}mask_sm.png ${dpath}/raw/*.RT1

${dpath}/${stl}_large.mpout:	tiger/makepolys
	time ./tiger/makepolys -o ${dpath}/${stl}_large.mpout $${${stu}LONLAT} $${${stu}PNGSIZE_LARGE} --maskout ${dpath}/${stl}mask_large.png ${dpath}/raw/*.RT1

${dpath}/${stl}_huge.mpout:	tiger/makepolys
	time ./tiger/makepolys -o ${dpath}/${stl}_huge.mpout $${${stu}LONLAT} $${${stu}PNGSIZE_HUGE} --maskout ${dpath}/${stl}mask_huge.png ${dpath}/raw/*.RT1
""")


class ProcessGlobals(object):
	def __init__(self, options):
		self.options = options
		self.geourls = None
		self.tigerlatest = None

	def getSF1Index(self):
		"""Returns whole sf1 index html file in one buffer."""
		sf1ipath = os.path.join(self.options.datadir, sf1IndexName)
		if not os.path.isfile(sf1ipath):
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
	
	def getTigerLatest(self):
		if self.tigerlatest is not None:
			return self.tigerlatest
		uf = urllib.urlopen(tigerbase)
		raw = uf.read()
		uf.close()
		editions = re.compile(r'href="tiger(\d\d\d\d)(.)e/')
		bestyear = None
		bested = None
		edmap = { 'f': 1, 's': 2, 't': 3 }
		for m in editions.finditer(raw):
			year = m.group(1)
			ed = m.group(2)
			if (bestyear is None) or (year > bestyear):
				bestyear = year
				bested = ed
			elif (year == bestyear) and (edmap[ed] > edmap[bested]):
				bested = ed
		if bestyear is None:
			raise Error('found no tiger editions at "%s"' % tigerbase)
		# reconstruct absolute url to edition
		self.tigerlatest = tigerbase + 'tiger' + bestyear + bested + 'e/'
		return self.tigerlatest

class StateData(object):
	def __init__(self, globals, st):
		self.pg = globals
		self.stl = st.lower()
		self.stu = st.upper()
		self.turl = None
		self.ziplist = None
		self.geom = None
	
	def makeUf101(self, geozip, uf101):
		if not os.path.isfile(geozip):
			gurl = self.pg.getGeoUrl(self.stl)
			print 'fetching ' + gurl
			urllib.urlretrieve(gurl, geozip)
		print geozip + ' -> ' + uf101
		zf = zipfile.ZipFile(geozip, 'r')
		raw = zf.read(self.stl + 'geo.uf1')
		zf.close()
		filter = 'uSF1  ' + self.stu + '101'
		fo = open(uf101, 'w')
		for line in raw.splitlines(True):
			if line.startswith(filter):
				fo.write(line)
		fo.close()
	
	def getTigerBase(self, dpath):
		if self.turl is not None:
			return self.turl
		turlpath = os.path.join(dpath, 'tigerurl')
		if not os.path.isfile(turlpath):
			tigerlatest = self.pg.getTigerLatest()
			turl = tigerlatest + self.stu + '/'
			print 'guessing tiger data source "%s", if this is wrong, edit "%s"' % (turl, turlpath)
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
			if not os.path.isdir(zipspath):
				os.mkdir(zipspath)
			turl = self.getTigerBase(dpath)
			uf = urllib.urlopen(turl)
			raw = uf.read()
			uf.close()
			of = open(indexpath, 'w')
			of.write(raw)
			of.close()
		else:
			f = open(indexpath, 'r')
			raw = f.read()
			f.close()
		self.ziplist = []
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
				urllib.urlretrieve(turl + z, zp)
		return ziplist
	
	def makelinks(self, dpath):
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
	
	def compileBinaryData(self, options, dpath):
		uf1path = os.path.join(dpath, self.stl + '101.uf1')
		linkspath = uf1path + '.links'
		outpath = None
		binpath = os.path.join(options.bindir, 'linkfixup')
		cmd = [binpath, '-U', self.stl + '101.uf1']
		if options.protobuf:
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
		start = time.time()
		status = subprocess.call(cmd, cwd=dpath)
		print 'data compile took %f seconds' % (time.time() - start)
		if status != 0:
			raise Exception('error (%d) executing: cd %s && "%s"' % (status, dpath,'" "'.join(cmd)))
	
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
			start = time.time()
			g = measureGeometry.geom()
			if not os.path.isdir(rawdir):
				os.mkdir(rawdir)
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
	
	def writeMakeFragment(self, options, dpath):
		mfpath = os.path.join(dpath, '.make')
		if not newerthan(__file__, mfpath):
			return
		print '->', mfpath
		out = open(mfpath, 'w')
		if options.protobuf:
			stl_bin = self.stl + '.pb'
		else:
			stl_bin = self.stl + '.gbin'
		out.write(makefile_fragment_template.substitute({
			'dpath': dpath,
			'stu': self.stu,
			'stl': self.stl,
			'stl_bin': stl_bin}))
		out.close()

	def getextras(self, dpath, extras):
		geourl = self.pg.getGeoUrl(self.stl)
		zipspath = self.zipspath(dpath)
		if not os.path.isdir(zipspath):
			os.mkdir(zipspath)
		for x in extras:
			xurl = geourl.replace('geo_uf1', x + '_uf1')
			xpath = os.path.join(zipspath, self.stl + x + '_uf1.zip')
			if not os.path.isfile(xpath):
				print 'fetching ' + xurl
				urllib.urlretrieve(xurl, xpath)
	
	def dostate(self, options):
		dpath = os.path.join(options.datadir, self.stu)
		if not os.path.isdir(dpath):
			os.mkdir(dpath)
		if options.extras:
			self.getextras(dpath, options.extras)
			if options.extras_only:
				return
		geozip = os.path.join(dpath, self.stl + 'geo_uf1.zip')
		uf101 = os.path.join(dpath, self.stl + '101.uf1')
		if (not os.path.isfile(uf101)) or newerthan(geozip, uf101):
			self.makeUf101(geozip, uf101)
		self.makelinks(dpath)
		self.compileBinaryData(options, dpath)
		geom = self.measureGeometry(dpath)
		handargspath = os.path.join(dpath, 'handargs')
		if not os.path.isfile(handargspath):
			ha = open(handargspath, 'w')
			ha.write('-g 10000\n')
			ha.close
		# TODO: create ${stl}109.dsz
		self.writeMakeFragment(options, dpath)
		start = time.time()
		status = subprocess.call(['make', '-k', '-f', os.path.join(dpath, '.make'), self.stl + '_all'])
		print 'final make took %f seconds' % (time.time() - start)
		

def main(argv):
	argp = optparse.OptionParser()
	argp.add_option('-n', '--dry-run', action='store_false', dest='doit', default=True)
	argp.add_option('-m', '--make', action='store_true', dest='domaake', default=False)
	argp.add_option('--nopng', '--without_png', dest='png', action='store_false', default=True)
	argp.add_option('--unpackall', action='store_true', dest='unpackall', default=False)
	argp.add_option('--gbin', action='store_false', dest='protobuf', default=True)
	argp.add_option('-d', '--data', dest='datadir', default='data')
	argp.add_option('--bindir', dest='bindir', default=os.path.dirname(os.path.abspath(__file__)))
	argp.add_option('--getextra', dest='extras', action='append', default=[])
	argp.add_option('--extras_only', dest='extras_only', action='store_true', default=False)
	(options, args) = argp.parse_args()

	if not os.path.isdir(options.datadir):
		raise Error('data dir "%s" does not exist' % options.datadir)

	pg = ProcessGlobals(options)

	for a in args:
		print a
		start = time.time()
		sd = StateData(pg, a)
		sd.dostate(options)
		print '%s took %f seconds' % (a, time.time() - start)

if __name__ == '__main__':
	main(sys.argv)
