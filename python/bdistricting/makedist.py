#!/usr/bin/python

import os
import subprocess
import sys
import tarfile
import time

# in name \t archive name
PACKAGEDEF = """
districter2	bin/districter2
drend	bin/drend
python/bdistricting/run_redistricter.py	bin/run_redistricter.py
python/bdistricting/runallstates.py	bin/runallstates.py
python/bdistricting/client.py	bin/client.py
python/bdistricting/newerthan.py	bin/newerthan.py
python/bdistricting/manybest.py	bin/manybest.py

python/bdistricting/resultserver.py	bin/resultserver.py
python/bdistricting/kmppspreadplot.py	bin/kmppspreadplot.py
python/bdistricting/plotstatlog.py	bin/plotstatlog.py
html/plotlib.js	bin/plotlib.js

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
	outprefix = time.strftime('redistricter_%Y%m%d_%H%M%S')
	outname = outprefix + '.tar.xz'
	files = parsePackageDef(PACKAGEDEF.splitlines())
	tf = tarfile.open(outname, 'w:xz')
	for fname, arcname in files:
		arcname = os.path.join(outprefix, arcname)
		tf.add(fname, arcname=arcname)
	tf.close()
	print('wrote:')
	print(outname)

if __name__ == '__main__':
	main()
