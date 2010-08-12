#!/usr/bin/python

"""
Reads legislatures.csv and writes data/??/config/*
"""

import csv
import os
import sys

from states import codeForState

congress_total = 0

f = open('legislatures.csv', 'rb')
fc = csv.reader(f)
for row in fc:
	stu = codeForState(row[0])
	configdir = os.path.join('data', stu, 'config')
	if not os.path.isdir(configdir):
		os.makedirs(configdir)
	name_part = row[1].split()[0]
	if name_part == 'Congress':
		congress_total += int(row[2])
	outname = os.path.join(configdir, name_part)
	print (outname, row[2])
	out = open(outname, 'w')
	out.write("""datadir: $DATA/%s\ncommon: -d %s\n""" % (stu, row[2]))
	if (row[2] == '1') or (row[2] == 1):
		out.write('disabled\n')
	out.close()

print 'congress_total == %s, should be 435' % congress_total
