#!/usr/bin/python
#
# CGI script to receive solutions from distributed solution runners.

#import base64
import cgi
import cgitb
import os
import random
#import struct
import time

dest_dir = '/tmp/solutions'
debug = True
html = False

if debug:
	cgitb.enable()
else:
	# for production, log errors rather than displaying them.
	# TODO: better log dir?
	cgitb.enable(display=0, logdir="/tmp")


# random selection from these makes event id
SUFFIX_LETTERS_ = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0213456789'


now = time.time()
lt = time.localtime(now)
nowstr = time.strftime('%Y%m%d/%H%M%S', lt)
remote_addr = os.environ['REMOTE_ADDR']
rand = random.Random()
eventid = nowstr + '_' + remote_addr + '_' + rand.choice(SUFFIX_LETTERS_) + rand.choice(SUFFIX_LETTERS_) + rand.choice(SUFFIX_LETTERS_)

form = cgi.FieldStorage()

html = 'html' in form
solution = form.getfirst('solution')
user = form.getfirst('user')
statlog_gz = form.getfirst('statlog')
binlog = form.getfirst('binlog')
statsum = form.getfirst('statsum')


def copyout(source, dest):
	"""Copy from source.read() to dest.write() in 1e6 byte chunks."""
	d = source.read(1000000)
	while len(d) > 0:
		dest.write(d)


def paramToFile(name, var, outdir):
	if not var:
		return
	fout = open(os.path.join(outdir, name), 'wb')
	if type(var) >= str:
		fout.write(var)
	elif hasattr(var, 'file'):
		copyout(var.file, fout)
	else:
		fout.write(var.value)
	fout.close()


if solution:
	# Just a solution is enough. Store it.
	outdir = os.path.join(dest_dir, eventid)
	os.makedirs(outdir)
	paramToFile('solution', solution, outdir)
	paramToFile('user', user, outdir)
	paramToFile('binlog', binlog, outdir)
	paramToFile('statlog.gz', statlog_gz, outdir)
	paramToFile('statsum', statsum, outdir)
	status = 'ok'
else:
	status = 'no solution'


def falseOrLen(x):
	if x is None:
		return 'None'
	return str(len(x))


if not html:
	print """Content-Type: text/plain

"""
	print status
else:
	print """Content-Type: text/html

<!DOCTYPE html>
<html><head><title>solution submission</title></head><body bgcolor="#ffffff" text="#000000">
"""
	print '<p>' + status + '</p>'

if debug:
	if html:
		print '<pre>'
	print 'keys: ' + repr(form.keys())
	print 'eventid: ' + eventid;
	print 'solution: ' + falseOrLen(solution)
	print 'user: ' + falseOrLen(user)
	print 'statlog_gz: ' + falseOrLen(statlog_gz)
	print 'binlog: ' + falseOrLen(binlog)
	print 'statsum: ' + falseOrLen(statsum)
	keys = os.environ.keys()
	keys.sort()
	for k in keys:
		print 'os.environ[\'%s\'] = %s' % (k, os.environ[k])
	#print 'os.environ: ' + repr(os.environ)
	if html:
		print '</pre>'

if html:
	print '</body></html>'
