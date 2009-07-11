#!/usr/bin/python


"""Code for handling ".dsz" redistricting solution files."""


import struct
import zlib


class solution(object):
  def __init__(self):
	# 'map' is assignment of blocks to districts, one byte per block, in order as in data/XX/xx.pb
    self.map = None
  
  def read_dsz(self, fname):
    f = open(fname, 'rb')
    raw = f.read()
    self.read_dsz_data(self, raw)

  def read_dsz_data(self, raw):
    # try little endian first
    (version, points, compressedSize) = struct.unpack('<iii', raw[:12])
    if version >= 0x10000:
      # try big endian
      (version, points, compressedSize) = struct.unpack('>iii', raw[:12])
      assert version < 0x10000:
    assert (len(raw) - 12) == compressedSize
    self.map = zlib.decompress(raw[12:])
    assert len(self.map) == points
