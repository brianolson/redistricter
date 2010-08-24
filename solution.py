#!/usr/bin/python


"""Code for handling ".dsz" redistricting solution files."""


import struct
import sys
import zlib


class solution(object):
	def __init__(self):
	# 'map' is assignment of blocks to districts, one byte per block, in order as in data/XX/xx.pb
		self.map = None
	
	def read_dsz(self, fname):
		f = open(fname, 'rb')
		raw = f.read()
		self.read_dsz_data(raw)

	def read_dsz_data(self, raw):
		# try little endian first
		(version, points, compressedSize) = struct.unpack('<iii', raw[:12])
		if version >= 0x10000:
			# try big endian
			(version, points, compressedSize) = struct.unpack('>iii', raw[:12])
			assert version < 0x10000
		assert (len(raw) - 12) == compressedSize
		self.map = zlib.decompress(raw[12:])
		assert len(self.map) == points

	def countDistricts(self):
		cds = {}
		for c in self.map:
			if c != -1:
				cds[c] = 1
		print cds
		return len(cds)

def makeDsz(intlist):
	version = 4
	points = len(intlist)
	bytes = ''
	for x in intlist:
		bytes += struct.pack('b', x)
	assert len(bytes) == points
	cbytes = zlib.compress(bytes)
	compressedSize = len(cbytes)
	header = struct.pack('<iii', version, points, compressedSize)
	return header + cbytes

def main(argv):
	for x in argv[1:]:
		s = solution()
		s.read_dsz(x)
		print '%s\t%d' % (x, s.countDistricts())

if __name__ == '__main__':
	main(sys.argv)
