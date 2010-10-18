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
	lastelines = []
	while p.poll() is None:
		for (fd, event) in poller.poll(500):
			if p.stdout.fileno() == fd:
				line = p.stdout.readline()
				if line:
					sys.stdout.write("O " + stu + ": " + line)
			elif p.stderr.fileno() == fd:
				line = p.stderr.readline()
				if line:
					sys.stdout.write("E " + stu + ": " + line)
					lastlines(lastelines, 10, line)
			else:
				sys.stdout.write("? %s fd=%d\n" % (stu, fd))
	return lastelines

def select_run(p, stu):
	"""Read err and out from a process, copying to sys.stdout.
	Return last 10 lines of error in list."""
	lastelines = []
	while p.poll() is None:
		(il, ol, el) = select.select([p.stdout, p.stderr], [], [], 0.5)
		for fd in il:
			if (p.stdout.fileno() == fd) or (fd == p.stdout):
				line = p.stdout.readline()
				if line:
					sys.stdout.write("O " + stu + ": " + line)
			elif (p.stderr.fileno() == fd) or (fd == p.stderr):
				line = p.stderr.readline()
				if line:
					sys.stdout.write("E " + stu + ": " + line)
					lastlines(lastelines, 10, line)
			else:
				sys.stdout.write("? %s fd=%s\n" % (stu, fd))
	return lastelines

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
}
# TODO: check mutually exclusive arguments in mergeArgs
EXCLUSIVE_CLASSES_ = [
	['--oldCDs', '--blankDists'],
	['--nearest-neighbor', '--d2'],
]

def mergeArgs(oldargs, newargs):
	"""new args replace, or append to oldargs."""
	ni = 0
	while ni < len(newargs):
		oi = 0
		nidone = False
		while oi < len(oldargs):
			if oldargs[oi] == newargs[ni]:
				if HAS_PARAM_[oldargs[oi]]:
					oi += 1
					ni += 1
					oldargs[oi] = newargs[ni]
				nidone = True
				break
			oi += 1
		if not nidone:
			oldargs.append(newargs[ni])
			if HAS_PARAM_[newargs[ni]]:
				ni += 1
				oldargs.append(newargs[ni])
		ni += 1
	return oldargs

def parseArgs(x):
	"""TODO, handle shell escaping rules."""
	return x.split()


class ParseError(Exception):
	pass

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
		self.drendargs = []
		self.args = []
		self.enabled = None
		self.geom = None
		self.path = config
		self.dataroot = dataroot
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
		self.args = ['-o', self.name + '_final.dsz']
		self.drendargs = ['--loadSolution', 'link1/bestKmpp.dsz',
			'--pngout', 'link1/%s_final2.png' % self.name]
		if self.datadir is not None:
			ok = self.readDatadirConfig()
			assert ok
		if config is not None:
			self.readConfigFile(config, dataroot)
	
	def __str__(self):
		return '(%s %s %s %s)' % (self.name, self.datadir, self.args, self.drendargs)
	
	def setRootDatadir(self, root):
		"""Replace $DATA with root in all args."""
		for i in xrange(len(self.args)):
			if isinstance(self.args[i], basestring):
				self.args[i] = self.args[i].replace('$DATA', root)
		for i in xrange(len(self.args)):
			if isinstance(self.drendargs[i], basestring):
				self.drendargs[i] = self.drendargs[i].replace('$DATA', root)
		if self.datadir:
			self.datadir = self.datadir.replace('$DATA', root)

	datadir_state_re = re.compile(r'.*/([a-zA-Z]{2})/?$')

	def isEnabled(self):
		return (self.enabled is None) or self.enabled
	
	def applyConfigLine(self, line, dataroot=None):
		line = line.strip()
		if (len(line) == 0) or line.startswith('#'):
			return True
		if dataroot is None:
			dataroot = self.dataroot
		if dataroot is not None:
			line = line.replace('$DATA', dataroot)
		if line.startswith('solve:'):
			mergeArgs(self.args, parseArgs(line[6:].strip()))
		elif line.startswith('drend:'):
			mergeArgs(self.drendargs, parseArgs(line[6:].strip()))
		elif line.startswith('common:'):
			newargs = parseArgs(line[7:].strip())
			mergeArgs(self.args, newargs)
			mergeArgs(self.drendargs, newargs)
		elif line.startswith('datadir!:'):
			self.datadir = line[9:].strip()
		elif line.startswith('datadir:'):
			old_datadir = self.datadir
			self.datadir = line[8:].strip()
			logging.debug('old datadir=%s new datadir=%s', old_datadir, self.datadir)
			if old_datadir != self.datadir:
				if not self.readDatadirConfig():
					raise ParseError('problem with datadir "%s"' % self.datadir)
		elif line == 'enable' or line == 'enabled':
			self.enabled = True
		elif line == 'disable' or line == 'disabled':
			self.enabled = False
		else:
			raise ParseError('bogus config line: "%s"\n' % line)
		return True
	
	def readConfigFile(self, x, dataroot=None):
		"""x is a path or a file like object iterable over lines.
		
		solve: arguments for solver
		drend: arguments for drend
		common: arguments for any tool
		datadir: path to datadir. Immediately reads config in datadir.
		datadir!: path to datadir. does nothing.
		enabled
		disabled
		empty lines and lines starting with '#' are ignored.
		
		$DATA is substituted with the global root datadir
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
		datadir_args = ['-P', datapath]
		logging.debug('datadir args %s', datadir_args)
		self.args = mergeArgs(datadir_args, self.args)
		logging.debug('merged args %s', self.args)
		datadir_drend_args = ['-P', datapath, '--mppb', pxpath]
		logging.debug('geom drend args %s', datadir_drend_args)
		self.drendargs = mergeArgs(datadir_drend_args, self.drendargs)
		logging.debug('merged drend args %s', self.drendargs)
		
		# TODO: delete this, setupstatedata.py isn't generating it anymore
		# set args from geometry.pickle if available
		pgeompath = os.path.join(self.datadir, 'geometry.pickle')
		if os.path.exists(pgeompath):
			f = open(pgeompath, 'rb')
			self.geom = pickle.load(f)
			f.close()
			geom_args = ['-d', self.geom.numCDs()]
#			'--pngout', self.name + '_final.png',
#			'--pngW', geom.basewidth, '--pngH', geom.baseheight,
			logging.debug('geom args %s', geom_args)
			self.args = mergeArgs(geom_args, self.args)
			logging.debug('merged args %s', self.args)
			geom_drendargs = [
				'-d', str(self.geom.numCDs()),
#				'--pngW', str(self.geom.basewidth * 4),
#				'--pngH', str(self.geom.baseheight * 4),
				]
			logging.debug('geom drend args %s', geom_drendargs)
			self.drendargs = mergeArgs(geom_drendargs, self.drendargs)
			logging.debug('merged drend args %s', self.drendargs)
		print self
		return True


def ignoreFile(cname):
	return cname.startswith('.') or cname.startswith('#') or cname.endswith('~') or cname.endswith(',v')


class runallstates(object):
	def __init__(self):
		self.bindir = os.environ.get('REDISTRICTER_BIN')
		if self.bindir is None:
			self.bindir = os.path.dirname(os.path.abspath(__file__))
		self.datadir = os.environ.get("REDISTRICTER_DATA")
		if self.datadir is None:
			self.datadir = os.path.join(self.bindir, "data")
		self.exe = None
		self.solverMode = []
		self.d2args = ['--d2', '--popRatioFactorPoints', '0,1.4,30000,1.4,80000,500,100000,50,120000,500', '-g', '150000']
		self.stdargs = ['--blankDists', '--sLog', 'g/', '--statLog', 'statlog', '--binLog', 'binlog', '--maxSpreadFraction', '0.01']
		self.start = time.time();
		self.end = None
		# built from argv, things we'll run
		self.statearglist = []
		self.numthreads = 1
		self.extrargs = []
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

	def addStopReason(self, reason):
		if self.lock:
			self.lock.acquire()
		self.stopreason += reason
		if self.lock:
			self.lock.release()

	def getNextState(self):
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
				x = manybest.slog("", float(a[1]), "", "")
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
		argp = optparse.OptionParser()
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
		argp.add_option('--solver-arg', dest='solver_arg', action='append', default=[], help='argument passed to solver. may be repeated')
		argp.add_option('--runlog', dest='runlog', default=None, help='append a record of all solver runs here')
		argp.add_option('--bestlog', dest='bestlog', default=None, help='append a record of each solver run that is best-so-far')
		argp.add_option('--server', dest='server', default=default_server, help='url of config page on server from which to download data')
		argp.add_option('--force-config-reload', dest='force_config_reload', action='store_true', default=False)
		argp.add_option('--verbose', '-v', dest='verbose', action='store_true', default=False)
		argp.add_option('--failuresPerSuccessesAllowed', '--fr', dest='failureRate', default=None, help='f/s checks the last (f+s) events and exits if >f are failures')
		(options, args) = argp.parse_args()
		self.options = options
		if options.verbose:
			logging.getLogger().setLevel(logging.DEBUG)
		for arg in args:
			astu = arg.upper()
			#if IsReadableFile(os.path.join(self.datadir, astu, "basicargs")) or IsReadableFile(os.path.join(self.datadir, astu, "geometry.pickle")):
			if (os.path.isdir(os.path.join(self.datadir, astu)) or
				self.options.server):
				# Any existing directory, or if there's a server we might fetch it.
				logging.debug('add stu "%s"', astu)
				self.statearglist.append(astu)
			else:
				sys.stderr.write("%s: bogus arg \"%s\"\n" % (argv[0], arg))
				sys.exit(1)
		self.datadir = os.path.realpath(options.datadir)
		options.datadir = self.datadir
		self.bindir = os.path.realpath(options.bindir)
		options.bindir = self.bindir
		if options.exepath:
			self.exe = os.path.realpath(options.exepath)
		if options.runsecs is not None:
			self.end = self.start + options.runsecs
		self.configArgList = options.configList
		self.configdir = options.configdir
		self.config_override_path = options.config_override_path
		self.numthreads = options.threads
		self.dry_run = options.dry_run
		if options.mode == 'd2':
			self.solverMode = self.d2args
		else:
			self.solverMode = options.solver_arg
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
			assert self.bindir is not None
			self.exe = os.path.join(self.bindir, "districter2")
			self.options.exepath = self.exe
		if not IsExecutableFile(self.exe):
			sys.stderr.write("bogus exe \"%s\" is not executable\n" % self.exe)
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
	
	def loadConfigurations(self):
		"""1. Things explicitly listed by --config; else
		2. Things in --configdir; else
		3. XX/config/*
		
		2 and 3 are filtered by --config-include --config-exclude"""
		# 1. --config
		for cname in self.configArgList:
			c = configuration(
				name=os.path.split(cname)[-1], config=cname, dataroot=self.datadir)
			assert c.name not in self.config
			self.config[c.name] = c
		if self.config:
			return
		# 2. --configdir
		if self.configdir is not None:
			self.loadConfigdir(self.configdir)
			return
		# 3. data/??/config/*
		for xx in os.listdir(self.datadir):
			if not os.path.isdir(os.path.join(self.datadir, xx)):
				logging.debug('data/"%s" not a dir', xx)
				continue
			stu = xx.upper()
			if self.statearglist and stu not in self.statearglist:
				logging.debug('"%s" not in state arg list', stu)
				continue
			configdir = os.path.join(self.datadir, stu, 'config')
			if not os.path.isdir(configdir):
				logging.debug('no %s/config', xx)
				continue
			for variant in os.listdir(configdir):
				if ignoreFile(variant):
					logging.debug('ignore file %s/config/"%s"', xx, variant)
					continue
				cpath = os.path.join(self.datadir, xx, 'config', variant)
				if not self.allowConfigPath(cpath):
					logging.debug('filter out "%s"', cpath)
					continue
				cname = stu + '_' + variant
				self.config[cname] = configuration(
					name=cname,
					datadir=os.path.join(self.datadir, xx),
					config=cpath,
					dataroot=self.datadir)
				logging.debug('set config "%s"', cname)
		if (self.statearglist or self.config_include) and not self.config:
			sys.stderr.write('error: failed to load any configs\n')
			sys.exit(1)
		# 4. fall back to old no-config state data dirs
		logging.warning('no configs, trying old setup')
		if not self.config:
			# get all the old defaults
			for stdir in glob.glob(self.datadir + "/??"):
				c = configuration(datadir=stdir, dataroot=self.datadir)
				self.config[c.name] = c
	
	def applyConfigOverrideLine(self, line):
		(cname, cline) = line.split(':', 1)
		cname = cname.strip()
		if cname in self.config:
			self.config[cname].applyConfigLine(cline)
	
	def loadConfigOverride(self):
		if not self.config_override_path:
			return
		try:
			st = os.stat(self.config_override_path)
		except OSError, e:
			# doesn't exist, whatever.
			return
		if self.config_override_lastload:
			if st.st_mtime > self.config_override_lastload:
				return
		if self.lock:
			self.lock.acquire()
		if self.client:
			for line in self.client.runOptionLines():
				self.applyConfigOverrideLine(line)
		f = open(self.config_override_path, 'r')
		for line in f:
			self.applyConfigOverrideLine(line)
		self.config_override_lastload = st.st_mtime
		f.close()
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
		while not self.shouldstop():
			# sleep 1 is a small price to pay to prevent stupid runaway loops
			time.sleep(1)
			stu = self.getNextState()
			self.loadConfigOverride()
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
		cmd = niceArgs + [self.exe] + self.stdargs + self.solverMode + self.config[stu].args + self.extrargs
		print "(cd %s && \\\n%s )" % (ctd, ' '.join(cmd))
		if not self.dry_run:
			p = subprocess.Popen(cmd, shell=False, bufsize=4000, cwd=ctd,
				stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			errorlines = []
			if has_poll:
				errorlines = poll_run(p, stu)
			elif has_select:
				errorlines = select_run(p, stu)
			else:
				self.addStopReason('has neither poll nor select\n')
				sys.stderr.write(self.stopreason + "\n")
				self.softfail = True
				return False
			try:
				for line in p.stdin:
					if line:
						sys.stdout.write("O " + stu + ": " + line)
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
				self.addStopReason("solver exited with status %d" % p.returncode)
				if errorlines:
					self.addStopReason('\n' + '\n'.join(errorlines))
					statlog = open(os.path.join(ctd, 'statlog'), 'a')
					statlog.write('# last lines of stderr:\n')
					for eline in errorlines:
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
		mb = manybest.manybest()
		mb.ngood = 15
		mb.mvbad = True
		mb.rmbad = True
		mb.rmempty = True
		mb.nlim = 10
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
		return True

	def doDrend(self, stu, mb):
		drendargs = self.config[stu].drendargs
		if not drendargs:
			return
		final2_png = os.path.join("link1", stu + "_final2.png")
		if (not self.dry_run) and (
		    (not mb.they) or os.path.exists(os.path.join(stu, final2_png))):
			return
		cmd = [os.path.join(self.bindir, "drend")] + drendargs
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
		
		

	def doBestlog(self, stu, mb):
		if self.bestlog is None:
			return
		if not mb.they:
			return
		best = mb.they[0]
		oldbest = self.bests.get(stu)
		if (oldbest is None) or (best.kmpp < oldbest.kmpp):
			self.bests[stu] = best
			if oldbest is None:
				oldmsg = "was none"
			else:
				oldmsg = "old=%f" % oldbest.kmpp
			outf = self.bestlog
			if self.dry_run:
				outf = sys.stderr
			outf.write("%s\t%f\t%s\t%s\t%s\n" % (
				stu,
				best.kmpp,
				os.path.join(stu, best.root),
				datetime.datetime.now().isoformat(" "),
				oldmsg))
			outf.flush()
			if self.client:
				self.client.sendResultDir(os.path.join(stu, best.root))
	
	def setCurrentRunningHtml(self, handler):
		if handler.path == '/':
			handler.dirExtra = ('<div><div>Started at: ' + time.ctime(self.start) +
				'</div><div>Currently Active:</div>' + 
				''.join(['<div><a href="%s/">%s/</a></div>' % (x, x) for x in self.currentOps.values()]) + '</div>')

	def main(self, argv):
		self.readArgs(argv)
		self.checkSetup()
		if self.options.server:
			logging.info('configuring client of server "%s"', self.options.server)
			self.client = client.Client(self.options)
			if self.statearglist:
				for stu in self.statearglist:
					self.client.getDataForStu(stu)
			else:
				# Pick one randomly, fetch it
				self.client.unpackArchive(self.client.randomDatasetName())
		self.loadConfigurations()
		if not self.config:
			sys.stderr.write('error: no configurations\n')
			sys.exit(1)
		for c in self.config.itervalues():
			print c
		
		self.states = self.config.keys()
		
		# run in a different order each time in case we do partial runs, spread the work
		random.shuffle(self.states)

		print " ".join(self.states)

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
				self.setCurrentRunningHtml(handler)
				return False
			serverthread = resultserver.startServer(self.options.port, extensions=extensionFu)
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
				threads.append(threading.Thread(target=runallstates.runthread, args=(self,threadLabel), name=threadLabel))
			for x in threads:
				x.start()
			for x in threads:
				x.join()

		if not self.dry_run:
			if self.stopreason:
				print self.stopreason
			if os.path.exists(self.stoppath):
				os.remove(self.stoppath)

if __name__ == "__main__":
	it = runallstates()
	it.main(sys.argv)
