#!/usr/bin/python

import os
import sys
import zipfile

tigerpath = os.path.join(
	os.path.dirname(
		os.path.abspath(__file__)),
	'tiger')
if tigerpath not in sys.path:
	sys.path.append(tigerpath)

from record1 import record1

class linker(object):
	def __init__(self):
		self.they = {}
		self.halves = {}
		self.verbose = False
	
	def put(self, a, b):
		if a < b:
			self.they[(a,b)] = 1
		else:
			self.they[(b,a)] = 1
	
	def half(self, ubid, tlid):
		x = self.halves.get(tlid)
		if x is not None:
			if x == ubid:
				if self.verbose:
					sys.stderr.write('weird split, tlid=%09d ubid=%013u\n' % (tlid, ubid))
				return
			self.put(ubid,x)
			del self.halves[tlid]
		else:
			self.halves[tlid] = ubid
	
	def process(self, cur):
		"""cur is a record1"""
		numrecs = len(cur.raw) / record1.fieldwidth
		for i in xrange(0, numrecs):
			stl = cur.STATEL(i)
			countyl = cur.COUNTYL(i)
			tractl = cur.TRACTL(i)
			blockl = cur.BLOCKL(i)
			if (stl[0] == ' ') or (countyl[0] == ' ') or (tractl[0] == ' ') or (blockl[0] == ' '):
				ubidl = None
			else:
				ubidl = long(countyl + tractl + blockl)
			rst = cur.STATER(i)
			countyr = cur.COUNTYR(i)
			tractr = cur.TRACTR(i)
			blockr = cur.BLOCKR(i)
			if (rst[0] == ' ') or (countyr[0] == ' ') or (tractr[0] == ' ') or (blockr[0] == ' '):
				ubidr = None
			else:
				ubidr = long(countyr + tractr + blockr)
			if ubidl is None:
				if ubidr is None:
					sys.stderr.write('error, %s:%d has neither left nor reight\n' % ('', i+1))
				else:
					self.half(ubidr, int(cur.TLID(i)))
			else:
				if ubidr is None:
					self.half(ubidl, int(cur.TLID(i)))
				elif ubidr == ubidl:
					pass
					#sys.stderr.write('dup ubid %013u\n' % ubidl)
				else:
					self.put(ubidl, ubidr)
	
	def processZipFilename(self, fname):
		zf = zipfile.ZipFile(fname, 'r')
		for x in zf.namelist():
			if x.lower().endswith('.rt1'):
				if self.verbose:
					sys.stderr.write('reading %s(%s)\n' % (fname, x))
				raw = zf.read(x)
				cur = record1(raw)
				self.process(cur)
	
	def processFilename(self, fname):
		fi = open(fname, 'r')
		raw = fi.read()
		cur = record1(raw)
		self.process(cur)
	
	def writeText(self, out):
		for x in self.they.iterkeys():
			(a, b) = x
			out.write('%013u%013u\n' % (a, b))

def main(argv):
	ta = argv[1:]
	out = sys.stdout
	sourcelist = []
	while ta:
		arg = ta.pop(0)
		if arg == '-o':
			out = open(ta.pop(0), 'wb')
		else:
			sourcelist.append(arg)
	l = linker()
	l.verbose = True
	for s in sourcelist:
		if s.lower().endswith('.zip'):
			l.processZipFilename(s)
		elif s.lower().endswith('.rt1'):
			l.processFilename(s)
	l.writeText(out)
	out.close()

if __name__ == '__main__':
	main(sys.argv)
