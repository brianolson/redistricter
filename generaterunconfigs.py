#!/usr/bin/python

"""
Reads legislatures.csv and writes data/??/config/*
"""

import csv
import os
import sys

legpath_ = os.path.join(os.path.dirname(__file__), 'legislatures.csv')

from states import codeForState

def run(datadir='data', stulist=None, dryrun=False, newerthan=None):
	congress_total = 0
	f = open(legpath_, 'rb')
	fc = csv.reader(f)
	for row in fc:
		stu = codeForState(row[0])
		if (stulist is not None) and (stu not in stulist):
			continue
		configdir = os.path.join(datadir, stu, 'config')
		if not os.path.isdir(configdir):
			os.makedirs(configdir)
		name_part = row[1].split()[0]
		if name_part == 'Congress':
			congress_total += int(row[2])
		outname = os.path.join(configdir, name_part)
		if newerthan and not newerthan(legpath_, outname):
			if dryrun:
				print '"%s" up to date' % (outname)
			continue
		if dryrun:
			print 'would write "%s"' % (outname)
		else:
			out = open(outname, 'w')
			out.write("""datadir: $DATA/%s\ncommon: -d %s\n""" % (stu, row[2]))
			if (row[2] == '1') or (row[2] == 1):
				out.write('disabled\n')
			out.close()
	return congress_total

if __name__ == '__main__':
	congress_total = run()
	print 'congress_total == %s, should be 435' % congress_total
