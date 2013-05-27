#!/usr/bin/python

"""Writes out per-state map images to current directory.
Usage:
render_2013_actual.py --datadir=/Volumes/bulktogo/redata/2010 --districtdir=/Volumes/bulktogo/redata/2010/2013_actual/cd113
"""

# drend  --csv-solution=09_CT_CD113.txt -P=ct.pb --mppb=CT.mppb --pngout=ct113.png

import csv
import logging
import optparse
import os
import re
import subprocess
import sys


from analyze_submissions import getStatesCsvSources, processActualsSource
from newerthan import newerthan, any_newerthan
import states


def csvToSimpleCsv(csvpath, outpath):
  """Convert CSV with district 'number' that could be '00A' '01A' 'MISC' to simple numeric district numbers."""
  fin = open(csvpath, 'rb')
  reader = csv.reader(fin)
  row = reader.next()
  # expect header row either:
  # BLOCKID,CD113
  # BLOCKID,DISTRICT,NAME
  assert row[0] == 'BLOCKID'
  assert ((row[1] == 'CD113') or (row[1] == 'DISTRICT'))
  unmapped = []
  districts = set()
  for row in reader:
    unmapped.append( (row[0], row[1]) )
    districts.add(row[1])
  fin.close()
  dl = list(districts)
  dl.sort()
  dmap = {}
  for i, dname in enumerate(dl):
    dmap[dname] = i + 1  # 1 based csv file district numbering
  fout = open(outpath, 'wb')
  writer = csv.writer(fout)
  for blockid, dname in unmapped:
    writer.writerow( (blockid, dmap[dname]) )
  fout.close()


_drendpath = None


def drendpath():
  global _drendpath
  if _drendpath is None:
    _drendpath = os.path.join(srcdir_, 'drend')
    if not os.path.exists(_drendpath):
      logging.error('no drend binary at %r', drendpath)
      sys.exit(1)
  return _drendpath




def main():
  srcdir_ = os.path.dirname(os.path.abspath(__file__))

  op = optparse.OptionParser()
  op.add_option('--datadir', dest='datadir')
  op.add_option('--districtdir', dest='distdir', help='where the district CSV files live')

  options, args = op.parse_args()

  stDistFiles, anyError = getStatesCsvSources(options.distdir)
  #for k,v in stDistFiles.iteritems():
  #  print '%s\t%s' % (k, v)

  if anyError:
    sys.exit(1)

  for stu, sourceCsvFname in stDistFiles.iteritems():
    print stu
    stl = stu.lower()

    # common datadir inputs
    stdir = os.path.join(options.datadir, stu)
    pb = os.path.join(stdir, stl + '.pb')
    mppb = os.path.join(stdir, stu + '.mppb')
    zipname = os.path.join(stdir, 'zips', stl + '2010.pl.zip')

    processActualsSource(options.distdir, stu, sourceCsvFname, pb, mppb, zipname)


if __name__ == '__main__':
  main()

