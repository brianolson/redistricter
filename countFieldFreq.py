#!/usr/bin/python

import csv


class FFCounter(object):
  def __init__(self):
    self._counters = None
    self.reccount = 0

  def _genCounters(self, cols):
    self._counters = []
    for i in xrange(0, cols):
      self._counters.append(dict())

  def countFeildFreq(self, fin, fields=None):
    if (self._counters is None) and fields:
      self._counters = [None] * (max(fields) + 1)
      #print 'counters = %r, fields=%r' % (self._counters, fields)
      for i in fields:
        self._counters[i] = dict()
    reader = csv.reader(fin)
    for row in reader:
      self.reccount += 1
      if self._counters is None:
        self._genCounters(len(row))
      for i, d in enumerate(self._counters):
        if d is not None:
          if i < len(row):
            v = row[i]
          else:
            v = 'None'
          d[v] = d.get(v, 0) + 1
  
  def __str__(self):
    outl = []
    for i, d in enumerate(self._counters):
      if not d:
        continue
      outl.append('%s:' % (i,))
      dk = d.keys()
      dk.sort()
      for k in dk:
        dv = d[k]
        outl.append('\t%s: %s' % (k, dv))
    return '\n'.join(outl)



if __name__ == '__main__':
  import optparse
  import sys
  import time

  start = time.time()
  op = optparse.OptionParser()
  op.add_option('-f', '--field', dest='fields', action='append', type='int', default=[])
  options, args = op.parse_args()
  c = FFCounter()
  if args:
    for arg in args:
      fin = open(arg, 'rb')
      c.countFeildFreq(fin, options.fields)
  else:
    c.countFeildFreq(sys.stdin, options.fields)

  end = time.time()
  sys.stderr.write('%s records in %s seconds\n' % (c.reccount, end - start))

  print c


