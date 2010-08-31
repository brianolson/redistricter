#!/usr/bin/python

import gzip
import os
import re
import subprocess
import sys

gen_re = re.compile(r'gen(?:eration)? (\d+)')
kmpp_re = re.compile(r'([0-9.]+) Km/person')
std_re = re.compile(r'std=([0-9.]+)')
spread_re = re.compile(r'max=([0-9.]+).*min=([0-9.]+)')
nodist_re = re.compile(r'in no district .pop=([0-9.]+)')


def xyRangeMinMax(seq, xmin, xmax):
	"""For a sequence of (x,y) values, return (ymin,ymax) within some range of x."""
	ymin = None
	ymax = None
	for (x,y) in seq:
		if (x < xmin) or (x > xmax):
			continue
		if (ymin is None) or (y < ymin):
			ymin = y
		if (ymax is None) or (y > ymax):
			ymax = y
	return (ymin, ymax)


class statlog(object):
	def __init__(self, path=None):
		self.kmpp = []
		self.std = []
		self.spread = []
		self.nodist = []
		self.generation = None
		if path is not None:
			self.readPath(path)
	
	def readPath(self, path):
		if path[-3:].lower() == '.gz':
			fin = gzip.open(path, 'rb')
		else:
			fin = open(path, 'r')
		self.readStatlogLines(fin)
		fin.close()

	def readStatlogLines(self, fin):
		for line in fin:
			if line.startswith('#'):
				continue
			#generation: 9100
			#gen 9100: 8325 in no district (pop=454931) 20.963471108 Km/person
			#population avg=590127 std=108173.879
			#max=674088 (dist# 3)	min=412281 (dist# 2)	median=645525 (dist# 1)
			m = gen_re.search(line)
			if m:
				self.generation = int(m.group(1))
			m = kmpp_re.search(line)
			if m:
				xy = (self.generation, float(m.group(1)))
				self.kmpp.append(xy)
			m = std_re.search(line)
			if m:
				xy = (self.generation, float(m.group(1)))
				self.std.append(xy)
			m = spread_re.search(line)
			if m:
				maxv = float(m.group(1))
				minv = float(m.group(2))
				xy = (self.generation, maxv-minv, minv, maxv)
				self.spread.append(xy)
			m = nodist_re.search(line)
			if m:
				xy = (self.generation, float(m.group(1)))
				self.nodist.append(xy)

	def writeGnuplotCommands(self, out):
		out.write(
"""set logscale y
set lines
set terminal png
set output 'kmpp.png'
plot '-' title 'Km/person'
""")
		for xy in self.kmpp:
			out.write("%g\t%0.15g\n" % (xy[0], xy[1]))
		out.write(
"""e
set output 'statlog.png'
plot '-' title 'std','-' title 'spread'
""")
		for xy in self.std:
			out.write("%g\t%0.15g\n" % (xy[0], xy[1]))
		out.write('e\n')
		for xy in self.spread:
			out.write("%g\t%0.15g\n" % (xy[0], xy[1]))
		out.write('e\n')
		out.write(
"""set output 'kmpp_var.png'
plot '-' title 'Km/person fractional range per 1000 steps'
""")
		for xy in self.kmpp:
			(ymin, ymax) = xyRangeMinMax(self.kmpp, xy[0] - 1000, xy[0])
			if (ymin is None) or (ymax is None):
				continue
			yf = (ymax - ymin) / xy[1]
			out.write("%g\t%0.15g\n" % (xy[0], yf))
		out.write('e\n')
		if len(self.nodist) > 2:
			out.write(
"""set output 'nodist.png'
set ylabel 'people'
plot '-' title 'no dist'
""")
			for xy in self.nodist:
				out.write("%0.15g\n" % xy[1])

	def writeJson(self, out):
		parts = []
		parts.append('"kmpp":[' + ','.join(['%g,%0.15g' % xy for xy in self.kmpp]) + ']')
		parts.append('"std":[' + ','.join(['%g,%0.15g' % xy for xy in self.std]) + ']')
		parts.append('"spread":[' + ','.join(['%g,%0.15g' % (xy[0], xy[1]) for xy in self.spread]) + ']')
		if len(self.nodist) > 2:
			parts.append('"nodist":[' + ','.join(['%g,%0.15g' % xy for xy in self.nodist]) + ']')
		out.write('{' + ','.join(parts) + '}')

def main(argv):
	x = statlog()
	fin = sys.stdin
	if os.path.exists('statlog.gz'):
		fin = gzip.open('statlog.gz', 'rb')
	elif os.path.exists('statlog'):
		fin = open('statlog', 'r')
	x.readStatlogLines(fin)
	fin.close()
	subp = None
	if True:
		subp = subprocess.Popen(['gnuplot'], stdin=subprocess.PIPE)
		fout = subp.stdin
	else:
		fout = open('/tmp/ps.gnuplot', 'w')
	x.writeGnuplotCommands(fout)
	fout.close()
	if subp is not None:
		subp.wait()

if __name__ == '__main__':
	main(sys.argv)
	
