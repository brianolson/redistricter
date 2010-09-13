#!/usr/bin/python

import os
import string
import sys

page_template = string.Template("""<!doctype html>
<html><head><title>${title}</title><head><body>
<h1>data sets</h1>
${datasets}
<h1>config</h1>
<pre class="config">${config}</pre>
</body></html>
""")


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
	config = open(os.path.join(dirpath, 'client_config'), 'r').read()
	datasets = []
	for name in os.listdir(dirpath):
		if name.endswith('_runfiles.tar.gz'):
			datasets.append(datafileLine(name))
	return page_template.substitute(
		title='redistricter data and config',
		datasets=''.join(datasets),
		config=config)

if __name__ == '__main__':
	sys.stdout.write(makeHTMLBodyForDirectory())
