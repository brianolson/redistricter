#!/usr/bin/python
#
# Run this in datasets dir:
# ${REDISTRICTER_BIN}/dataserver.py

import optparse
import os
import string
import sys
import time

try:
	from boto.s3.connection import S3Connection
	have_boto = True
	# host='commondatastorage.googleapis.com'
	# host='s3.amazonaws.com'
	def yieldS3Datasets(awsid, awskey, host, bucket, prefix):
		s3 = S3Connection(awsid, awskey, host=host)
		buck = s3.get_bucket(bucket)
		for k in buck.list(prefix=prefix):
			if k.name.endswith('_runfiles.tar.gz'):
				yield k.generate_url(expires_in=-1, query_auth=False, force_http=True)
except:
	have_boto = False
	def yieldS3Datasets(awsid, awskey, host, bucket, prefix):
		raise Exception('no boto')

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
<p>Generated at: ${gentime}</p>
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


def datasetListForDirectory(dirpath):
	"""Return HTML string."""
	datasets = []
	dsfiles = []
	for name in os.listdir(dirpath):
		if name.endswith('_runfiles.tar.gz'):
			dsfiles.append(name)
	dsfiles.sort()
	for name in dsfiles:
		datasets.append(datafileLine(name))
	return ''.join(datasets)


def datasetListForS3(options):
	"""Return HTML string."""
	datasets = []
	for url in yieldS3Datasets(options.awsid, options.awssecret, options.s3host, options.s3bucket, options.s3prefix):
		name = url.rsplit('/', 1)[1]
		line = '<div><a class="data" href="%s">%s</a></div>\n' % (url, name)
		datasets.append(line)
	return ''.join(datasets)


def makeHTMLBodyForDirectory(dirpath='.', datasets=''):
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
	return page_template.substitute({
		'title': 'redistricter data and config',
		'datasets': datasets,
		'config': config,
		'gentime': time.asctime(),
		'runopts': runopts})

def needsUpdate(dirpath, outpath):
	"""Check mod times of inputs against output. Return true if should generate output."""
	if newerthan(__file__, outpath):
		return True
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
	argp = optparse.OptionParser()
	argp.add_option('--dir', dest='dir', default='.', help='where to find datasets and config files')
	argp.add_option('--out', dest='out', default='index.html', help='file name to write config to, default=index.html')
	argp.add_option('--awsid', dest='awsid', default=os.environ.get('AWS_ID'))
	argp.add_option('--awssecret', dest='awssecret', default=os.environ.get('AWS_SECRET'))
	argp.add_option('--s3host', dest='s3host', default='s3.amazonaws.com')
	argp.add_option('--s3bucket', dest='s3bucket', default=None, help='bucket/prefix*_runfiles.tar.gz')
	argp.add_option('--s3prefix', dest='s3prefix', default='datasets', help='bucket/prefix*_runfiles.tar.gz')
	argp.add_option('--force', dest='force', default=False, action='store_true')
	(options, args) = argp.parse_args()
	outpath = os.path.abspath(options.out)
	if options.force or needsUpdate(options.dir, outpath):
		out = open(outpath, 'w')
		if options.s3bucket:
			datasets = datasetListForS3(options)
		else:
			datasets = datasetListForDirectory(options.dir)
		out.write(makeHTMLBodyForDirectory(options.dir, datasets))
		out.close()
		print 'wrote: %s' % outpath
	else:
		print 'no update needed'
