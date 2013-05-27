#!/usr/bin/python


"""Measure race demographics for 2000 census and 109th Congress. [DEPRECATED]
Create html tables for simple race breakdown per district.
Requires '00001' census data files, typically 'data/XX/zips/xx00001_uf1.zip'
which can be fetched by 'setupstatedata.py --getextra=00001
"""


import measureGeometry
import optparse
import os
import cPickle as pickle
from setupstatedata import newerthan
import string
import subprocess
import sys


cmd_template = string.Template(
'unzip -p ${datadir}/zips/${stl}${datafile}_uf1.zip ${stl}${datafile}.uf1 | '
'${bindir}/analyze '
'--compare :12,14,15,16,17,18,19,20 '
'--labels total,white,black,native,asian,pacific,other,mixed '
'--dsort 1 --notext '
'--html $htmlout '
'-B $pbfile -d $numd --loadSolution $solution')


def make_cmd(stl, bindir, datadir, datafile, pbfile, numd, solution, htmlout):
	return cmd_template.substitute({
		'datadir': datadir,
		'datafile': datafile,
		'stl': stl,
		'bindir': bindir,
		'pbfile': pbfile,
		'numd': numd,
		'solution': solution,
		'htmlout': htmlout
		})


def get_html_table(cmd):
	print cmd
	p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
	p.wait()
	return p.stdout.read()
	

def foo(bindir, datadir, stu, datafile, resultdir, prefix):
	stl = stu.lower()
	zipfile = datadir + '/zips/' + stl + datafile + '_uf1.zip'
	assert os.path.isfile(zipfile)
	pbfile = os.path.join(datadir, stl + '.pb')
	assert os.path.isfile(pbfile)
	f = open(os.path.join(datadir, 'geometry.pickle'))
	geom = pickle.load(f)
	numd = geom.numCDs()
	official_map = os.path.join(datadir, stl + '109.dsz')
	old_html_path = os.path.join(datadir, prefix + '.html')
	assert os.path.isfile(official_map)
	new_map = os.path.join(resultdir, 'link1', 'bestKmpp.dsz')
	assert os.path.isfile(new_map)
	new_html_path = os.path.join(resultdir, 'link1', prefix + '.html')
	if newerthan(pbfile, old_html_path) or newerthan(official_map, old_html_path):
		cmd = make_cmd(
			stl, bindir, datadir, datafile, pbfile, numd, official_map,
			old_html_path)
		#print cmd
		retcode = subprocess.call(cmd, shell=True)
		if retcode != 0:
			raise Exception('analyze failed with %d: %s' % (retcode, cmd))
	if newerthan(pbfile, new_html_path) or newerthan(new_map, new_html_path):
		cmd = make_cmd(
			stl, bindir, datadir, datafile, pbfile, numd, new_map,
			new_html_path)
		#print cmd
		retcode = subprocess.call(cmd, shell=True)
		if retcode != 0:
			raise Exception('analyze failed with %d: %s' % (retcode, cmd))


def main(argv):
	op = optparse.OptionParser()
	op.add_option('--datadir', dest='datadir', default='data')
	op.add_option('--resultdir', dest='resultdir', default='.')
	op.add_option('--bindir', dest='bindir', default='.')
	op.add_option('--prefix', dest='prefix', default='race')
	
	(options, args) = op.parse_args()
	for st in args:
		stu = st.upper()
		resultdir = os.path.join(options.resultdir, stu)
		assert os.path.isdir(resultdir)
		datadir = os.path.join(options.datadir, stu)
		assert os.path.isdir(datadir)
		foo(options.bindir, datadir, stu, '00001', resultdir, options.prefix)


if __name__ == '__main__':
	main(sys.argv)
