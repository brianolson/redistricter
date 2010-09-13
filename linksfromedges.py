#!/usr/bin/python
"""Use tools.jar and org.bolson.redistricter.LinksFromEdges to process
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
	cmd = ['java']
	if enableassertions:
		cmd.append('-enableassertions')
	return cmd + ['-Xmx1000M', '-cp', classpath, 'org.bolson.redistricter.LinksFromEdges'] + extra_args


def main(argv):
	command = makeCommand(argv[1:], enableassertions=True)
	print ' '.join(command)
	p = subprocess.Popen(command, shell=False, stdin=None)
	p.wait()

if __name__ == '__main__':
	main(sys.argv)
