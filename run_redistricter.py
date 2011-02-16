#!/usr/bin/env python
# touch work/stop to gracefully stop with a cycle is done
# touch work/reload to make this script exec itself at that time
# These are also settable by buttons on the web interface.
# (these files are deleted after they're detected so that they only apply once)
#
# Some interesting options to pass to the underlying script after '--':
#--verbose
#--diskQuota=1000000000
#
# Additional arguments to the solver can be set in work/configoverride

import gzip
import optparse
import os
import select
import subprocess
import sys
import time

def read_flagfile(op, linesource, options=None, old_args=None):
	"""Return (options, args) as per OptionParser.parse_args()."""
	lines = []
	for line in linesource:
		line = line.strip()
		if not line:
			continue
		if line[0] == '#':
			continue
		lines.append(line)
	(options, args) = op.parse_args(args=lines, values=options)
	if old_args:
		args = old_args + args
	return (options, args)

def main():
	op = optparse.OptionParser()
	op.add_option('--port', dest='port', type='int', default=9988)
	op.add_option('--threads', dest='threads', type='int', default=2)
	op.add_option('--flagfile', dest='flagfile', default=None)
	(options, args) = op.parse_args()

	if options.flagfile:
		(options, args) = read_flagfile(op, open(options.flagfile, 'r'), options, args)
	elif os.path.exists('flagfile'):
		(options, args) = read_flagfile(op, open('flagfile', 'r'), options, args)
	rootdir = os.path.dirname(os.path.abspath(__file__))
	bindir = os.path.join(rootdir, 'bin')
	datadir = os.path.join(rootdir, 'data')
	workdir = os.path.join(rootdir, 'work')

	if not os.path.isdir(datadir):
		os.mkdir(datadir)
	if not os.path.isdir(workdir):
		os.mkdir(workdir)

	runallstates = os.path.join(bindir, 'runallstates.py')

	cmd = [runallstates,
	'--bestlog=bestlog',
	'--runlog=runlog',
	'--d2',
	'--fr=4/9',
	'--server=http://bdistricting.com/rd_datasets/',
	'--port=%d' % (options.port,),
	'--threads=%d' % (options.threads,),
	'--datadir=' + datadir,
	'--bindir=' + bindir] + args

	proc = subprocess.Popen(cmd, cwd=workdir, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	
	print 'status should be available on'
	print 'http://localhost:%d/' % (options.port,)
	
	log = RotatingLogWriter(os.path.join(workdir, 'dblog_'))
	piped_run(proc, log)
	log.close()

	reloadmarker = os.path.join(workdir, 'reload')
	if os.path.exists(reloadmarker):
		os.unlink(reloadmarker)
		os.execv(__file__, sys.argv)



class RotatingLogWriter(object):
	def __init__(self, prefix):
		self.prefix = prefix
		self.outname = None
		self.out = None
		self.currentDay = None
		self.lastMarkerTime = None
		
		now = time.localtime()
		self.startOut(now)
	
	def startOut(self, now):
		self.outname = self.prefix + ('%04d%02d%02d.gz' % (now[0], now[1], now[2]))
		self.currentDay = now[2]
		self.out = gzip.open(self.outname, 'ab', 9)
	
	def write(self, data):
		now = time.time()
		lnow = time.localtime(now)
		if (not self.lastMarkerTime) or ((now - self.lastMarkerTime) > 60):
			self.lastMarkerTime = now
			self.out.write(time.strftime('# %Y%m%d_%H%M%S\n', lnow))
		self.out.write(data)
		if lnow[2] != self.currentDay:
			self.out.close()
			self.startOut(lnow)
	
	def close(self):
		self.out.close()

def lastlines(arr, limit, line):
	"""Maintain up to limit lines in arr as fifo."""
	if len(arr) >= limit:
		start = (len(arr) - limit) + 1
		arr = arr[start:]
	arr.append(line)
	return arr

def poll_run(p, out):
	"""Read err and out from a process, copying to sys.stdout.
	Return last 10 lines of error in list."""
	poller = select.poll()
	poller.register(p.stdout, select.POLLIN | select.POLLPRI )
	poller.register(p.stderr, select.POLLIN | select.POLLPRI )
	lastolines = []
	lastelines = []
	while p.poll() is None:
		for (fd, event) in poller.poll(500):
			if p.stdout.fileno() == fd:
				line = p.stdout.readline()
				if line:
					out.write("O: " + line)
					lastlines(lastolines, 10, line)
			elif p.stderr.fileno() == fd:
				line = p.stderr.readline()
				if line:
					out.write("E: " + line)
					lastlines(lastelines, 10, line)
			else:
				out.write("? fd=%d\n" % (fd,))
	return (lastolines, lastelines)

def select_run(p, out):
	"""Read err and out from a process, copying to sys.stdout.
	Return last 10 lines of error in list."""
	lastolines = []
	lastelines = []
	while p.poll() is None:
		(il, ol, el) = select.select([p.stdout, p.stderr], [], [], 0.5)
		for fd in il:
			if (p.stdout.fileno() == fd) or (fd == p.stdout):
				line = p.stdout.readline()
				if line:
					out.write("O: " + line)
					lastlines(lastolines, 10, line)
			elif (p.stderr.fileno() == fd) or (fd == p.stderr):
				line = p.stderr.readline()
				if line:
					out.write("E: " + line)
					lastlines(lastelines, 10, line)
			else:
				out.write("? fd=%d\n" % (fd,))
	return (lastolines, lastelines)

has_poll = "poll" in dir(select)
has_select = "select" in dir(select)

def piped_run(p, out):
	"""Returns (outlines, errorlines)."""
	if has_poll:
		return poll_run(p, out)
	elif has_select:
		return select_run(p, out)



if __name__ == '__main__':
	main()
