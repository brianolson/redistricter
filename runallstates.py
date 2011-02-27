#!/usr/bin/python

"""Run the solver repeatedly, storing results, tracking the best.

Uses compiled data setup by the setupstatedata.py script.
Uses the `districter2` solver binary.
Reads ${REDISTRICTER_DATA}/??/config/*
There may be config/Congress config/Assembly and config/Senate, describing
a state's US House and state legislature districting needs.

In the data it is run in, this script creates ??_*/ directories
(like CA_Congress) for results to be stored in. Each such configuration
contains timestamped directories YYYYMMDD_HHMMSS/ for each run of the solver.
(Many runs are needed to try various random solution starts.)
The best solution will be sym-linked to by XX_Foo/link1
XX_Foo/best/index.html will show the best 10 results so far.

Inside each XX_Foo/YYYYMMDD_HHMMSS/ run directory should be:
  statlog.gz recording statistics from the solver at many points through the run.
  statsum which is a few key lines out of statlog
  *.dsz solution files
  *.png images of solutions

./runallstates.py --help should be helpful too.
"""

__author__ = "Brian Olson"

# TODO: shiny GUI?
# At the most basic, curses or Tk progress bars.
# Or, render a recent map and display it.
# Maybe rewrite this run script in C++ and build it into the districter binary?
# (But I get free leak-resistance by not having a long-lived program,
# and pickle is kinda handy.)
# For now, there's the web interface, and it may stay that way.
# There should probably be a doubleclickable Mac runner.

import cPickle as pickle
import datetime
import glob
import logging
import optparse
import os
import random
import re
import select
import stat
import subprocess
import sys
import tarfile
import threading
import time
import traceback

# local imports
import client
import manybest

has_poll = "poll" in dir(select)
has_select = "select" in dir(select)

# won't download more data if this doesn't fit in quota.
QUOTA_HEADROOM_ = 20000000

def readUname():
	p = subprocess.Popen(['uname'], stdout=subprocess.PIPE)
	p.wait()
	return p.stdout.read()

# TODO: maybe a .dll case for cygwin?
# This is only needed for running llvm bitcode through lli.
uname = readUname().strip()
if uname == 'Darwin':
	library_suffix = '.dylib'
else:
	library_suffix = '.so'

if (not has_poll) and (not has_select):
	sys.stderr.write(
"""Lacking both select.poll() and select.select().
No good way to get data back from subprocess.
Upgrade your Python?
""")
	sys.exit(1)

#logging.basicConfig(level=logging.DEBUG)

def IsExecutableDir(path):
	return os.path.isdir(path) and os.access(path, os.X_OK)

def IsExecutableFile(path):
	return os.path.isfile(path) and os.access(path, os.X_OK)

def IsReadableFile(path):
	return os.access(path, os.R_OK)

def timestamp():
	now = datetime.datetime.now()
	return "%04d%02d%02d_%02d%02d%02d" % (now.year, now.month, now.day,
		now.hour, now.minute, now.second)

SIZE_STRING_RE_ = re.compile(r'([0-9]+)([kmg]?b?)', re.IGNORECASE)

def sizeStringToInt(x, default=None):
	m = SIZE_STRING_RE_.match(x)
	if not m:
		logging.warn('could not parse size string "%s"', x)
		return default
	base = int(m.group(1))
	scalestr = m.group(2).lower()
	if (not scalestr) or (scalestr == 'b'):
		return base
	if scalestr[0] == 'k':
		return base * 1000
	if scalestr[0] == 'm':
		return base * 1000000
	if scalestr[0] == 'g':
		return base * 1000000000
	logging.warn('did not undestand suffix to size string "%s"', x)
	return default

def getNiceCommandFragment():
	if IsExecutableFile("/bin/nice"):
		return "/bin/nice -n 20 "
	elif IsExecutableFile("/usr/bin/nice"):
		return "/usr/bin/nice -n 20 "
	return ""

def getNiceCommandArgs():
	if IsExecutableFile("/bin/nice"):
		return ["/bin/nice", "-n", "20"]
	elif IsExecutableFile("/usr/bin/nice"):
		return ["/usr/bin/nice", "-n", "20"]
	return []

nice = getNiceCommandFragment()
niceArgs = getNiceCommandArgs()

def lastlines(arr, limit, line):
	"""Maintain up to limit lines in arr as fifo."""
	if len(arr) >= limit:
		start = (len(arr) - limit) + 1
		arr = arr[start:]
	arr.append(line)
	return arr

def poll_run(p, stu):
	"""Read err and out from a process, copying to sys.stdout.
	Return last 10 lines of error in list."""
	poller = select.poll()
	poller.register(p.stdout, select.POLLIN | select.POLLPRI )
	poller.register(p.stderr, select.POLLIN | select.POLLPRI )
	lastolines = []
	lastelines = []
	pollx = p.poll()
	while pollx is None:
		for (fd, event) in poller.poll(500):
			if p.stdout.fileno() == fd:
				line = p.stdout.readline()
				if line:
					sys.stdout.write("O " + stu + ": " + line)
					lastlines(lastolines, 10, line)
			elif p.stderr.fileno() == fd:
				line = p.stderr.readline()
				if line:
					sys.stdout.write("E " + stu + ": " + line)
					lastlines(lastelines, 10, line)
			else:
				sys.stdout.write("? %s fd=%d\n" % (stu, fd))
		pollx = p.poll()
	sys.stderr.write('ended with %r\n' % (pollx,))
	return (lastolines, lastelines)

def select_run(p, stu):
	"""Read err and out from a process, copying to sys.stdout.
	Return last 10 lines of error in list."""
	lastolines = []
	lastelines = []
	pollx = p.poll()
	while pollx is None:
		(il, ol, el) = select.select([p.stdout, p.stderr], [], [], 0.5)
		for fd in il:
			if (p.stdout.fileno() == fd) or (fd == p.stdout):
				line = p.stdout.readline()
				if line:
					sys.stdout.write("O " + stu + ": " + line)
					lastlines(lastolines, 10, line)
			elif (p.stderr.fileno() == fd) or (fd == p.stderr):
				line = p.stderr.readline()
				if line:
					sys.stdout.write("E " + stu + ": " + line)
					lastlines(lastelines, 10, line)
			else:
				sys.stdout.write("? %s fd=%s\n" % (stu, fd))
		pollx = p.poll()
	sys.stderr.write('ended with %r\n' % (pollx,))
	return (lastolines, lastelines)

HAS_PARAM_ = {
	'-P': True,
	'-B': True,
	'--pngout': True,
	'-d': True,
	'-i': True,
	'-U': True,
	'-g': True,
	'-o': True,
	'-r': True,
	'--loadSolution': True,
	'--distout': True,
	'--coordout': True,
	'--pngW': True,
	'--pngH': True,
	'--binLog': True,
	'--statLog': True,
	'--sLog': True,
	'--pLog': True,
	'--oldCDs': False,
	'--blankDists': False,
	'-q': False,
	'--popRatioFactor': True,
	'--popRatioFactorEnd': True,
	'--popRatioFactorPoints': True,
	'--nearest-neighbor': False,
	'--d2': False,
	'--maxSpreadFraction': True,
	'--maxSpreadAbsolute': True,
	'--mppb': True,
	'--runDutySeconds': True,
	'--sleepDutySeconds': True,
}


def parseArgs(x):
	"""Parse a string into {arg, value}.
	TODO, handle shell escaping rules."""
	arg = None
	out = {}
	for part in x.split():
		if '=' in part:
			(a, v) = part.split('=', 1)
			out[a] = v
			continue
		elif arg is not None:
			out[arg] = part
			arg = None
			continue
		elif part not in HAS_PARAM_:
			logging.warn('parseArgs part "%s" not a known param, assuming it is argument-less', part)
			out[part] = None
		elif HAS_PARAM_[part]:
			arg = part
		else:
			out[part] = None
	return out


def dictToArgList(x):
	out = []
	for arg, value in x.iteritems():
		out.append(arg)
		if value is not None:
			out.append(value)
	return out


def argListToDict(they):
	i = 0
	out = {}
	while i < len(they):
		if HAS_PARAM_[they[i]]:
			out[they[i]] = they[i+1]
			i += 2
		else:
			out[they[i]] = None
			i += 1
	return out


class ParseError(Exception):
	pass


def AfterStartswith(line, prefix):
	if line.startswith(prefix):
		return line[len(prefix):]
	return None


class configuration(object):
	"""
	Get basic args from:
	1. $configdir/${name}_args
	2. $datadir/geometry.pickle
	#3. $datadir/basicargs

	Get drend command from:
	1. $configdir/${name}_drendcmd
	2. $datadir/geometry.pickle
	#3. $datadir/drendcmd
	"""
	def __init__(self, name=None, datadir=None, config=None, dataroot=None):
		# name often upper case 2 char code
		self.name = name
		self.datadir = datadir
		self.drendargs = {}
		self.args = {}
		self.enabled = None
		self.geom = None
		self.path = config
		self.dataroot = dataroot
		self.kmppSendThreshold = None
		self.spreadSendThreshold = None
		self.sendAnything = False
		# weight into chance of running randomly selected
		self.weight = 1.0
		# readtime can be used to re-read changed config files
		self.readtime = None
		if name is None:
			if datadir is None:
				raise Exception('one of name or datadir must not be None')
			m = configuration.datadir_state_re.match(datadir)
			if m is None:
				raise Exception('failed to get state name from datadir path "%s"' % datadir)
			self.name = m.group(1)
		# set some basic args now that name is set.
		self.args = {'-o': self.name + '_final.dsz'}
		self.drendargs = {'--loadSolution': 'link1/bestKmpp.dsz',
			'--pngout': 'link1/%s_final2.png' % self.name}
		if self.datadir is not None:
			ok = self.readDatadirConfig()
			assert ok
		if config is not None:
			self.readConfigFile(config, dataroot)
	
	def __str__(self):
		enstr = 'disabled'
		if self.isEnabled():
			enstr = 'enabled w=' + str(self.weight)
		return '(%s %s %s %s)' % (self.name, enstr, self.datadir, self.args)
	
	def setRootDatadir(self, root):
		"""Replace $DATA with root in all args."""
		for arg, value in self.args.iteritems():
			if value:
				self.args[arg] = value.replace('$DATA', root)
		for arg, value in self.drendargs.iteritems():
			if value:
				self.drendargs[arg] = value.replace('$DATA', root)
		if self.datadir:
			self.datadir = self.datadir.replace('$DATA', root)

	datadir_state_re = re.compile(r'.*/([a-zA-Z]{2})/?$')

	def isEnabled(self):
		return (self.enabled is None) or self.enabled
	
	def applyConfigLine(self, line, dataroot=None):
		"""Alter the config:
		solve: arguments for solver
		drend: arguments for drend
		common: arguments for any tool
		datadir: path to datadir. Immediately reads config in datadir.
		datadir!: path to datadir. does nothing.
		weight: How often this config should run relative to others.
		kmppSendThreshold: only send solutions at least this good.
		spreadSendThreshold: only send solutions at least this good.
		sendAnything   -- Send even invalid results.
		enabled
		disabled

		$DATA is substituted with the global root datadir
		empty lines and lines starting with '#' are ignored.
		"""
		line = line.strip()
		if (len(line) == 0) or line.startswith('#'):
			return True
		if dataroot is None:
			dataroot = self.dataroot
		if dataroot is not None:
			line = line.replace('$DATA', dataroot)
		if line.startswith('solve:'):
			self.args.update(parseArgs(line[6:].strip()))
		elif line.startswith('drend:'):
			self.drendargs.update(parseArgs(line[6:].strip()))
		elif line.startswith('common:'):
			newargs = parseArgs(line[7:].strip())
			self.args.update(newargs)
			self.drendargs.update(newargs)
		elif line.startswith('datadir!:'):
			self.datadir = line[9:].strip()
		elif line.startswith('datadir:'):
			old_datadir = self.datadir
			self.datadir = line[8:].strip()
			logging.debug('old datadir=%s new datadir=%s', old_datadir, self.datadir)
			if old_datadir != self.datadir:
				if not self.readDatadirConfig():
					raise ParseError('problem with datadir "%s"' % self.datadir)
		elif line.startswith('weight:'):
			self.weight = float(line[7:].strip())
		elif line.startswith('kmppSendThreshold:'):
			self.kmppSendThreshold = float(line[18:].strip())
		elif line.startswith('spreadSendThreshold:'):
			self.spreadSendThreshold = float(line[20:].strip())
		elif line.startswith('sendAnything'):
			self.sendAnything = line.lower() != 'sendanything: false'
		elif line == 'enable' or line == 'enabled':
			self.enabled = True
		elif line == 'disable' or line == 'disabled':
			self.enabled = False
		else:
			raise ParseError('bogus config line: "%s"\n' % line)
		return True
	
	def readConfigFile(self, x, dataroot=None):
		"""x is a path or a file like object iterable over lines.
		See applyConfigLine(line,dataroot=None) for details on
		config line formats.
		empty lines and lines starting with '#' are ignored.
		"""
		if isinstance(x, basestring):
			self.path = x
			self.readtime = time.time()
			x = open(x, 'r')
		for line in x:
			self.applyConfigLine(line, dataroot)

	def rereadIfUpdated(self):
		if not self.path:
			return
		cf_stat = os.stat(self.path)
		if self.readtime < cf_stat.st_mtime:
			self.readtime = cf_stat.st_mtime
			f = open(self.path, 'r')
			for line in f:
				self.applyConfigLine(line, self.dataroot)

	def readDatadirConfig(self, datadir=None):
		"""Read a datadir/geometry.pickle and calculate basic config.
		datadir is path to state dir, "data/MN" or so.
		"""
		if datadir is None:
			assert self.datadir is not None
		else:
			self.datadir = datadir
		if not os.path.isdir(self.datadir):
			return False
		m = configuration.datadir_state_re.match(self.datadir)
		if m is None:
			sys.stderr.write(
				'could not parse a state code out of datadir "%s"\n' % self.datadir)
			return False
		statecode = m.group(1)
		stl = statecode.lower()
		if os.path.exists(os.path.join(self.datadir, 'norun')):
			self.enabled = False
		
		# set some basic args based on any datadir
		datapath = os.path.join(self.datadir, stl + '.pb')
		pxpath = os.path.join(self.datadir, statecode + '.mppb')
		datadir_args = {'-P': datapath}
		logging.debug('datadir args %r', datadir_args)
		datadir_args.update(self.args)
		self.args = datadir_args
		logging.debug('merged args %r', self.args)
		datadir_drend_args = {'-P': datapath, '--mppb': pxpath}
		logging.debug('geom drend args %r', datadir_drend_args)
		datadir_drend_args.update(self.drendargs)
		self.drendargs = datadir_drend_args
		logging.debug('merged drend args %r', self.drendargs)
		logging.debug('readDatadirConfig: %s', self)
		return True


def ignoreFile(cname):
	return cname.startswith('.') or cname.startswith('#') or cname.endswith('~') or cname.endswith(',v')


def getDefaultBindir():
	bindir = os.environ.get('REDISTRICTER_BIN')
	if bindir is None:
		bindir = os.path.dirname(os.path.abspath(__file__))
	return bindir


def getDefaultDatadir(bindir=None):
	datadir = os.environ.get("REDISTRICTER_DATA")
	if datadir is None:
		if bindir is None:
			bindir = getDefaultBindir()
		datadir = os.path.join(bindir, "data")
	return datadir


def duFile(path):
	st = os.stat(path)
	if hasattr(st, 'st_blocks'):
		return st.st_blocks * 512
	else:
		return st.st_size


def du(dir):
	sum = 0
	for path, dirnames, filenames in os.walk(dir):
		for d in dirnames:
			sum += duFile(os.path.join(path, d))
		for f in filenames:
			sum += duFile(os.path.join(path, f))
	return sum


def htmlEscape(x):
	return x.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


class runallstates(object):
	def __init__(self):
		self.bindir = getDefaultBindir()
		self.datadir = getDefaultDatadir(self.bindir)
		self.exe = None
		self.solverMode = {}
		self.d2args = {'--d2': None, '--popRatioFactorPoints': '0,1.4,30000,1.4,80000,500,100000,50,120000,500', '-g': '150000'}
		self.stdargs = {'--blankDists': None, '--statLog': 'statlog', '--binLog': 'binlog', '--maxSpreadFraction': '0.01'}
		self.start = time.time();
		self.end = None
		# built from argv, things we'll run
		self.statearglist = []
		# TODO: detect number of cores and set threads automatically
		# TODO: duty-cycle one thread down to fractional load
		self.numthreads = 1
		self.dry_run = False
		self.verbose = 1
		# added with --config
		self.configArgList = []
		# added with --configdir
		self.configdir = None
		# map from STU/config-name to configuration objects
		self.config = {}
		# list of regex to check against config file path
		self.config_include = []
		self.config_exclude = []
		self.config_override_path = None
		self.config_override_lastload = None
		# list of STU uppercase state abbreviations
		self.states = []
		self.softfail = False
		self.stoppath = None
		self.stopreason = ''
		self.runlog = None  # file
		self.bestlog = None  # file
		self.bests = None  # map<stu, manybest.slog>
		# used by getNextState and addStopReason
		self.lock = None
		# used by getNextState and runthread
		self.qpos = 0
		# dict from optparse
		self.options = {}
		# What is currently running
		self.currentOps = {}
		# number of errors that is allowed per ...
		self.errorRate = 0
		# number of last runs to count errors over
		self.errorSample = 1
		# array of int of length errorSample. 1=success. sum must be >= (errorSample - errorRate)
		self.runSuccessHistory = []
		# Client object for referring to getting data from server.
		self.client = None
		# For local cache when running as a client
		self.diskQuota = 100000000
		self.diskUsage = None
		self.runtimeLoadDataNext = time.time() + 120

	def addStopReason(self, reason):
		if self.lock:
			self.lock.acquire()
		self.stopreason += reason
		if self.lock:
			self.lock.release()

	def getNextWeightedRandomState(self):
		weightedStu = []
		totalweight = 0.0
		for stu in self.states:
			conf = self.config[stu]
			if conf.isEnabled():
				weight = conf.weight
				weightedStu.append( (weight, stu) )
				totalweight += weight
		pick = random.random() * totalweight
		for weight, stu in weightedStu:
			pick -= weight
			if pick <= 0:
				return stu
		raise Excption('internal error, weights shifted.')

	def getNextState(self):
		if self.options.weighted:
			return self.getNextWeightedRandomState()
		if self.lock:
			self.lock.acquire()
		origQpos = self.qpos
		stu = self.states[self.qpos]
		self.qpos = (self.qpos + 1) % len(self.states)
		# check isEnabled at this time instead of setup so that it can be gotten dynamically from configoverrides
		while not self.config[stu].isEnabled():
			stu = self.states[self.qpos]
			self.qpos = (self.qpos + 1) % len(self.states)
			if self.qpos == origQpos:
				if self.lock:
					self.lock.release()
				raise Exception('there are no enabled states')
		if self.lock:
			self.lock.release()
		return stu
	
	def doBestlog(self, stu, mb):
		if self.bestlog is None:
			return
		if not mb.they:
			return
		best = mb.they[0]
		oldbest = self.bests.get(stu)
		# TODO: if old best is still current and hasn't been sent, re-try send
		if (oldbest is None) or (best.kmpp < oldbest.kmpp):
			self.bests[stu] = best
			if oldbest is None:
				oldmsg = "was none"
			else:
				oldmsg = "old=%f" % oldbest.kmpp
			outf = self.bestlog
			if self.dry_run:
				outf = sys.stderr
			if best.spread is not None:
				spreadstr = str(best.spread)
			else:
				spreadstr = ''
			outf.write("%s\t%f\t%s\t%s\t%s\t%s\n" % (
				stu,
				best.kmpp,
				os.path.join(stu, best.root),
				datetime.datetime.now().isoformat(" "),
				oldmsg,
				spreadstr))
			outf.flush()
	
	def readBestLog(self, path):
		"""Read in a bestlog, loading latest state into memory."""
		if not os.path.exists(path):
			return {}
		rbestlog = open(path, "r")
		bests = {}
		for line in rbestlog:
			try:
				a = line.split("\t")
				stu = a[0]
				if len(a) >= 6:
					spread = int(a[5])
				else:
					srpead = None
				x = manybest.slog("", float(a[1]), spread, "", "")
				bests[stu] = x
			except Exception, e:
				sys.stderr.write("readBestLog error: %s\n" % e)
		rbestlog.close()
		return bests
		
	def openBestLog(self, path):
		"""Open a bestlog. Read in state, then append."""
		try:
			self.bests = self.readBestLog(path)
		except Exception, e:
			sys.stderr.write("readBestLog failed: %s\n" % e)
		if self.bests is None:
			self.bests = {}
		self.bestlog = open(path, "a")

	def openRunLog(self, path):
		"""Open for appending a log of things run."""
		self.runlog = open(path, "a")
	
	def readArgs(self, argv):
		default_server = os.environ.get('REDISTRICTER_SERVER')
		argp = optparse.OptionParser(description="arguments after '--' will be passed to the solver")
		# TODO: add options to not make PNG after run or to not store g/ intermediates.
		argp.add_option('-d', '--data', '--datadir', dest='datadir', default=self.datadir)
		argp.add_option('--bindir', '--bin', dest='bindir', default=self.bindir)
		argp.add_option('--exe', dest='exepath', default=None)
		argp.add_option('--runsecs', dest='runsecs', type='float', default=None)
		argp.add_option('--config', dest='configList', action='append', default=[])
		argp.add_option('--config-include', dest='config_include', action='append', default=[])
		argp.add_option('--config-exclude', dest='config_exclude', action='append', default=[])
		argp.add_option('--configdir', dest='configdir', default=None)
		argp.add_option('--config-override', dest='config_override_path', default='configoverride')
		argp.add_option('--threads', dest='threads', type='int', default=1)
		argp.add_option('--port', dest='port', type='int', default=-1, help='port to serve stats on via HTTP')
		argp.add_option('--dry-run', '-n', dest='dry_run', action='store_true', default=False)
		argp.add_option('--mode', dest='mode', type='choice', choices=('d2','nn'), default='nn')
		argp.add_option('--d2', dest='mode', action='store_const', const='d2')
		argp.add_option('--nn', dest='mode', action='store_const', const='nn')
		argp.add_option('--runlog', dest='runlog', default='runlog', help='append a record of all solver runs here')
		argp.add_option('--bestlog', dest='bestlog', default='bestlog', help='append a record of each solver run that is best-so-far')
		argp.add_option('--keepbest', dest='keepbest', default=2, type='int', help='number of best solutions to keep')
		argp.add_option('--solutionlog', dest='solutionlog', default=False, action='store_true', help='store intermediate solutions under g/*')
		argp.add_option('--nosolutionlog', dest='solutionlog', action='store_false')
		argp.add_option('--server', dest='server', default=default_server, help='url of config page on server from which to download data')
		argp.add_option('--force-config-reload', dest='force_config_reload', action='store_true', default=False)
		argp.add_option('--verbose', '-v', dest='verbose', action='store_true', default=False)
		argp.add_option('--weighted', dest='weighted', action='store_true', default=True, help='Pick weighted-random configurations to run next. (default)')
		argp.add_option('--round-robin', dest='weighted', action='store_false', help='Run each configuration in turn.')
		argp.add_option('--failuresPerSuccessesAllowed', '--fr', dest='failureRate', default='3/7', help='f/s checks the last (f+s) events and exits if >f are failures')
		argp.add_option('--diskQuota', dest='diskQuota', default='100M', help='how much disk to use storing data and results. Default 100M.')
		#argp.add_option('', dest='', default=None, help='')
		(options, args) = argp.parse_args()
		self.options = options
		if options.verbose:
			logging.getLogger().setLevel(logging.DEBUG)
		if options.diskQuota:
			self.diskQuota = sizeStringToInt(
				options.diskQuota, self.diskQuota)
		extraArgs = []
		if '--' in args:
			# append args after '-' to self.stdarg via argListToDict()
			ddindex = args.index('-')
			extra = args[ddindex + 1:]
			args = args[:ddindex]
			self.stdargs.update(argListToDict(extraArgs))
		for arg in args:
			astu = arg.upper()
			#if IsReadableFile(os.path.join(self.datadir, astu, "basicargs")) or IsReadableFile(os.path.join(self.datadir, astu, "geometry.pickle")):
			if (os.path.isdir(os.path.join(self.datadir, astu)) or
				self.options.server):
				# Any existing directory, or if there's a server we might fetch it.
				logging.debug('add stu "%s"', astu)
				self.statearglist.append(astu)
			else:
				extraArgs.append(arg)
				#sys.stderr.write("%s: bogus arg \"%s\" of %r\n" % (argv[0], arg, args))
				#sys.exit(1)
		if extraArgs:
			logging.info('passing extra args to solver: %r', extraArgs)
			self.stdargs.update(argListToDict(extraArgs))
		self.datadir = os.path.realpath(options.datadir)
		options.datadir = self.datadir
		self.bindir = os.path.realpath(options.bindir)
		options.bindir = self.bindir
		if options.exepath:
			self.exe = [os.path.realpath(options.exepath)]
		if options.runsecs is not None:
			self.end = self.start + options.runsecs
		self.configArgList = options.configList
		self.configdir = options.configdir
		self.config_override_path = options.config_override_path
		self.numthreads = options.threads
		self.dry_run = options.dry_run
		if options.mode == 'd2':
			self.solverMode = self.d2args
		if options.runlog is not None:
			self.openRunLog(options.runlog)
		if options.bestlog is not None:
			self.openBestLog(options.bestlog)
		for pattern in options.config_include:
			self.config_include.append(re.compile(pattern))
		for pattern in options.config_exclude:
			self.config_exclude.append(re.compile(pattern))
		if options.failureRate:
			(f,s) = options.failureRate.split('/')
			self.errorRate = int(f)
			self.errorSample = self.errorRate + int(s)
			self.runSuccessHistory = [1 for x in xrange(self.errorSample)]
	
	def logCompletion(self, succeede):
		"""Log a result, return if we should quit."""
		self.runSuccessHistory.append(succeede)
		while len(self.runSuccessHistory) > self.errorSample:
			self.runSuccessHistory.pop(0)
		return sum(self.runSuccessHistory) < (self.errorSample - self.errorRate)
	
	def checkSetup(self):
		if self.exe is None:
			bitcodepath = os.path.join(self.bindir, "districter2.bc")
			if os.path.exists(bitcodepath):
				# use clang
				self.exe = [
					'lli',
					'--load=libpng12' + library_suffix,
					'--load=libprotobuf' + library_suffix,
					bitcodepath]
		if self.exe is None:
			exepath = os.path.join(self.bindir, "districter2")
			if IsExecutableFile(exepath):
				self.exe = [exepath]
		if self.exe is None:
			sys.stderr.write("failed to find district2 executable\n")
			sys.exit(1)
		if not IsExecutableDir(self.datadir):
			if self.options.server:
				os.makedirs(self.datadir)
			else:
				sys.stderr.write("bogus data dir \"%s\"\n" % self.datadir)
				sys.exit(1)
	
	def allowConfigPath(self, path):
		if self.config_include:
			# must match all
			for pattern in self.config_include:
				if not pattern.search(path):
					return False
		for pattern in self.config_exclude:
			if pattern.search(path):
				return False
		return True
	
	def loadConfigdir(self, configdir):
		"""Load a directory filled with config files."""
		for cname in os.listdir(configdir):
			# skip some common directory cruft
			if ignoreFile(cname):
				continue
			cpath = os.path.join(configdir, cname)
			if not self.allowConfigPath(cpath):
				continue
			logging.debug('loading %s as %s, dataroot=%s', cname, cpath, self.datadir)
			try:
				c = configuration(name=cname, datadir=None, config=cpath, dataroot=self.datadir)
				assert c.name not in self.config
				self.config[c.name] = c
			except:
				sys.stderr.write('failed to load config "%s"\n' % cpath)
		if not self.config:
			sys.stderr.write('error: --configdir="%s" but loaded no configs\n' % (configdir))
			sys.exit(1)
	
	def loadStateConfigurations(self, dirpath):
		"""For data/XX, load data/XX/config/* and return (name, config) list."""
		stu = os.path.basename(dirpath)
		assert len(stu) == 2
		configdir = os.path.join(dirpath, 'config')
		if not os.path.isdir(configdir):
			logging.debug('no %s/config', dirpath)
			return None
		found = []
		for variant in os.listdir(configdir):
			if ignoreFile(variant):
				logging.debug('ignore file %s/config/"%s"', dirpath, variant)
				continue
			cpath = os.path.join(dirpath, 'config', variant)
			if not self.allowConfigPath(cpath):
				logging.debug('filter out "%s"', cpath)
				continue
			cname = stu + '_' + variant
			self.config[cname] = configuration(
				name=cname,
				datadir=dirpath,
				config=cpath,
				dataroot=self.datadir)
			found.append( (cname, self.config[cname]) )
			logging.debug('set config "%s"', cname)
		return found
	
	def loadConfigurations(self):
		"""1. Things explicitly listed by --config; else
		2. Things in --configdir; else
		3. XX/config/*
		
		2 and 3 are filtered by --config-include --config-exclude"""
		# 1. --config
		if self.configArgList:
			for cname in self.configArgList:
				c = configuration(
					name=os.path.split(cname)[-1], config=cname, dataroot=self.datadir)
				assert c.name not in self.config
				self.config[c.name] = c
			return
		# 2. --configdir
		if self.configdir is not None:
			self.loadConfigdir(self.configdir)
			return
		# 3. data/??/config/*
		for xx in os.listdir(self.datadir):
			dirpath = os.path.join(self.datadir, xx)
			if not os.path.isdir(dirpath):
				logging.debug('data/"%s" not a dir', xx)
				continue
			stu = xx.upper()
			if stu != xx:
				logging.debug('data/"%s" is not upper case', xx)
				continue
			if self.statearglist and stu not in self.statearglist:
				logging.debug('"%s" not in state arg list', stu)
				continue
			self.loadStateConfigurations(dirpath)
		if (self.statearglist or self.config_include) and not self.config:
			sys.stderr.write('error: failed to load any configs\n')
			sys.exit(1)
		if self.config:
			return
		# 4. fall back to old no-config state data dirs
		# TODO: delete this, it'll never happen again.
		logging.warning('no configs, trying old setup')
		if not self.config:
			# get all the old defaults
			for stdir in glob.glob(self.datadir + "/??"):
				c = configuration(datadir=stdir, dataroot=self.datadir)
				self.config[c.name] = c
	
	def applyConfigOverrideLine(self, line):
		(cname, cline) = line.split(':', 1)
		cname = cname.strip()
		if cname == 'all':
			for cfg in self.config.itervalues():
				cfg.applyConfigLine(cline)
			return
		if cname in self.config:
			self.config[cname].applyConfigLine(cline)
	
	def _loadConfigOverrideFile(self):
		"""Return lines to apply."""
		if not self.config_override_path:
			return []
		try:
			st = os.stat(self.config_override_path)
		except OSError, e:
			# doesn't exist, whatever.
			return []
		if self.config_override_lastload:
			if st.st_mtime > self.config_override_lastload:
				return []
		print 'reloading configoverride'
		self.config_override_lastload = st.st_mtime
		f = open(self.config_override_path, 'r')
		out = f.readlines()
		f.close()
		return out

	def loadConfigOverride(self):
		lines = []
		if self.client:
			self.client.loadConfiguration()
			for line in self.client.runOptionLines():
				lines.append(line)
		lines.extend(self._loadConfigOverrideFile())
		if not lines:
			return
		if self.lock:
			self.lock.acquire()
		for line in lines:
			self.applyConfigOverrideLine(line)
		if self.lock:
			self.lock.release()
	
	def shouldstop(self):
		if self.softfail:
			return True
		if self.dry_run:
			return False
		if self.stoppath and os.path.exists(self.stoppath):
			self.addStopReason(self.stoppath + ' exists')
			return True
		if self.end is not None:
			now = time.time()
			if now > self.end:
				self.addStopReason('ran past end time (now=%s end=%s)' % (
					now, self.end))
				return True
		return False
	
	def maybe_mkdir(self, path):
		if self.dry_run or self.verbose:
			print "mkdir %s" % path
		if not self.dry_run:
			os.mkdir(path)
	
	def runthread(self, label='x'):
		# See also RunThread.run below
		while not self.shouldstop():
			# sleep 1 is a small price to pay to prevent stupid runaway loops
			time.sleep(1)
			self.runtimeLoadDataFromServer()
			self.loadConfigOverride()
			stu = self.getNextState()
			self.runstate(stu, label)
	
	def runstate(self, stu, label=None):
		"""Wrapper around runstate_inner to aid in runlog."""
		start_timestamp = timestamp()
		try:
			ok = self.runstate_inner(stu, start_timestamp, label)
		except Exception, e:
			ok = False
			e_str = 'runstate_inner(%s,) failed with: %s' % (stu, traceback.format_exc())
			sys.stderr.write(e_str + '\n')
			traceback.print_exc()
			self.addStopReason(e_str)
			self.softfail = True
		if (not self.dry_run) and (self.runlog is not None):
			if ok:
				okmsg = 'ok'
			else:
				okmsg = 'FAILED'
			self.runlog.write('%s %s - %s %s\n' % (
				stu, start_timestamp, timestamp(), okmsg))
			self.runlog.flush()
		if label:
			del(self.currentOps[label])
		return ok

	def runstate_inner(self, stu, start_timestamp, label):
		"""This is the primary sequence of actions to run the solver on a state."""
		if not os.path.exists(stu):
			self.maybe_mkdir(stu)
		ctd = os.path.join(stu, start_timestamp)
		if label:
			self.currentOps[label] = ctd
		self.maybe_mkdir(ctd)
		if self.options.solutionlog:
			self.maybe_mkdir(os.path.join(ctd,"g"))
		statlog = os.path.join(ctd, "statlog")
		statsum = os.path.join(ctd, "statsum")
		if not self.dry_run:
			self.config[stu].rereadIfUpdated()
			fout = open(statlog, "w")
			if not fout:
				self.addStopReason("could not open \"%s\"" % statlog)
				sys.stderr.write(self.stopreason + "\n")
				self.softfail = True
				return False
			fout.close()
		args = dict()
		args.update(self.stdargs)
		args.update(self.solverMode)
		args.update(self.config[stu].args)
		if self.options.solutionlog:
			args['--sLog'] = 'g/'
		cmd = niceArgs + self.exe + dictToArgList(args)
		print "(cd %s && \\\n%s )" % (ctd, ' '.join(cmd))
		if not self.dry_run:
			p = subprocess.Popen(cmd, shell=False, bufsize=4000, cwd=ctd,
				stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			errorlines = []
			outlines = []
			if has_poll:
				(outlines, errorlines) = poll_run(p, stu)
			elif has_select:
				(outlines, errorlines) = select_run(p, stu)
			else:
				self.addStopReason('has neither poll nor select\n')
				sys.stderr.write(self.stopreason + "\n")
				self.softfail = True
				return False
			try:
				for line in p.stdin:
					if line:
						sys.stdout.write("O " + stu + ": " + line)
						lastlines(outlines, 10, line)
			except:
				pass
			try:
				for line in p.stderr:
					if line:
						sys.stdout.write("E " + stu + ": " + line)
						lastlines(errorlines, 10, line)
			except:
				pass
			if p.returncode != 0:
				# logCompletion mechanism allows for deferred script quit, 
				# possibly after many intermittent failures which will all
				# be logged here. I guess that's ok.
				# TODO: present failures to web result server.
				statusString = "solver exited with status %d" % p.returncode
				self.addStopReason(statusString)
				statlog = open(os.path.join(ctd, 'statlog'), 'a')
				statlog.write(statusString)
				statlog.write('\n')
				if errorlines:
					self.addStopReason('\n' + '\n'.join(errorlines))
					statlog.write('# last lines of stderr:\n')
					for eline in errorlines:
						statlog.write('#' + eline + '\n')
				if outlines:
					statlog.write('# last lines of stdout:\n')
					for eline in outlines:
						statlog.write('#' + eline + '\n')
				statlog.close()
				sys.stderr.write(self.stopreason + '\n')
				self.softfail = self.logCompletion(0)
				return False
			self.logCompletion(1)
			fin = open(statlog, "r")
			fout = open(statsum, "w")
			for line in fin:
				if line[0] == "#":
					fout.write(line)
			fout.close()
			fin.close()
		if self.dry_run or self.verbose:
			print "grep ^# %s > %s" % (statlog, statsum)
			print "gzip %s" % statlog
		if not self.dry_run:
			# TODO: don't call out, do it in python
			ret = subprocess.call(["gzip", statlog])
			if ret != 0:
				self.addStopReason("gzip statlog failed %d" % ret)
				sys.stderr.write(self.stopreason + '\n')
				self.softfail = True
				return False
		if self.options.solutionlog:
			cmd = ["tar", "jcf", "g.tar.bz2", "g"]
			if self.dry_run or self.verbose:
				print "(cd %s && %s)" % (ctd, " ".join(cmd))
			if not self.dry_run:
				g_tar = tarfile.open(os.path.join(ctd, 'g.tar.bz2'), 'w|bz2')
				g_tar.add(os.path.join(ctd, 'g'), arcname='g')
				g_tar.close()
			# TODO: use python standard library recursive remove
			cmd = ["rm", "-rf", "g"]
			if self.dry_run or self.verbose:
				print "(cd %s && %s)" % (ctd, " ".join(cmd))
			if not self.dry_run:
				subprocess.Popen(cmd, cwd=ctd).wait()
				# don't care if rm-rf failed? it wouldn't report anyway?
		didSend = None
		if self.client and self.config[stu].sendAnything:
			if self.dry_run:
				print 'would send dir "%s" to server' % (ctd, )
			else:
				self.client.sendResultDir(ctd, {'config': stu}, sendAnything=True)
			didSend = ctd
		mb = manybest.manybest()
		mb.ngood = self.options.keepbest
		mb.mvbad = True
		mb.rmbad = True
		mb.rmempty = True
		mb.nlim = self.options.keepbest
		mb.verbose = sys.stderr
		mb.dry_run = self.dry_run
		mb.setRoot(stu)
		#./manybest.py -ngood 15 -mvbad -rmbad -rmempty -n 10
		try:
			mb.run()
		except manybest.NoRunsException:
			pass
		self.doDrend(stu, mb)
		self.doBestlog(stu, mb)
		bestPath = None
		if len(mb.they) > 0:
			bestPath = os.path.join(stu, mb.they[0].root)
		sconf = self.config[stu]
		if (
			bestPath and
			self.client and
			(bestPath != didSend) and
			((not sconf.kmppSendThreshold)
			 or (mb.they[0].kmpp <= sconf.kmppSendThreshold)) and
			((not sconf.spreadSendThreshold) or
			 (mb.they[0].spread is None) or
			 (mb.they[0].spread <= sconf.spreadSendThreshold))):
			if self.dry_run:
				print 'would send best dir "%s" if it has not already been sent' % (mb.they[0], )
			else:
				self.client.sendResultDir(bestPath, {'config': stu})
		return True

	def doDrend(self, stu, mb):
		if not self.config[stu].drendargs:
			return
		final2_png = os.path.join("link1", stu + "_final2.png")
		if (not self.dry_run) and (
		    (not mb.they) or os.path.exists(os.path.join(stu, final2_png))):
			return
		cmd = [os.path.join(self.bindir, "drend")] + dictToArgList(self.config[stu].drendargs)
		cmdstr = "(cd %s && %s)" % (stu, ' '.join(cmd))
		print cmdstr
		if not self.dry_run:
			ret = subprocess.Popen(cmd, shell=False, cwd=stu).wait()
			if ret != 0:
				self.addStopReason("drend failed (%d): %s\n" % (ret, cmdstr))
				sys.stderr.write(self.stopreason + '\n')
				self.softfail = True
				return False
		start_png = os.path.join(self.datadir, stu, stu + "_start.png")
		if not os.path.exists(start_png):
			logging.info('no start_png "%s"', start_png)
			return
		ba_png = os.path.join("link1", stu + "_ba.png")
		cmd = ["convert", start_png, final2_png, "+append", ba_png]
		print "(cd %s && %s)" % (stu, " ".join(cmd))
		if not self.dry_run:
			subprocess.Popen(cmd, cwd=stu).wait()
		cmd = ["convert", ba_png, "-resize", "500x500", os.path.join("link1", stu + "_ba_500.png")]
		print "(cd %s && %s)" % (stu, " ".join(cmd))
		if not self.dry_run:
			subprocess.Popen(cmd, cwd=stu).wait()
	
	def setCurrentRunningHtml(self, handler):
		"""Return False if main handler should continue, True if this extension has done the whole output."""
		if handler.path == '/':
			handler.dirExtra = ('<div class="gst"><div class="stt">Started at: ' + time.ctime(self.start) +
				' (now: ' + time.ctime() + ')</div><div class="cact">Currently Active:</div>' + 
				''.join(['<div class="acti"><a href="%s/">%s/</a></div>' % (x, x) for x in self.currentOps.values()]) + '</div>')
			return False
		if handler.path == '/config':
			handler.send_response(200)
			handler.end_headers()
			handler.wfile.write("""<!doctype html>
<html><head><title>redistricting run configuration</title></head>
<body><h1>redistricting run configuration</h1>
""")
			handler.wfile.write(
				'<h2>options</h2><p style="font-family:monospace;">%s</p>' %
				(htmlEscape(repr(self.options)),))
			handler.wfile.write('<h2>configurations</h2><table>')
			configkeys = self.config.keys()
			configkeys.sort()
			for cname in configkeys:
				c = self.config[cname]
				handler.wfile.write('<tr><td>%s</td><td>%s</td></tr>\n' % (cname, c))
			handler.wfile.write('</table>')
			handler.wfile.write('<h2>runallstatesobject</h2><table>')
#			for elem in dir(self):
			for elem in ('bests', 'bindir', 'configArgList', 'config_exclude', 'config_include', 'config_override_lastload', 'config_override_path', 'configdir', 'currentOps', 'd2args', 'datadir', 'diskQuota', 'diskUsage', 'dry_run', 'end', 'errorRate', 'errorSample', 'exe', 'lock', 'numthreads', 'qpos', 'runSuccessHistory', 'runlog', 'softfail', 'solverMode', 'start', 'statearglist', 'states', 'stdargs', 'stoppath', 'stopreason', 'verbose'):
				handler.wfile.write('<tr><td>%s</td><td>%s</td></tr>\n' % (elem, htmlEscape(repr(getattr(self, elem)))))
			handler.wfile.write('</table>')
			handler.wfile.write('</body></html>\n')
			return True
	
	def loadDataFromServer(self):
		if not self.client:
			return
		loadedDirPaths = []
		try:
			if self.statearglist:
				for stu in self.statearglist:
					loadedDirPath = self.client.getDataForStu(stu)
					if loadedDirPath:
						loadedDirPaths.append(loadedDirPath)
			else:
				# Pick one randomly, fetch it
				self.diskUsage = du(self.datadir)
				if self.diskQuota - self.diskUsage < QUOTA_HEADROOM_:
					logging.warn('using %d bytes of %d quota, want at least %d free before fetching more data', self.diskUsage, self.diskQuota, QUOTA_HEADROOM_)
					return
				loadedDirPath = self.client.unpackArchive(
					self.client.randomDatasetName())
				if loadedDirPath:
					loadedDirPaths.append(loadedDirPath)
		except:
			# something went wrong, try to go with what we got
			traceback.print_exc()
			pass
		for path in loadedDirPaths:
			newConfigs = self.loadStateConfigurations(path)
			for name, cf in newConfigs:
				logging.info('got new config "%s" under path "%s"', name, path)

	def runtimeLoadDataFromServer(self):
		"""Part of runthread, occasionally load more data."""
		if not self.client:
			return
		if self.lock:
			self.lock.acquire()
		doit = self.runtimeLoadDataNext < time.time()
		if doit:
			self.runtimeLoadDataNext = time.time() + 20000
		if self.lock:
			self.lock.release()
		if not doit:
			return
		self.loadDataFromServer()
	
	def main(self, argv):
		self.readArgs(argv)
		self.checkSetup()
		if self.options.server:
			logging.info('configuring client of server "%s"', self.options.server)
			self.client = client.Client(self.options)
			self.loadDataFromServer()
		self.loadConfigurations()
		if not self.config:
			sys.stderr.write('error: no configurations\n')
			sys.exit(1)
		if self.verbose:
			for c in self.config.itervalues():
				print c
		
		self.states = self.config.keys()
		self.states.sort()
		print " ".join(self.states)
		
		# run in a different order each time in case we do partial runs, spread the work
		random.shuffle(self.states)
		
		rootdir = os.getcwd()
		self.stoppath = os.path.join(rootdir, "stop")
		
		if self.dry_run:
			self.numthreads = 1
			for stu in self.states:
				self.runstate(stu)
			return
		
		severthread = None
		if self.options.port > 0:
			import resultserver
			def extensionFu(handler):
				return self.setCurrentRunningHtml(handler)
			actions = {}
			if self.stoppath:
				resultserver.TouchAction(self.stoppath, 'Graceful Stop', 'stop').setDict(actions)
			resultserver.TouchAction(os.path.join(rootdir, 'reload'), 'Reload After Stop', 'reload').setDict(actions)
			serverthread = resultserver.startServer(self.options.port, extensions=extensionFu, actions=actions)
			if serverthread is not None:
				print "serving status at\nhttp://localhost:%d/" % self.options.port
			else:
				print "status serving failed to start"

		if self.numthreads <= 1:
			print "running one thread"
			self.runthread()
		else:
			print "running %d threads" % self.numthreads
			self.lock = threading.Lock()
			threads = []
			for x in xrange(0, self.numthreads):
				threadLabel = 't%d' % x
				threads.append(RunThread(self, threadLabel))
				#threads.append(threading.Thread(target=runallstates.runthread, args=(self,threadLabel), name=threadLabel))
			for x in threads:
				x.start()
				time.sleep(1.5)
			for x in threads:
				x.join()

		if not self.dry_run:
			if self.stopreason:
				print self.stopreason
			if os.path.exists(self.stoppath):
				os.remove(self.stoppath)

class RunThread(object):
	allthreads = []
	lock = threading.Lock()
	
	def __init__(self, runner, label):
		self.runner = runner
		self.thread = threading.Thread(target=self.run, name=label)
		self.label = label
		self.stu = None
		RunThread.lock.acquire()
		RunThread.allthreads.append(self)
		RunThread.lock.release()
	
	def start(self):
		self.thread.start()
	
	def join(self):
		self.thread.join()
	
	def getNextState(self):
		stu = None
		while stu is None:
			stu = self.runner.getNextState()
			RunThread.lock.acquire()
			for ot in RunThread.allthreads:
				if ot.stu == stu:
					stu = None
					break
			self.stu = stu
			RunThread.lock.release()
	
	def run(self):
		# See also runthread() above
		while not self.runner.shouldstop():
			# sleep 1 is a small price to pay to prevent stupid runaway loops
			time.sleep(1)
			self.runner.runtimeLoadDataFromServer()
			self.runner.loadConfigOverride()
			self.getNextState()
			if self.stu is None:
				# try again in a second
				continue
			self.runner.runstate(self.stu, self.label)
			self.stu = None

if __name__ == "__main__":
	it = runallstates()
	it.main(sys.argv)
