#!/usr/bin/python

"""Parse Census 2009 shapefile bundled files.

http://www.census.gov/geo/www/tiger/tgrshp2009/TGRSHP09.pdf

Parse an ESRI Shapefile, or at least parse enough to read Census data.

http://www.esri.com/library/whitepapers/pdfs/shapefile.pdf

And associated dBASE .dbf file.

http://www.dbase.com/knowledgebase/int/db7_file_fmt.htm
http://www.clicketyclick.dk/databases/xbase/format/dbf.html#DBF_STRUCT
"""

__author__ = "Brian Olson"


import StringIO
import struct
import sys
import time
import zipfile

class ShapefileHeaderField(object):
	def __init__(self, start, stop, format):
		self.start = start
		self.stop = stop
		self.format = format

	def get(self, rawbytes):
		return struct.unpack(
			self.format, rawbytes[self.start:self.stop])[0]


FILE_HEADER_ = {
	'fileCode': ShapefileHeaderField(0,4,'>i'),
	'fileLength': ShapefileHeaderField(24,28,'>i'),
	'version': ShapefileHeaderField(28,32,'<i'),
	'shapeType': ShapefileHeaderField(32,36,'<i'),
	'xmin': ShapefileHeaderField(36,44,'<d'),
	'ymin': ShapefileHeaderField(44,52,'<d'),
	'xmax': ShapefileHeaderField(52,60,'<d'),
	'ymax': ShapefileHeaderField(60,68,'<d'),
	'zmin': ShapefileHeaderField(68,76,'<d'),
	'zmax': ShapefileHeaderField(76,84,'<d'),
	'mmin': ShapefileHeaderField(84,92,'<d'),
	'mmax': ShapefileHeaderField(92,100,'<d'),
	}


SHAPE_TYPE_NAMES = {
	0: 'Null Shape',
	1: 'Point',
	3: 'PolyLine',
	5: 'Polygon',
	8: 'MultiPoint',
	11: 'PointZ',
	13: 'PolyLineZ',
	15: 'PolygonZ',
	18: 'MultiPointZ',
	21: 'PointM',
	23: 'PolyLineM',
	25: 'PolygonM',
	28: 'MultiPointM',
	31: 'MultiPatch',
	}


class ShapefileHeader(object):
	def __init__(self, headerbytes):
		self.rawbytes = headerbytes

	def fileCode(self):
		return struct.unpack('<i', self.rawbytes[0:3])

	def __getattr__(self, name):
		gh = FILE_HEADER_.get(name)
		if gh:
			return gh.get(self.rawbytes)
		raise AttributeError("no such attribute %s" % name)

	def __setattr__(self, name, value):
		if name in FILE_HEADER_:
			raise AttributeError("not allowed to write %s" % name)
		return object.__setattr__(self, name, value)


def ParseRecordHeader(rawbytes):
	"""In: 8 bytes.
	Out: (record number, content length in 16 bit word count)"""
	return struct.unpack('>ii', rawbytes)


part_count_hist_ = {}
point_count_hist_ = {}
def countHistInc(h, n):
	ov = h.get(n)
	if ov is None:
		ov = 0
	h[n] = ov + 1

class Polygon(object):
	"""Container/parser for Shapefile Polygon element (5)."""
	def __init__(self):
		self.shapetype = 5
		self.xmin = None
		self.ymin = None
		self.xmax = None
		self.ymax = None
		self.numParts = None
		self.numPoints = None
		self.parts = None
		self.points = None

	def parse(self, rawbytes):
		self.parseHeader(rawbytes[0:44])
		partsend = 44 + self.numParts * 4
		self.parts = struct.unpack(
		    '<%di' % self.numParts, rawbytes[44:partsend])
		self.points = struct.unpack(
		    '<%dd' % (self.numPoints * 2), rawbytes[partsend:])

	def parseHeader(self, rawbytes):
		assert len(rawbytes) == 44
		(shapetype,
		 self.xmin, self.ymin, self.xmax, self.ymax,
		 self.numParts, self.numPoints) = struct.unpack(
		    '<iddddii', rawbytes)
		assert shapetype == self.shapetype, 'expected shape %d, got %s' % (self.shapetype, shapetype)
		countHistInc(part_count_hist_, self.numParts)
		countHistInc(point_count_hist_, self.numPoints)

#	def __unicode__(self):
#		return u'(%f<=X<=%f)(%f<=Y<=%f)(parts ' + ' '.join(self.parts) + ')(points ' + ' '.join(self.points) + ')'

	def __str__(self):
		return (
		    '(%f<=X<=%f)(%f<=Y<=%f)(parts ' +
		    ' '.join(map(str,self.parts)) +
		    ')(points ' +
		    ' '.join(map(str,self.points)) + ')')

class PolyLine(Polygon):
	"""Container/parser for Shapefile PolyLine element (3)."""
	# It's the same data layout as Polygon, just different interpretation.
	# No last-point-duplicate-of-first-point, no closed loop.
	def __init__(self):
		Polygon.__init__(self)
		self.shapetype = 3


SHAPE_TYPES = {
	3: PolyLine,
	5: Polygon,
	}

def ReadRecord(fin):
	recordHeaderBytes = fin.read(8)
	if (recordHeaderBytes is None) or (len(recordHeaderBytes) < 8):
		return None
	(num, shorts) = ParseRecordHeader(recordHeaderBytes)
	rawbytes = fin.read(shorts * 2)
	if (rawbytes is None) or (len(rawbytes) < (shorts * 2)):
		return None
	(shapeType,) = struct.unpack('<i', rawbytes[0:4])
	#assert shapeType in SHAPE_TYPES, 'shapeType=%s' % shapeType
	p = SHAPE_TYPES[shapeType]()
	p.parse(rawbytes)
	return (num, p)


class Shapefile(object):
	def __init__(self):
		self.count = 0
		self.header = None
		#self.records = []

	def parseFile(self, f):
		self.header = ShapefileHeader(f.read(100))
		for i in FILE_HEADER_.iterkeys():
			print i, self.header.__getattr__(i)
		print 'ShapeType: ' + SHAPE_TYPE_NAMES[self.header.shapeType]
		while True:
			num_p = ReadRecord(f)
			if num_p is None:
				break
			#self.records.append(num_p[1])
			yield num_p[1]
			self.count += 1


class DBaseFieldDescriptor(object):
	def __init__(self, name=None, ftype=None, length=None, count=None,
		     mdx=None, aiv=None, rawbytes=None):
		self.name = name
		self.ftype = ftype
		self.length = length
		self.count = count
		self.mdx = mdx
		self.nextAutoincrementValue = aiv
		self.start = None
		self.end = None
		self.data = []
		if rawbytes is not None:
			self.parse(rawbytes)

	def parse(self, rawbytes):
		# TODO: strip trailing '\0'
		if len(rawbytes) == 48:
			(self.name, self.ftype, self.length, self.count,
			 unused_1,
			 self.mdx,
			 unused_2,
			 self.nextAutoincrementValue,
			 unused_3
			 ) = struct.unpack('<32scBBHBHII', rawbytes)
		elif len(rawbytes) == 32:
			(self.name, # 11s
			 self.ftype, # c
			 unused_1, # I
			 self.length, self.count, # BB
			 unused_2
			 ) = struct.unpack('<11scIBB14s', rawbytes)
		self.name = self.name.strip(' \t\r\n\0')

	def convert(self, raw):
		if self.ftype == 'C':
			return raw
		if self.ftype == 'N':
			if '.' in raw:
				return float(raw)
			else:
				return int(raw)
		assert False

	def fromLine(self, line):
		v = self.convert(line[self.start:self.end])
		self.data.append(v)
		return v

	def __str__(self):
		return 'DbaseField(name=%s %s length=%s count=%s)' % (
		    self.name, self.ftype, self.length, self.count)


class CensusDBaseFile(object):
	def __init__(self):
		self.version = None
		self.numRecords = None
		self.numHeaderBytes = None
		self.numRecordBytes = None
		self.incomplete = None
		self.encrypted = None
		self.mdxFlag = None
		self.languageId = None
		self.driverName = None
		self.specDate = None
		self.fields = []
		self.recordLength = None
		self.recordCount = 0

	def readFile(self, f):
		self.readHeader(f)
		x = f.read(1)
		while x != '\x1a':
			line = f.read(self.recordLength)
			yield self.parseLine(line)
			self.recordCount += 1
			x = f.read(1)

	def readHeader(self, f):
		headerbytes = f.read(32)
		(self.version,
		 year, month, day,
		 self.numRecords, self.numHeaderBytes, self.numRecordBytes,
		 unused_1,
		 self.incomplete, self.encrypted,
		 unused_2, unused_3, unused_4,
		 self.mdxFlag, self.languageId,
		 unused_5) = struct.unpack(
		    '<BBBBIHHHBBIIIBBH', headerbytes)
		if (self.version & 0x07) == 4:
			self.driverName = f.read(32)
			unused_6 = f.read(4)
			field_descriptor_b_size = 47
		else:
			field_descriptor_b_size = 31
			assert (self.version & 0x07) == 3
		startpos = 0
		year += 1900
		self.specDate = '%04d-%02d-%02d' % (year, month, day)
		print str(self)
		x = f.read(1)
		while x != '\x0d':
			xb = f.read(field_descriptor_b_size)
			newfield = DBaseFieldDescriptor(rawbytes=x+xb)
			print newfield
			newfield.start = startpos
			startpos += newfield.length
			newfield.end = startpos
			self.fields.append(newfield)
			x = f.read(1)
		self.recordLength = startpos

	def parseLine(self, line):
		out = {}
		for field in self.fields:
			out[field.name] = field.fromLine(line)
		return out

	def getRow(self, index):
		out = {}
		for field in self.fields:
			out[field.name] = field.data[index]
		return out

	def __str__(self):
		return 'Dbf(version=%02x numRecords=%d numHeaderBytes=%d numRecordBytes=%d incomplete=%d encrypted=%d mdx=%d lang=%d driver=%s specDate=%s)' % (
		    self.version, self.numRecords, self.numHeaderBytes, self.numRecordBytes, self.incomplete, self.encrypted, self.mdxFlag, self.languageId, self.driverName, self.specDate)

def readDbf(fname):
	start = time.time()
	f = open(fname, 'rb')
	dbf = CensusDBaseFile()
	for rec in dbf.readFile(f):
		pass
	f.close()
	stop = time.time()
	print 'read %d records in %f seconds' % (
	    dbf.recordCount, (stop - start))
	print dbf.getRow(100)

def readShapefile(fname):
	start = time.time()
	f = open(fname, 'rb')
	s = Shapefile()
	s.parseFile(f)
	stop = time.time()
	print 'read %d records in %f seconds' % (s.count, (stop - start))
	part_counts = part_count_hist_.keys()
	part_counts.sort()
	print 'part counts:'
	for i in part_counts:
		print '%d\t%d' % (i, part_count_hist_[i])
	point_counts = point_count_hist_.keys()
	point_counts.sort()
	print 'point counts:'
	for i in point_counts:
		print '%d\t%d' % (i, point_count_hist_[i])

def readZipfile(fname):
	zf = zipfile.ZipFile(fname)
	dbrecords = []
	polys = []
	dbf = None
	shp = None
	for arg in zf.namelist():
		if arg.endswith('.dbf'):
			dbf = CensusDBaseFile()
			for rec in dbf.readFile(StringIO.StringIO(zf.read(arg))):
				dbrecords.append(rec)
		elif arg.endswith('.shp'):
			shp = Shapefile()
			for rec in shp.parseFile(StringIO.StringIO(zf.read(arg))):
				polys.append(rec)
	assert len(dbrecords) == len(polys)
	return (dbf,shp,dbrecords,polys)

def main(argv):
	for arg in argv[1:]:
		if arg.endswith('.shp'):
			readShapefile(arg)
		elif arg.endswith('.dbf'):
			readDbf(arg)
		else:
			print 'bogus arg "%s"' % arg
			sys.exit(1)


if __name__ == '__main__':
	main(sys.argv)
