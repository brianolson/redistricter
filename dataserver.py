#!/usr/bin/python
#
# Run this in datasets dir:
# ${REDISTRICTER_BIN}/dataserver.py

import os
import string
import sys

from newerthan import newerthan

page_template = string.Template("""<!doctype html>
<html><head><title>${title}</title><head><body>
<h1>data sets</h1>
${datasets}
<h1>server/client config</h1>
<pre class="config">${config}</pre>
<h1>run configuration overrides</h1>
<p>Can be further overridden locally by 'configoverride' file in run directory or other file --config-override is pointing to.</p>
<pre class="runopts">${runopts}</pre>
</body></html>
""")

EXAMPLE_CONFIG_ = """[config]
configurl: http://127.0.0.1:8111/config
"""

EXAMPLE_RUNOPTS_ = """NH_House:disable
"""

def dictDictToConfig(sections):
	"""Take a dict() of dict() and return a string parsable by ConfigParser."""
	chunks = []
	for secname, secpairs in sections.iteritems():
		chunks.append('[%s]\n' % secname)
		for k, v in secpairs.iteritems():
			chunks.append('%s:%s\n' % (k, v))
	return ''.join(chunks)


def datafileLine(name, urlprefix=''):
	return '<div><a class="data" href="%s%s">%s</a></div>\n' % (urlprefix, name, name)

def datafileList(filenames, urlprefix=''):
	chunks = []
	for name in filenames:
		line = datafileLine(name, urlprefix)
		chunks.append(line)
	return ''.join(chunks)


def wrapConfigFile(configData):
	return '<pre class="config">' + configData + '</pre>\n'


def makeHTMLBodyForDirectory(dirpath='.'):
	"""Return HTML body listing config and data in directory."""
	ccpath = os.path.join(dirpath, 'client_config')
	if not os.path.exists(ccpath):
		config = EXAMPLE_CONFIG_
		try:
			fout = open(ccpath, 'w')
			fout.write(EXAMPLE_CONFIG_)
			fout.close()
		except Exception, e:
			sys.stderr.write(
				'tried to write basic client config to "%s" but failed: %s' % (ccpath, e))
	else:
		config = open(ccpath, 'r').read()
	ropath = os.path.join(dirpath, 'configoverride')
	if not os.path.exists(ropath):
		runopts = EXAMPLE_RUNOPTS_
		try:
			fout = open(ropath, 'w')
			fout.write(EXAMPLE_RUNOPTS_)
			fout.close()
		except Exception, e:
			sys.stderr.write(
				'tried to write basic run options to "%s" but failed: %s' % (ropath, e))
	else:
		runopts = open(ropath, 'r').read()
	datasets = []
	for name in os.listdir(dirpath):
		if name.endswith('_runfiles.tar.gz'):
			datasets.append(datafileLine(name))
	return page_template.substitute({
		'title': 'redistricter data and config',
		'datasets': ''.join(datasets),
		'config': config,
		'runopts': runopts})

def needsUpdate(dirpath, outpath):
	"""Check mod times of inputs against output. Return true if should generate output."""
	if newerthan(os.path.join(dirpath, 'client_config'), outpath):
		return True
	if newerthan(os.path.join(dirpath, 'configoverride'), outpath):
		return True
	for name in os.listdir(dirpath):
		if name.endswith('_runfiles.tar.gz'):
			if newerthan(name, outpath):
				return True
	return False

if __name__ == '__main__':
	outpath = os.path.abspath('index.html')
	if needsUpdate('.', outpath):
		out = open(outpath, 'w')
		out.write(makeHTMLBodyForDirectory())
		out.close()
		print 'wrote index.html'
	else:
		print 'no update needed'
