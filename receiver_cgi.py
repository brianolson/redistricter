#!/usr/bin/python
#
# CGI script to receive solutions from distributed solution runners.
#
# TODO: store all received data per submission in one tar file.

#import base64
import cgi
import cgitb
import os
import random
import sys
import time


# random selection from these makes event id
SUFFIX_LETTERS_ = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0213456789'


rand = random.Random()


def makeEventId(remote_addr=None, now=None):
	now = time.time()
	lt = time.localtime(now)
	nowstr = time.strftime('%Y%m%d/%H%M%S', lt)
	outparts = [nowstr, '_']
	if remote_addr:
		outparts.append(str(remote_addr))
		outparts.append('_')
	outparts.append(rand.choice(SUFFIX_LETTERS_))
	outparts.append(rand.choice(SUFFIX_LETTERS_))
	outparts.append(rand.choice(SUFFIX_LETTERS_))
	return ''.join(outparts)


def copyout(source, dest):
	"""Copy from source.read() to dest.write() in 1e6 byte chunks."""
	d = source.read(1000000)
	while len(d) > 0:
		dest.write(d)
		d = source.read(1000000)


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


def falseOrLen(x):
	if x is None:
		return 'None'
	return str(len(x))


def printOut(x):
	sys.stdout.write(x)


def main(input, environ, out=printOut):
	dest_dir = environ.get('REDISTRICTER_SOLUTIONS')
	if not dest_dir:
		dest_dir = '/tmp/solutions'
	# TODO: take ?debug=true&html=false
	form = cgi.FieldStorage(fp=input, environ=environ)
	#debug = 'debug' in form
	debug = True
	html = 'html' in form
	
	if debug:
		cgitb.enable()
	else:
		# for production, log errors rather than displaying them.
		# TODO: better log dir?
		cgitb.enable(display=0, logdir="/tmp")
	
	solution = form.getfirst('solution')
	user = form.getfirst('user')
	statlog_gz = form.getfirst('statlog')
	binlog = form.getfirst('binlog')
	statsum = form.getfirst('statsum')
	
	remote_addr = environ.get('REMOTE_ADDR')
	eventid = makeEventId(remote_addr)
	
	if solution:
		# TODO: write to tar file
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

	if not html:
		out("""Content-Type: text/plain

	""")
		out(status)
	else:
		out("""Content-Type: text/html

	<!DOCTYPE html>
	<html><head><title>solution submission</title></head><body bgcolor="#ffffff" text="#000000">
	""")
		out('<p>' + status + '</p>\n')

	if debug:
		if html:
			out('<pre>\n')
		out('keys: ' + repr(form.keys()) + '\n')
		out('eventid: ' + eventid + '\n')
		out('solution: ' + falseOrLen(solution) + '\n')
		out('user: ' + falseOrLen(user) + '\n')
		out('statlog_gz: ' + falseOrLen(statlog_gz) + '\n')
		out('binlog: ' + falseOrLen(binlog) + '\n')
		out('statsum: ' + falseOrLen(statsum) + '\n')
		keys = form.keys()
		keys.sort()
		for k in keys:
			out('form[\'%s\'] = %s\n' % (k, form[k]))
		keys = environ.keys()
		keys.sort()
		for k in keys:
			out('environ[\'%s\'] = %s\n' % (k, environ[k]))
		#out('os.environ: ' + repr(environ))
		if html:
			out('</pre>\n')

	if html:
		out('</body></html>\n')

if __name__ == '__main__':
	main(sys.stdin, os.environ)
