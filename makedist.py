#!/usr/bin/python

import os
import subprocess
import sys
import tarfile
import time

# in name \t archive name
PACKAGEDEF = """
districter2_staticproto	bin/districter2
drend_static	bin/drend
client.py	bin/client.py
kmppspreadplot.py	bin/kmppspreadplot.py
manybest.py	bin/manybest.py
measurerace.py	bin/measurerace.py
newerthan.py	bin/newerthan.py
plotlib.js	bin/plotlib.js
plotstatlog.py	bin/plotstatlog.py
resultserver.py	bin/resultserver.py
resultspage.py	bin/resultspage.py
runallstates.py	bin/runallstates.py
solution.py	bin/solution.py
states.py	bin/states.py
totaltime.py	bin/totaltime.py
variability.py	bin/variability.py
run_redistricter.py	run_redistricter
at_home_README.txt	README.TXT
"""

def parsePackageDef(linesource):
	for line in linesource:
		line = line.strip()
		if not line:
			continue
		if line[0] == '#':
			continue
		out = line.split('\t')
		assert len(out) == 2, 'bad line: "%s"' % (line,)
		yield out

def main():
#	ok = subprocess.call(['make', '-j', '4', 'all', 'jall', 'districter2_staticproto', 'OG=-O2 -NDEBUG'])
#	assert ok == 0
	outprefix = time.strftime('redistricer_%Y%m%d_%H%M%S')
	outname = outprefix + '.tar.gz'
	files = parsePackageDef(PACKAGEDEF.splitlines())
	tf = tarfile.open(outname, 'w:gz')
	for fname, arcname in files:
		arcname = os.path.join(outprefix, arcname)
		tf.add(fname, arcname=arcname)
	tf.close()
	print 'wrote:'
	print outname

if __name__ == '__main__':
	main()

