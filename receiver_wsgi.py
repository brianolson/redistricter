#!/usr/bin/python
#
# WSGI (PEP 0333) based upload receiver

import cgi
import cgitb
from cStringIO import StringIO
import os
import random
import sys
import tarfile
import time


# variables read from the environment
kSoldirEnvName = 'REDISTRICTER_SOLUTIONS'


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


def paramToFile(name, var, outdir):
	if not var:
		return
	fout = open(os.path.join(outdir, name), 'wb')
	if type(var) >= str:
		fout.write(var)
	elif hasattr(var, 'file'):
		d = var.file.read(1000000)
		while len(d) > 0:
			fout.write(d)
			d = var.file.read(1000000)
	else:
		fout.write(var.value)
	fout.close()


def falseOrLen(x):
	if x is None:
		return 'None'
	return str(len(x))


def stringTruth(x):
	"""Values 'f' 'false' '0' '' None (case independent) are False, anything else is True."""
	if x is None:
		return False
	if x == '':
		return False
	if x == '0':
		return False
	lx = x.lower()
	if lx == 'false':
		return False
	if lx == 'f':
		return False
	return True


class outFileSet(object):
	def __init__(self, outdir):
		self.outdir = outdir
		os.makedirs(self.outdir)
	
	def outParam(self, name, value):
		if not value:
			return
		paramToFile(name, value, self.outdir)
	
	def close(self):
		pass


class outTarfileSet(object):
	def __init__(self, outpath):
		outdir = os.path.dirname(outpath)
		if not os.path.isdir(outdir):
			os.makedirs(outdir)
		self.out = tarfile.open(outpath, 'w|gz')
	
	def outParam(self, name, value):
		if not value:
			return
		ti = tarfile.TarInfo()
		ti.name = name
		ti.size = len(value)
		ti.mode = 0444
		# TODO: set mtime from Last-Modified if present
		ti.mtime = time.time()
		ti.type = tarfile.REGTYPE
		sf = StringIO(value)
		self.out.addfile(ti, sf)
	
	def close(self):
		self.out.close()


def application(environ, start_response):
	form = cgi.FieldStorage(fp=environ.get('wsgi.input'), environ=environ)
	debug = stringTruth(form.getfirst('debug')) or stringTruth(environ.get('DEBUG'))
	if debug:
		# only allow debug for real debug situations
		addr = environ.get('REMOTE_ADDR')
		if addr and (addr != '127.0.0.1'):
			debug = False
	html = 'html' in form
	
	if debug:
		cgitb.enable()
	else:
		# for production, log errors rather than displaying them.
		# TODO: better log dir?
		cgitb.enable(display=0, logdir="/tmp")
	
	dest_dir = environ[kSoldirEnvName]
	
	solution = form.getfirst('solution')
	vars = form.getfirst('vars')
	statlog_gz = form.getfirst('statlog')
	binlog = form.getfirst('binlog')
	statsum = form.getfirst('statsum')
	
	remote_addr = environ.get('REMOTE_ADDR')
	eventid = makeEventId(remote_addr)
	
	headers = []
	if html:
		headers.append( ('Content-Type', 'text/html') )
	else:
		headers.append( ('Content-Type', 'text/plain') )
	
	if solution or binlog or statlog_gz:
		solsave = outTarfileSet(os.path.join(dest_dir, eventid + '.tar.gz'))
		# solsave = outFileSet(os.path.join(dest_dir, eventid))
		start_response('200 OK', headers)
		# TODO: write to tar file
		# Just a solution is enough. Store it.
		solsave.outParam('solution', solution)
		solsave.outParam('vars', vars)
		solsave.outParam('binlog', binlog)
		solsave.outParam('statlog.gz', statlog_gz)
		solsave.outParam('statsum', statsum)
		status = 'ok'
	else:
		start_response('400 bad request', headers)
		status = 'no solution|binlog|statlog.gz'
	outl = []
	if html:
		outl.append("""<!DOCTYPE html>
<html><head><title>solution submission</title></head><body bgcolor="#ffffff" text="#000000">
<p>%s</p>
""" % status)
	else:
		outl.append(status)
		outl.append('\n')
	if debug:
		if html:
			outl.append('<pre>\n')
		outl.append('eventid: ' + eventid + '\n')
		outl.append('solution: ' + falseOrLen(solution) + '\n')
		outl.append('vars: ' + falseOrLen(vars) + '\n')
		outl.append('statlog_gz: ' + falseOrLen(statlog_gz) + '\n')
		outl.append('binlog: ' + falseOrLen(binlog) + '\n')
		outl.append('statsum: ' + falseOrLen(statsum) + '\n')
		outl.append('keys: ' + repr(form.keys()) + '\n')
		keys = form.keys()
		keys.sort()
		for k in keys:
			v = form[k].value
			if len(v) > 50:
				vs = 'len(%d)' % len(v)
			else:
				vs = repr(v)
			outl.append('form[\'%s\'] = %s\n' % (k, vs))
		keys = environ.keys()
		keys.sort()
		for k in keys:
			outl.append('environ[\'%s\'] = %r\n' % (k, environ[k]))
		#outl.append('os.environ: ' + repr(environ))
		if html:
			outl.append('</pre>\n')

	if html:
		outl.append('</body></html>\n')
	return [''.join(outl)]


if __name__ == '__main__':
	import optparse
	import wsgiref.simple_server
	
	argp = optparse.OptionParser()
	argp.add_option('--host', dest='host', default='', help='Host: this responds to')
	argp.add_option('--port', '-p', dest='port', default=8080, type='int')
	argp.add_option('--solution-dir', dest='soldir', default=None)
	(options, args) = argp.parse_args()
	if options.soldir:
		os.environ[kSoldirEnvName] = options.soldir
	if kSoldirEnvName not in os.environ:
		os.environ[kSoldirEnvName] = '/tmp/solutions'
	httpd = wsgiref.simple_server.make_server(
		options.host,
		options.port,
		application)
#		wsgiref.simple_server.demo_app)
	httpd.base_environ[kSoldirEnvName] = os.environ[kSoldirEnvName]
	httpd.serve_forever()
