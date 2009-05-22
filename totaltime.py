#!/usr/bin/python

__author__ = 'brian.olson@gmail.com (Brian Olson)'

import os
import os.path
import re
import sys

timepattern = re.compile(r'.*District calculation:\s*([0-9.]+)\s*sec.*',
                         re.DOTALL|re.IGNORECASE|re.MULTILINE)

def main(argv):
  total = 0.0
  count = 0
  badcount = 0
  badlines = list()
  for root, dirs, files in os.walk('.'):
    if 'statsum' in files:
      path = os.path.join(root, 'statsum')
      f = open(path)
      stattext = f.read(4000)
      f.close()
      # example line:
      # #District calculation: 16691.462512 sec user time, 21.314760 system sec, 17453.5 17761 wall sec, 8.986630 g/s
      m = timepattern.match(stattext)
      count += 1
      if m:
        t = float(m.group(1))
        total += t
      else:
        badcount += 1
        if stattext and len(stattext):
          badlines.append(stattext)
          if badcount > 4:
            print "\n".join(badlines)
            return
  print ("total %f seconds (%f days) from %d runs (%d failed parse)"
          % (total, total / (3600.0 * 24.0), count, badcount))

if __name__ == '__main__':
  main(sys.argv)
