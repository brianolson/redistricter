#!/usr/bin/python

"""Split a csv file into a file for each column."""


import csv
import glob
import gzip
import optparse
import subprocess
import sys
import zipfile


def processOneCsv(fin, outfiles, prefix='c'):
	reader = csv.reader(fin)
	for row in reader:
		if len(outfiles) == 0:
			for i in xrange(len(row)):
				outname = prefix + str(i) + '.gz'
				outfiles.append(gzip.open(outname, 'w'))
		assert len(row) == len(outfiles)
		for i in xrange(len(row)):
			outfiles[i].write(row[i] + '\n')


def pathSplit(path):
	parts = []
	while path:
		(head, tail) = os.path.split(path)
		if tail is not None:
			parts.insert(0, tail)
		elif head is not None:
			parts.insert(0, head)
			break
		else:
			sys.stderr.write('os.path.split(%s) failed\n' % path)
		path = head
	return parts


def processDataDir(datadir_path):
	zipfile_paths = glob.glob(datadir_path + '/??/zips/*_uf1.zip')
	dpl = len(datadir_path) + 1
	for zipfile_path in zipfile_paths:
		subdatadir = zipfile_path[dpl:]


def main(argv):
	prefix = 'c'
	outfiles = []
	op = optparse.OptionParser()
	op.add_option('--prefix', dest='prefix', default='c')
	op.add_option('--data', dest='data', default=None)
	(options, args) = op.parse_args()
	if not args:
		processOneCsv(sys.stdin, outfiles, fin.prefix)
	for arg in args:
		if arg.endswith('.zip'):
			zf = zipfile.ZipFile(arg, 'r')
			for name in zf.namelist():
				if name.endswith('.uf1'):
					print arg + '/' + name
					raw = zf.read(name)
					processOneCsv(
					    raw.splitlines(),
					    outfiles, options.prefix)
		else:
			fin = open(arg, 'r')
			processOneCsv(fin, outfiles, options.prefix)
	for fx in outfiles:
		fx.close()
		subprocess.call(['gzip', fx.name])

if __name__ == '__main__':
	main(sys.argv)
