#!/usr/bin/python

import csv
import os
import pickle
import sys

import measureGeometry

from states import codeForState

datadir = 'data'

stus = {}

if not os.path.isdir('runconfigs'):
	os.makedirs('runconfigs')

f = open('legislatures.csv', 'rb')
fc = csv.reader(f)
for row in fc:
	stu = codeForState(row[0])
	shortname = stu + '_' + row[1].split()[0]
	print (shortname, row[2])
	out = open(os.path.join('runconfigs', shortname), 'w')
	out.write("""datadir: $DATA/%s\ncommon: -d %s\n""" % (stu, row[2]))
	out.close()
	stus[stu] = 1

for stu in stus.iterkeys():
	stuconfig = os.path.join('runconfigs', stu)
	if not os.path.exists(stuconfig):
		studata = os.path.join(datadir, stu)
		geompath = os.path.join(studata, 'geometry.pickle')
		if os.path.exists(geompath):
			f = open(geompath, 'rb')
			geom = pickle.load(f)
			f.close()
			if geom.numCDs() > 1:
				print stu
				out = open(stuconfig, 'w')
				out.write('datadir: $DATA/%s\n' % (stu))
				out.close()
			else:
				print '%s only has one district' % (stu)
