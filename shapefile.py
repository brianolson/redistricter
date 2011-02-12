#!/usr/bin/python
"""Use tools.jar and org.bolson.redistricter.ShapefileBundle to process
census shapefile bundles."""

import glob
import os
import re
import subprocess
import sys

def globJarsToClasspath(dirname):
	jars = glob.glob(dirname + '/*.jar')
	return ':'.join(jars)

def makeCommand(extra_args, bindir=None, enableassertions=False):
	"""Return array of commands to run with subprocess."""
	if bindir is None:
		bindir = os.path.dirname(os.path.abspath(__file__))
	classpath = os.path.join(bindir, 'tools.jar') + ':' + globJarsToClasspath(os.path.join(bindir, 'jars'))
	java_home = os.environ.get('JAVA_HOME')
	cmd = None
	if java_home:
		java = os.path.join(java_home, 'bin', 'java')
		if os.path.isfile(java) and os.access(java, os.X_OK):
			cmd = [java]
	if not cmd:
		cmd = ['java']
	if enableassertions:
		cmd.append('-enableassertions')
	return cmd + ['-Xmx1000M', '-cp', classpath, 'org.bolson.redistricter.ShapefileBundle'] + extra_args


BUNDLE_NAME_RE_ = re.compile(r'.*tl_(\d\d\d\d)_(\d\d)_tabblock(\d?\d?).zip$', re.IGNORECASE)

# TODO: swap 00 vs non 00 meaning
def betterShapefileZip(a, b, prefer00=False):
	"""Return True if a is better than b.
	
	"Better" means newer year, or not Census00 type.
	"""
	if a is not None and b is None:
		return True
	ma = BUNDLE_NAME_RE_.match(a)
	mb = BUNDLE_NAME_RE_.match(b)
	if ma is None:
		raise Exception, '"%s" is not understood bundle name' % a
	if mb is None:
		raise Exception, '"%s" is not understood bundle name' % b
	if ma.group(2) != mb.group(2):
		raise Exception, 'trying to compare different states, %s and %s' % (ma.group(2), mb.group(2))
	# newer year is better
	if int(ma.group(1)) > int(mb.group(1)):
		return True
	if prefer00:
		if ma.group(3) and not mb.group(3):
			return True
	else:
		if mb.group(3) == '10':
			return False
		if ma.group(3) == '10':
			return True
		if mb.group(3) and not ma.group(3):
			return True
	return False

def processDatadir(datadir, stu, bindir=None, dryrun=True):
	bestzip is None
	zips = glob(os.path.join(datadir, 'zips', '*tabblock*zip'))
	for zname in zips:
		if betterShapefileZip(zname, bestzip):
			bestzip = zname
	linksname = os.path.join(datadir, stu + '.links')
	mppb_name = os.path.join(datadir, stu + '.mppb')
	mask_name = os.path.join(datadir, stu + 'mask.png')
	mppbsm_name = os.path.join(datadir, stu + '_sm.mppb')
	masksm_name = os.path.join(datadir, stu + 'mask_sm.png')
	args1 = []
	if not os.path.exists(linksname):
		args1 += ['--links', linksname]
	if not os.path.exists(mppb_name):
		args1 += ['--rast', mppb_name, '--mask', mask_name,
			'--boundx', '1920', '--boundy', '1080']
	if args1:
		command = makeCommand(args1, bindir)
		if dryrun:
			print ' '.join(command)
		else:
			subprocess.Popen(command, shell=False, stdin=None)
	if not os.path.exists(mppbsm_name):
		args2 = ['--rast', mppbsm_name, '--mask', masksm_name,
			'--boundx', '640', '--boundy', '480', bestzip]

def main(argv):
	command = makeCommand(argv[1:], enableassertions=True)
	#command = makeCommand(argv[1:], enableassertions=False)
	print ' '.join(command)
	p = subprocess.Popen(command, shell=False, stdin=None)
	p.wait()

if __name__ == '__main__':
	main(sys.argv)
