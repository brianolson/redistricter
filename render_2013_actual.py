#!/usr/bin/python

"""Writes out per-state map images to current directory.
Usage:
render_2013_actual.py --datadir=/Volumes/bulktogo/redata/2010 --districtdir=/Volumes/bulktogo/redata/2010/2013_actual/cd113
"""

# drend  --csv-solution=09_CT_CD113.txt -P=ct.pb --mppb=CT.mppb --pngout=ct113.png

import logging
import optparse
import os
import re
import subprocess
import sys


from analyze_submissions import getStatesCsvSources, processActualsSource
from newerthan import newerthan, any_newerthan
import states


def main():
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

