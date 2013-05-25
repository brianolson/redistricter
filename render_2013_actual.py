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


from analyze_submissions import measure_race
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


def main():
  srcdir_ = os.path.dirname(os.path.abspath(__file__))

  op = optparse.OptionParser()
  op.add_option('--datadir', dest='datadir')
  op.add_option('--districtdir', dest='distdir', help='where the district CSV files live')

  options, args = op.parse_args()

  stDistFiles = {}
  anyError = False

  distdirall = os.listdir(options.distdir)
  
  statefileRawRe = re.compile('.._(..)_.*\.txt', re.IGNORECASE)

  for fname in distdirall:
    m = statefileRawRe.match(fname)
    if m:
      stu = m.group(1).upper()
      if states.nameForPostalCode(stu) is not None:
        # winner
        old = stDistFiles.get(stu)
        if old is None:
          stDistFiles[stu] = fname
        else:
          logging.error('collision %s -> %s AND %s', stu, old, fname)
          anyError = True

  #for k,v in stDistFiles.iteritems():
  #  print '%s\t%s' % (k, v)

  if anyError:
    sys.exit(1)

  drendpath = os.path.join(srcdir_, 'drend')
  if not os.path.exists(drendpath):
    logging.error('no drend binary at %r', drendpath)
    sys.exit(1)

  def noSource(sourceName):
    logging.error('missing source %s', sourceName)

  for k,v in stDistFiles.iteritems():
    print k
    stl = k.lower()
    stu = k.upper()
    csvpath = os.path.join(options.distdir, v)
    simplecsvpath = os.path.join(options.distdir, stu + '.csv')
    stdir = os.path.join(options.datadir, stu)
    pb = os.path.join(stdir, stl + '.pb')
    mppb = os.path.join(stdir, stu + '.mppb')
    zipname = os.path.join(stdir, 'zips', stl + '2010.pl.zip')
    htmlout = os.path.join(stu + '.html')
    pngout = stu + '.png'
    if any_newerthan( (pb, mppb, csvpath, zipname), (pngout, htmlout), noSourceCallback=noSource):
      if newerthan(csvpath, simplecsvpath):
        csvToSimpleCsv(csvpath, simplecsvpath)
      if any_newerthan( (pb, mppb, simplecsvpath), pngout):
        cmd = [drendpath, '-d=-1', '-P', pb, '--mppb', mppb, '--csv-solution', simplecsvpath, '--pngout', pngout]
        print ' '.join(cmd)
        p = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, shell=False)
        retcode = p.wait()
        if retcode != 0:
          logging.error('cmd `%s` retcode %s log %s', cmd, retcode, p.stdout.read())
          sys.exit(1)
      if any_newerthan( (pb, mppb, simplecsvpath, zipname), htmlout):
        measure_race(stl, '-1', pb, simplecsvpath, htmlout, zipname, printcmd=lambda x: sys.stderr.write(' '.join(x) + '\n'))



if __name__ == '__main__':
  main()

