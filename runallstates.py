#!/usr/bin/python
# Expects to run in the build dir with data/?? containing state data.
# Using setupstatedata.pl in the standard way should do this.
#
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
import os
import random
import re
import select
import stat
import subprocess
import sys
import threading
import time

# local imports
import manybest
import measureGeometry

__author__ = "Brian Olson"

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
				sys.stdout.write("O " + stu + ": " + line)
			elif p.stderr.fileno() == fd:
				line = p.stderr.readline()
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
				sys.stdout.write("O " + stu + ": " + line)
			elif (p.stderr.fileno() == fd) or (fd == p.stderr):
				line = p.stderr.readline()
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
			self.readDatadirConfig()
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
		self.datadir = self.datadir.replace('$DATA', root)

	datadir_state_re = re.compile(r'.*/([a-zA-Z]{2})/?$')

	def isEnabled(self):
		return (self.enabled is None) or self.enabled
	
	def applyConfigLine(self, line, dataroot=None):
		line = line.strip()
		if (len(line) == 0) or line.startswith('#'):
			return True
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
				self.readDatadirConfig()
		elif line == 'enabled':
			self.enabled = True
		elif line == 'disabled':
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
				self.applyConfigLine(line, dataroot)

	def readDatadirConfig(self, datadir=None):
		"""Read a datadir/geometry.pickle and calculate basic config.
		datadir is path to state dir, "data/MN" or so.
		"""
		if datadir is None:
			assert self.datadir is not None
		else:
			self.datadir = datadir
		m = configuration.datadir_state_re.match(self.datadir)
		if m is None:
			sys.stderr.write(
				'could not parse a state code out of datadir "%s"\n' % self.datadir)
			return None
		statecode = m.group(1)
		stl = statecode.lower()
		if os.path.exists(os.path.join(self.datadir, 'norun')):
			self.enabled = False
		
		# set some basic args based on any datadir
		datapath = os.path.join(self.datadir, stl + '.pb')
		pxpath = os.path.join(self.datadir, stl + '.mppb')
		datadir_args = ['-P', datapath]
		logging.debug('datadir args %s', datadir_args)
		self.args = mergeArgs(datadir_args, self.args)
		logging.debug('merged args %s', self.args)
		datadir_drend_args = ['-P', datapath, '--mppb', pxpath]
		logging.debug('geom drend args %s', datadir_drend_args)
		self.drendargs = mergeArgs(datadir_drend_args, self.drendargs)
		logging.debug('merged drend args %s', self.drendargs)
		
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
		else:
			logging.warning('no %s', pgeompath)
		print self


class runallstates(object):
	def __init__(self):
		self.datadir = os.environ.get("DISTRICT_DATA")
		if self.datadir is None:
			self.datadir = os.path.join(os.getcwd(), "data")
		self.bindir = None
		self.exe = None
		self.solverMode = []
		self.d2args = ['--d2', '--popRatioFactorPoints', '0,1.4,30000,1.4,80000,500,100000,50,120000,500', '-g', '150000']
		self.stdargs = ['--blankDists', '--sLog', 'g/', '--statLog', 'statlog', '--maxSpreadFraction', '0.01']
		self.start = datetime.datetime.now();
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

	def addStopReason(self, reason):
		if self.lock:
			self.lock.acquire()
		self.stopreason += reason
		if self.lock:
			self.lock.release()

	def getNextState(self):
		if self.lock:
			self.lock.acquire()
		stu = self.states[self.qpos]
		self.qpos = (self.qpos + 1) % len(self.states)
		if self.lock:
			self.lock.release()
		return stu

	def runthread(self):
		while not self.shouldstop():
			# sleep 1 is a small price to pay to prevent stupid runaway loops
			time.sleep(1)
			stu = self.getNextState()
			self.runstate(stu)

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
		"""Reads argv[1:]"""
		i = 1
		while i < len(argv):
			arg = argv[i]
			if arg == "--data":
				i += 1
				self.datadir = argv[i]
			elif arg == "--bin":
				i += 1
				self.bindir = argv[i]
				if not IsExecutableFile( os.path.join(self.bindir, "districter2")):
					sys.stderr.write( "bogus bin dir \"%s\" does not contain districter2\n" % bindir )
					sys.exit(1)
			elif arg == "--exe":
				i += 1
				maybe_exe = argv[i]
				if not IsExecutableFile( maybe_exe ):
					sys.stderr.write( "bogus exe \"%s\" is not executable\n" % maybe_exe )
					sys.exit(1)
				self.exe = maybe_exe
			elif arg == "--runsecs":
				i += 1
				self.end = self.start + datetime.timedelta(seconds=float(argv[i]))
			elif arg == "--":
				i += 1
				self.extrargs = " " + argv[i]
			elif arg == "--config":
				i += 1
				self.configArgList.append(argv[i])
			elif arg == "--configdir":
				i += 1
				assert self.configdir is None
				self.configdir = argv[i]
				self.loadConfigdir(self.configdir)
			elif arg == "--threads":
				i += 1
				self.numthreads = int(argv[i])
			elif (arg == "--dry-run") or (arg == "-n"):
				self.dry_run = True
			elif arg == "--d2":
				self.solverMode = self.d2args
			elif arg == "--nn":
				self.solverMode = []
			elif arg == "--runlog":
				i += 1
				self.openRunLog(argv[i])
			elif arg == "--bestlog":
				i += 1
				self.openBestLog(argv[i])
			else:
				astu = arg.upper()
				#if IsReadableFile(os.path.join(self.datadir, astu, "basicargs")) or IsReadableFile(os.path.join(self.datadir, astu, "geometry.pickle")):
				if os.path.isdir(os.path.join(self.datadir, astu)):
					self.statearglist.append(astu)
				else:
					sys.stderr.write("%s: bogus arg \"%s\"\n" % (argv[0], arg))
					sys.exit(1)
			i += 1


	def checkSetup(self):
		if self.exe is None:
			if self.bindir is None:
				self.bindir = os.getcwd()
			self.exe = os.path.join(self.bindir, "districter2")
			if not IsExecutableFile(self.exe):
				sys.stderr.write("bogus exe \"%s\" is not executable\n" % self.exe)
				sys.exit(1)

		if not IsExecutableDir(self.datadir):
			sys.stderr.write("bogus data dir \"%s\"\n" % self.datadir)

	def loadConfigdir(self, configdir):
		for cname in os.listdir(configdir):
			# skip some common directory cruft
			if cname.startswith('.') or cname.startswith('#') or cname.endswith('~') or cname.endswith(',v'):
				continue
			cpath = os.path.join(configdir, cname)
			logging.debug('loading %s as %s, dataroot=%s', cname, cpath, self.datadir)
			c = configuration(name=cname, datadir=None, config=cpath, dataroot=self.datadir)
			assert c.name not in self.config
			self.config[c.name] = c

	def loadConfigurations(self):
		for cname in self.configArgList:
			c = configuration(
				name=os.path.split(cname)[-1], config=cname, dataroot=self.datadir)
			assert c.name not in self.config
			self.config[c.name] = c
		# moved to readArgs
		#if self.configdir is not None:
		#	self.loadConfigdir(self.configdir)
		if self.config:
			# filter config list by statearglist
			all_config = self.config
			self.config = {}
			for st in self.statearglist:
				if st in all_config:
					it = all_config[st]
					self.config[it.name] = it
				else:
					self.config[st] = configuration(
						name=st, datadir=os.path.join(self.datadir, st))
		else:
			for st in self.statearglist:
				assert st not in self.config
				self.config[st] = configuration(
					name=st, datadir=os.path.join(self.datadir, st))
		if not self.config:
			# get all the old defaults
			for stdir in glob.glob(self.datadir + "/??"):
				c = configuration(datadir=stdir)
				self.config[c.name] = c
		for k,v in self.config.iteritems():
			v.setRootDatadir(self.datadir)

	def shouldstop(self):
		if self.softfail:
			return True
		if self.dry_run:
			return False
		if self.stoppath and os.path.exists(self.stoppath):
			self.addStopReason(self.stoppath + ' exists')
			return True
		if self.end is not None:
			now = datetime.datetime.now()
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

	def runstate(self, stu):
		"""Wrapper around runstate_inner to aid in runlog."""
		start_timestamp = timestamp()
		try:
			ok = self.runstate_inner(stu, start_timestamp)
		except Exception, e:
			ok = False
			sys.stderr.write(e)
			self.addStopReason(str(e))
			self.softfail = True
		if (not self.dry_run) and (self.runlog is not None):
			if ok:
				okmsg = 'ok'
			else:
				okmsg = 'FAILED'
			self.runlog.write('%s %s - %s %s\n' % (
				stu, start_timestamp, timestamp(), okmsg))
			self.runlog.flush()
		return ok

	def runstate_inner(self, stu, start_timestamp):
		"""This is the primary sequence of actions to run the solver on a state."""
		if not os.path.exists(stu):
			self.maybe_mkdir(stu)
		ctd = os.path.join(stu, start_timestamp)
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
					sys.stdout.write("O " + stu + ": " + line)
			except:
				pass
			try:
				for line in p.stderr:
					sys.stdout.write("E " + stu + ": " + line)
					lastlines(errorlines, 10, line)
			except:
				pass
			if p.returncode != 0:
				self.addStopReason("solver exited with status %d" % p.returncode)
				if errorlines:
					self.addStopReason('\n' + '\n'.join(errorlines))
				sys.stderr.write(self.stopreason + '\n')
				self.softfail = True
				return False
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
			ret = subprocess.Popen(cmd, cwd=ctd).wait()
			if ret != 0:
				self.addStopReason("tar g failed %d" % ret)
				sys.stderr.write(self.stopreason + '\n')
				self.softfail = True
				return False
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
		print "(cd %s && %s)" % (stu, ' '.join(cmd))
		if not self.dry_run:
			ret = subprocess.Popen(cmd, shell=False, cwd=stu).wait()
			if ret != 0:
				self.addStopReason("drend failed %d\n" % ret)
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
		

	def main(self, argv):
		self.readArgs(argv)
		self.checkSetup()
		self.loadConfigurations()
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

		if self.numthreads <= 1:
			print "running one thread"
			self.runthread()
		else:
			print "running %d threads" % self.numthreads
			self.lock = threading.Lock()
			threads = []
			for x in xrange(0, self.numthreads):
				threads.append(threading.Thread(target=runallstates.runthread, args=(self,)))
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
