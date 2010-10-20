#!/usr/bin/python
#
# Put datasets to s3 using boto library.
# Oh look, it's like rsync all over again.
#
# boto library can read AWS config from env AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY

import glob
import logging
import optparse
import os
import time

from boto.s3.connection import S3Connection

def getDatasetNamePart(x):
	if not x.endswith('_runfiles.tar.gz'):
		return None
		#raise Exception('bad dataset path: %s' % x)
	return x.rsplit('/', 1)[1]


if __name__ == '__main__':
	argp = optparse.OptionParser()
	argp.add_option('--dir', dest='dir', default='.', help='where to find datasets and config files')
	argp.add_option('--awsid', dest='awsid', default=None)
	argp.add_option('--awssecret', dest='awssecret', default=None)
	argp.add_option('--s3host', dest='s3host', default=None)
	argp.add_option('--s3bucket', dest='s3bucket', default=None, help='bucket/prefix*_runfiles.tar.gz')
	argp.add_option('--s3prefix', dest='s3prefix', default='data/', help='bucket/prefix*_runfiles.tar.gz')
	argp.add_option('--force', dest='force', default=False, action='store_true')
	argp.add_option('--verbose', '-v', dest='verbose', default=True)
	(options, args) = argp.parse_args()
	if options.verbose:
		logging.basicConfig(level=logging.DEBUG)
	files = []
	for arg in args:
		if arg.endswith('_runfiles.tar.gz') and os.path.exists(arg):
			files.append(arg)
		else:
			raise Exception('bogus arg "%s" not a file or valid option' % arg)
	if not files:
		files = glob.glob(os.path.join(options.dir, '*_runfiles.tar.gz'))
	logging.debug('files: %r', files)
	if not files:
		raise Exception('no dataset files found')
	s3args = {}
	if options.s3host:
		s3args['host'] = options.s3host
	if options.awsid:
		s3args['aws_access_key_id'] = options.awsid
	if options.awssecret:
		s3args['aws_secret_access_key'] = options.awssecret
	s3 = S3Connection(**s3args)
	buck = s3.get_bucket(options.s3bucket)
	current = {}
	for tk in buck.list(options.s3prefix):
		dsname = getDatasetNamePart(tk.name)
		if not dsname:
			logging.debug('skip: %s', tk.name)
			continue
		logging.debug('found existing: %s', tk.name)
		current[dsname] = tk
	toput = []
	for fname in files:
		name = getDatasetNamePart(fname)
		skey = current.get(name)
		if not skey:
			# not there, put it
			toput.append((fname, options.s3prefix + name))
		else:
			# TODO: this does timezones wrong
			smodtime = time.mktime(time.strptime(
				skey.last_modified.split('.')[0], '%Y-%m-%dT%H:%M:%S'))
			st = os.stat(fname)
			if smodtime < st.st_mtime:
				# local is newer, put it
				toput.append((fname, options.s3prefix + name))
			else:
				logging.debug('remote %s is newer than local', name)
	for fname, remoteName in toput:
		logging.info('put "%s" -> "%s"', fname, remoteName)
		nk = buck.new_key(remoteName)
		nk.set_contents_from_filename(fname, policy='public-read')
