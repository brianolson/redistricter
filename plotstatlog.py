#!/usr/bin/python

import gzip
import re
import subprocess
import sys

gen_re = re.compile(r'gen (\d+)')
kmpp_re = re.compile(r'([0-9.]+) Km/person')
std_re = re.compile(r'std=([0-9.]+)')
spread_re = re.compile(r'max=([0-9.]+).*min=([0-9.]+)')
nodist_re = re.compile(r'in no district .pop=([0-9.]+)')

class statlog(object):
	def __init__(self):
		self.kmpp = []
		self.std = []
		self.spread = []
		self.nodist = []
		self.generation = None

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
			# TODO: write x\ty
			out.write("%0.15g\n" % xy[1])
		out.write(
"""e
set output 'statlog.png'
plot '-' title 'std','-' title 'spread'
""")
		for xy in self.std:
			# TODO: write x\ty
			out.write("%0.15g\n" % xy[1])
		out.write('e\n')
		for xy in self.spread:
			# TODO: write x\ty
			out.write("%0.15g\n" % xy[1])
		out.write('e\n')
		if len(self.nodist) > 2:
			out.write(
"""set output 'nodist.png'
set ylabel 'people'
plot '-' title 'no dist'
""")
			for xy in self.nodist:
				out.write("%0.15g\n" % xy[1])


def main(argv):
	x = statlog()
	x.readStatlogLines(sys.stdin)
	subp = subprocess.Popen(['gnuplot'], stdin=subprocess.PIPE)
	fout = subp.stdin
	#fout = open('/tmp/ps.gnuplot', 'w')
	x.writeGnuplotCommands(fout)
	fout.close()

if __name__ == '__main__':
	main(sys.argv)
	
