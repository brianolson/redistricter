#!/usr/bin/python

"""Analyze statlog files for kmmp and spread variability.

Look for boundaries that would be useful to set for
--kmppGiveupFraction and --spreadGiveupFraction
"""

__author__ = 'Brian Olson'


import gzip
import logging
import math
import os
import re


KMPP_VAR_RE = re.compile(r'kmpp var per ([0-9]+)=([0-9.]+)')
SPREAD_VAR_RE = re.compile(r'spread var per ([0-9]+)=([0-9.]+)')


kmppValues = []
spreadValues = []


class ExponentialHistogram(object):
	def __init__(self, min, max, buckets=20):
		# N buckets means N-1 split points
		kmax = max / min
		factor = math.pow(kmax, 1.0 / buckets)
		self.splits = [min + (min * (factor**k)) for k in xrange(1,buckets)]
		self.buckets = [0 for x in xrange(0,buckets)]
	
	def add(self, value):
		i = 0
		while i < len(self.splits):
			if value < self.splits[i]:
				self.buckets[i] += 1
				return
			i += 1
		self.buckets[i] += 1
	
	def addAll(self, they):
		for x in they:
			self.add(x)
	
	def __str__(self):
		lines = []
		lines.append('       - %6g: %d' % (self.splits[0], self.buckets[0]))
		for i in xrange(1, len(self.splits)):
			lines.append('%6g - %6g: %d' % (self.splits[i-1], self.splits[i], self.buckets[i]))
		lines.append('%6g -       : %d' % (self.splits[-1], self.buckets[-1]))
		return '\n'.join(lines)


def allowPerValue(per, value):
	if per == 5000:
		return True
	return False


def processLines(lines):
	count = 0
	for line in lines:
		m = KMPP_VAR_RE.search(line)
		if m:
			per = int(m.group(1))
			value = float(m.group(2))
			if allowPerValue(per, value):
				kmppValues.append(value)
				count += 1
		m = SPREAD_VAR_RE.search(line)
		if m:
			per = int(m.group(1))
			value = float(m.group(2))
			if allowPerValue(per, value):
				spreadValues.append(value)
				count += 1
	logging.info('read %d values', count)


def gatherValues(path='.'):
	global kmppValues
	global spreadValues
	for dirpath, dirnames, filenames in os.walk(path):
		for fname in filenames:
			#logging.debug('walk: %s', os.path.join(dirpath, fname))
			if fname == 'statlog':
				fpath = os.path.join(dirpath, fname)
				logging.info('reading: %s', fpath)
				f = open(fpath, 'r')
				processLines(f)
				f.close()
			elif fname == 'statlog.gz':
				fpath = os.path.join(dirpath, fname)
				logging.info('reading: %s', fpath)
				f = gzip.open(fpath, 'rb')
				processLines(f)
				f.close()
	print 'found %d kmpp values, %d spread values' % (len(kmppValues), len(spreadValues))
	

def main():
	logging.getLogger().setLevel(logging.DEBUG)
	gatherValues('.')
	minKmppv = min(kmppValues)
	maxKmppv = max(kmppValues)
	print '%f < kmpp-v < %f' % (minKmppv, maxKmppv)
	kmppvhist = ExponentialHistogram(minKmppv, maxKmppv, 20)
	kmppvhist.addAll(kmppValues)
	print kmppvhist
	
	minSpreadv = min(spreadValues)
	maxSpreadv = max(spreadValues)
	print '%f < spread-v < %f' % (minSpreadv, maxSpreadv)
	spreadvhist = ExponentialHistogram(minSpreadv, maxSpreadv, 20)
	spreadvhist.addAll(spreadValues)
	print kmppvhist


if __name__ == '__main__':
	main()
