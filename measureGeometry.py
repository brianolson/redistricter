#!/usr/bin/python

import math
from optparse import OptionParser
import os
import re
import string
import sys
import zipfile

tigerpath = os.path.join(
	os.path.dirname(
		os.path.abspath(__file__)),
	'tiger')
if tigerpath not in sys.path:
	sys.path.append(tigerpath)

from record1 import record1
from record2 import record2
from recordA import recordA
#from recordI import recordI

class geom(object):
	def __init__(self):
		# measured:
		self.minlat = None
		self.maxlat = None
		self.minlon = None
		self.maxlon = None
		self.districts = {}
		# calculated:
		self.dlatraw = None
		self.dlonraw = None
		self.ratioraw = None
		self.cosineAvgLat = None
		self.dlon = None
		self.dlat = None
		self.ratio = None
		self.width = None
		self.height = None
		self.basewidth = None
		self.baseheight = None
	
	def checkcd(self, cd):
		try:
			x = int(cd)
			self.districts[x] = 1
		except:
			# int parse fail. whatever.
			pass
	
	def checklat(self, lat):
		if (self.minlat is None) or (self.minlat > lat):
			self.minlat = lat
		if (self.maxlat is None) or (self.maxlat < lat):
			self.maxlat = lat
	
	def checklon(self, lon):
		if (self.minlon is None) or (self.minlon > lon):
			self.minlon = lon
		if (self.maxlon is None) or (self.maxlon < lon):
			self.maxlon = lon
	
	def checkpt(self, lat, lon):
		self.checklat(lat / 1000000.0)
		self.checklon(lon / 1000000.0)
	
	def checkR2(self, raw):
		r2 = record2(raw)
		for i in xrange(r2.numRecords()):
			line = r2.record(i)
			for x in xrange(0, 10):
				lonstart = 18 + (19 * x)
				lonend = lonstart + 10
				latend = lonend + 9
				lon = line[lonstart:lonend]
				lat = line[lonend:latend]
				if not ((lon == '+000000000') and (lat == '+00000000')):
					self.checkpt(int(lat), int(lon))
	
	def checkR1(self, raw):
		r1 = record1(raw)
		for i in xrange(r1.numRecords()):
			lat = r1.FRLAT_int(i)
			lon = r1.FRLONG_int(i)
			self.checkpt(lat, lon)
			lat = r1.TOLAT_int(i)
			lon = r1.TOLONG_int(i)
			self.checkpt(lat, lon)
	
	def checkRA(self, raw):
		ra = recordA(raw)
		for i in xrange(ra.numRecords()):
			self.checkcd(ra.CDCU(i))
	
	def numCDs(self):
		return len(self.districts)
	
	def checkZip(self, fname, whileyoureatit=None):
		zf = zipfile.ZipFile(fname, 'r')
		for name in zf.namelist():
			ln = name.lower()
			raw = None
			if ln.endswith('.rt1'):
				raw = zf.read(name)
				self.checkR1(raw)
			elif ln.endswith('.rt2'):
				raw = zf.read(name)
				self.checkR2(raw)
			elif ln.endswith('.rta'):
				raw = zf.read(name)
				self.checkRA(raw)
			if whileyoureatit is not None:
				whileyoureatit(zf, name, raw)
		zf.close()
	
	def checkFile(self, fname):
		ln = fname.lower()
		if ln.endswith('.zip'):
			self.checkZip(fname)
			return True
		if ln.endswith('.rt1') or ln.endswith('.rt2') or ln.endswith('.rta'):
			f = open(fname, 'rb')
			raw	= f.read()
			if ln.endswith('.rt1'):
				self.checkR1(raw)
			elif ln.endswith('.rt2'):
				self.checkR2(raw)
			elif ln.endswith('.rta'):
				self.checkRA(raw)
			f.close()
			return True
		return False
	
	def calculate(self):
		self.dlatraw = self.maxlat - self.minlat
		self.dlonraw = self.maxlon - self.minlon
		self.ratioraw = (1.0 * self.dlonraw) / self.dlatraw
		self.cosineAvgLat = math.cos(((self.maxlat + self.minlat)/2.0) * math.pi / 180.0)
		self.dlon = self.dlonraw * self.cosineAvgLat
		self.dlat = self.dlatraw
		self.ratio = self.dlon / self.dlat
		self.width = 480 * self.ratio
		self.height = 640 / self.ratio
		if self.width > 640:
			self.basewidth = 640
			self.baseheight = self.height
		else:
			self.basewidth = self.width
			self.baseheight = 480

	measure_head_tpl = string.Template("""congressional districts: $cdlist
-d $numdists
    (minlon,maxlon), (minlat,maxlat)
bb: ($minlon,$maxlon), ($minlat,$maxlat)
--minlon $minlon --maxlon $maxlon --minlat $minlat --maxlat $maxlat

""")

	def writeMeasure(self, out):
		cdlist = ', '.join(map(str, self.districts))
		out.write(geom.measure_head_tpl.safe_substitute({
			'minlon': self.minlon,
			'maxlon': self.maxlon,
			'minlat': self.minlat,
			'maxlat': self.maxlat,
			'numdists': self.numCDs(),
			'cdlist': cdlist}))
		
		out.write("""dlon/dlat
%f/%f
%f
""" % (self.dlonraw, self.dlatraw, self.ratioraw))
		out.write("""* cos(average latitude)\t* %f
dlon/dlat
%f/%f
%f
""" % (self.cosineAvgLat, self.dlon, self.dlat, self.ratio))
		
		for mul in 1, 4, 8, 16:
			out.write('#--pngW %d --pngH %d\n' % (self.width * mul, 480 * mul))
			out.write('#--pngW %d --pngH %d\n' % (640 * mul, self.height * mul))
		out.write('--pngW %d --pngH %d\n' % (self.basewidth * 4, self.baseheight * 4))

	def makedefaults(self, out, stu):
		modes = [
			('', 4),
			('_SM', 1),
			('_LARGE', 8),
			('_HUGE', 16),
		]
		for suffix, mult in modes:
			out.write('%sPNGSIZE%s ?= --pngW %d --pngH %d\n' % (
				stu, suffix, self.basewidth * mult, self.baseheight * mult))
		out.write('%sLONLAT ?= --minlon %s --maxlon %s --minlat %s --maxlat %s\n' % (
			stu, self.minlon, self.maxlon, self.minlat, self.maxlat))
		out.write('%sDISTOPT ?= -d %d\n' % (stu, len(self.districts)))

	def run(self, stu, files, do_makedefaults=None, outdir=None, outname=None):
		for arg in files:
			ok = self.checkFile(arg)
			if not ok:
				raise Exception('error processing "%s"\n' % arg)
		self.calculate()
		if outdir:
			out = open(os.path.join(outdir, 'measure'), 'w')
		elif outname:
			out = open(outname, 'w')
		else:
			out = sys.stdout
		self.writeMeasure(out)
		out.close()
		if do_makedefaults or ((do_makedefaults is None) and outdir):
			if outdir:
				mdpath = os.path.join(outdir, 'makedefaults')
			else:
				mdpath = 'makedefaults'
			makedefaults = open(mdpath, 'w')
			self.makedefaults(makedefaults, stu)
			makedefaults.close()

def main(argv):
	oparser = OptionParser()
	oparser.add_option('-o', '--out', dest='outname', help='where to write measurment data to (default stdout)')
	oparser.add_option('-d', '--outdir', dest='outdir', help='directory to write "measure" and "makedefaults" to')
	oparser.add_option('--makedefaults', dest='do_makedefaults', action='store_true', default=None)
	oparser.add_option('--no-makedefaults', dest='do_makedefaults', action='store_false')
	oparser.add_option('-s', '--state', dest='state', help='state abbreviation')
	(options, args) = oparser.parse_args()
	stu = options.state
	files = []
	for arg in args:
		if os.path.isfile(arg):
			files.append(arg)
		elif re.match('[A-Z][A-Z]', arg):
			stu = arg
		else:
			raise Exception('bogus arg "%s"' % arg)
	if not stu:
		raise Exception('need to define state abbreviation')
	g = geom()
	g.run(stu, files, options.do_makedefaults, options.outdir, options.outname)
	

if __name__ == '__main__':
	main(sys.argv)
