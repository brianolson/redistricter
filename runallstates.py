#!/usr/bin/python
# Expects to run in the build dir with data/?? containing state data.
# Using setupstatedata.pl in the standard way should do this.

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
		if name is None:
			if datadir is None:
				raise Exception('one of name or datadir must not be None')
			m = configuration.datadir_state_re.match(datadir)
			if m is None:
				raise Exception('failed to get state name from datadir path "%s"' % datadir)
			self.name = m.group(1)
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
		empty lines and lines starting with '#' are ignored.
		
		$DATA is substituted with the global root datadir
		"""
		if isinstance(x, basestring):
			x = open(x, 'r')
		for line in x:
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
		pgeompath = os.path.join(self.datadir, 'geometry.pickle')
		if os.path.exists(pgeompath):
			f = open(pgeompath, 'rb')
			self.geom = pickle.load(f)
			f.close()
			datapath = os.path.join(self.datadir, stl + '.pb')
			pxpath = os.path.join(self.datadir, stl + '.mppb')
			new_args = [
				'-P', datapath, '-d', self.geom.numCDs(),
				'-o', self.name + '_final.dsz',
#				'--pngout', self.name + '_final.png',
#				'--pngW', geom.basewidth, '--pngH', geom.baseheight,
				]
			logging.debug('geom args %s', new_args)
			self.args = mergeArgs(new_args, self.args)
			logging.debug('merged args %s', self.args)
			new_drendargs = [
				'-P', datapath, '-d', self.geom.numCDs(),
				'--pngW', self.geom.basewidth * 4, '--pngH', self.geom.baseheight * 4,
				'--loadSolution', 'link1/bestKmpp.dsz',
				'--mppb', pxpath,
				'--pngout', 'link1/%s_final2.png' % self.name]
			logging.debug('geom drend args %s', new_drendargs)
			self.drendargs = mergeArgs(new_drendargs, self.drendargs)
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
		# datadir/statedir paths
		#self.statedirs = []
		# added with --config
		self.configArgList = []
		# added with --configdir
		self.configdir = None
		# map from STU/config-name to configuration objects
		self.config = {}
		# list of STU uppercase state abbreviations
		self.states = []
		# map from STU to arg string
		#self.basicargs = {}
		# map from STU to fully formed drend command line (string)
		#self.drendcmds = {}
		self.softfail = False
		self.stoppath = None
		self.stopreason = ''
		self.runlog = None  # file
		self.bestlog = None  # file
		self.bests = None  # map<stu, manybest.slog>

	def readBestLog(self, path):
		"""Read in a bestlog, loading latest state into memory."""
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
				if IsReadableFile(os.path.join(self.datadir, astu, "basicargs")) or IsReadableFile(os.path.join(self.datadir, astu, "geometry.pickle")):
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


	def loadConfigurations(self):
		for st in self.statearglist:
			assert st not in self.config
			self.config[st] = configuration(
				name=st, datadir=os.path.join(self.datadir, st))
		for cname in self.configArgList:
			c = configuration(
				name=os.path.split(cname)[-1], config=cname, dataroot=self.datadir)
			assert c.name not in self.config
			self.config[c.name] = c
		if self.configdir is not None:
			for cname in os.listdir(self.configdir):
				# skip some common directory cruft
				if cname.startswith('.') or cname.startswith('#') or cname.endswith('~') or cname.endswith(',v'):
					continue
				c = configuration(
					name=cname, config=os.path.join(self.configdir, cname),
					dataroot=self.datadir)
				assert c.name not in self.config
				self.config[c.name] = c
		if len(self.config) == 0:
			# get all the old defaults
			for stdir in glob.glob(self.datadir + "/??"):
				c = configuration(datadir=stdir)
				self.config[c.name] = c
		for k,v in self.config.iteritems():
			v.setRootDatadir(self.datadir)

	def readStatedirs(self):
		sys.stderr.write('readStatedirs is DISABLED\n')
		return
		#for s in self.statedirs:
		#	self.readStatedir(s)
	
	#TODO: delete this
	def readStatedir(self, s):
		"""Args for some state come from (in order of precedence, highest to lowest):
		./${stu}_basicargs
		./${stu}_drendcmd
		
		$state_data_dir/geometry.pickle
		
		$state_data_dir/basicargs
		$state_data_dir/drendcmd
		
		geometry.pickle is a measureGeometry.geom object generated by measureGeometry or as in setupstatedata.py . This is the preferred default.
		
		basicargs files should have one line of the form:
		-B data/${stu}/${stl}.${binsuffix} --pngout ${stu}_final.png -d $numdists -o ${stu}_final.dsz --pngW 640 --pngH 480
		"data/" will be replaced with a path to the specified data dir
		
		drendcmd files should have one line of the form:
		../drend -B ../data/${stu}/${stl}.${binsuffix} $distnumopt $pngsize --loadSolution bestKmpp.dsz -px ../data/${stu}/${stl}.mpout --pngout ${stu}_final2.png
		"../drend" and "../data/" will be replaced with appropriate paths.
		"""
		stu = s[-2:]
		stl = stu.lower()
		if (not self.statearglist) and os.path.exists(os.path.join(s, "norun")):
			return
		pgeompath = os.path.join(s, 'geometry.pickle')
		geom = None
		try:
			f = open(pgeompath, 'rb')
			geom = pickle.load(f)
			f.close()
		except:
			pass
		ba_path = stu + "_basicargs"
		if (not os.path.exists(ba_path)) and (geom is not None):
			self.basicargs[stu] = (
				'-B %s --pngout %s_final.png -d %d -o %s_final.dsz --pngW %d --pngH %d' %
				(os.path.join(s, stl + '.pb'), stu, geom.numCDs(), stu,
				 geom.basewidth, geom.baseheight))
		else:
			ba_path = os.path.join(s, "basicargs")
			if not os.path.exists(ba_path):
				sys.stderr.write("error: %s missing basicargs\n" % stu)
				return
			fin = open(ba_path, "r")
			ba = ""
			if fin:
				ba = fin.readline()
				fin.close()
				if ba:
					ba = ba.strip(" \t\n\r")
					ba = ba.replace("-B data", "-B " + self.datadir)
			else:
				return
			self.basicargs[stu] = ba
		dc_path = stu + "_drendcmd"
		if (not os.path.exists(dc_path)) and (geom is not None):
			drendpath = os.path.join(self.bindir, 'drend')
			datapath = os.path.join(s, stl + '.pb')
			pxpath = os.path.join(s, stl + '.mpout')
			self.drendcmds[stu] = (
				'%s -B %s -d %d --pngW %d --pngH %d --loadSolution link1/bestKmpp.dsz -px %s --pngout link1/%s_final2.png' % 
				(drendpath, datapath, geom.numCDs(), geom.basewidth * 4, geom.baseheight * 4,
				 pxpath, stu))
		else:
			dc_path = os.path.join(s, "drendcmd")
			fin = open(dc_path, "r")
			if fin:
				dc = fin.readline()
				fin.close()
				if dc:
					dc = dc.strip(" \t\n\r")
					dc = dc.lstrip("# \t")
					dc = dc.replace("../data", self.datadir)
					dc = dc.replace("../drend", os.path.join(self.bindir, "drend"))
					dc = dc.replace("--pngout ", "--pngout link1/")
					dc = dc.replace("--loadSolution ", "--loadSolution link1/")
					self.drendcmds[stu] = dc
		self.states.append(stu)

	def shouldstop(self):
		if self.softfail:
			return True
		if self.dry_run:
			return False
		if self.stoppath and os.path.exists(self.stoppath):
			self.stopreason = self.stoppath + ' exists'
			return True
		if self.end is not None:
			now = datetime.datetime.now()
			if now > self.end:
				self.stopreason = 'ran past end time (now=%s end=%s)' % (
					now, self.end)
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
		ok = self.runstate_inner(stu, start_timestamp)
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
		#ha = ""
		#ha_path = stu + "_handargs"
		#if not os.path.exists(ha_path):
		#	ha_path = os.path.join(self.datadir, stu, "handargs")
		#fin = open(ha_path, "r")
		#if fin:
		#	ha = fin.readline()
		#	fin.close()
		#	ha = ha.strip(" \t\n\r")
		#	ha = " " + ha
		ctd = os.path.join(stu, start_timestamp)
		self.maybe_mkdir(ctd)
		self.maybe_mkdir(os.path.join(ctd,"g"))
		statlog = os.path.join(ctd, "statlog")
		statsum = os.path.join(ctd, "statsum")
		if not self.dry_run:
			fout = open(statlog, "w")
			if not fout:
				self.stopreason = "could not open \"%s\"" % statlog
				sys.stderr.write(self.stopreason + "\n")
				self.softfail = True
				return False
			fout.close()
		cmd = niceArgs + [self.exe] + self.stdargs + self.solverMode + self.config[stu].args + self.extrargs
		print "(cd %s && \\\n%s )" % (ctd, cmd)
		if not self.dry_run:
			p = subprocess.Popen(cmd, shell=True, bufsize=4000, cwd=ctd,
				stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			errorlines = []
			if has_poll:
				errorlines = poll_run(p, stu)
			elif has_select:
				errorlines = select_run(p, stu)
			else:
				self.stopreason = 'has neither poll nor select\n'
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
				self.stopreason = "solver exited with status %d" % p.returncode
				if errorlines:
					self.stopreason += '\n' + '\n'.join(errorlines)
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
				self.stopreason = "gzip statlog failed %d" % ret
				sys.stderr.write(self.stopreason + '\n')
				self.softfail = True
				return False
		cmd = ["tar", "jcf", "g.tar.bz2", "g"]
		if self.dry_run or self.verbose:
			print "(cd %s && %s)" % (ctd, " ".join(cmd))
		if not self.dry_run:
			ret = subprocess.Popen(cmd, cwd=ctd).wait()
			if ret != 0:
				self.stopreason = "tar g failed %d" % ret
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
		drendargs = self.config[stu].drendargs
		final2_png = os.path.join("link1", stu + "_final2.png")
		if (drendargs and ((mb.they and
			not os.path.exists(os.path.join(stu, final2_png))) or self.dry_run)):
			cmd = [os.path.join(self.bindir, "drend")] + drendargs
			print "(cd %s && %s)" % (stu, cmd)
			if not self.dry_run:
				ret = subprocess.Popen(drendcmd, shell=True, cwd=stu).wait()
				if ret != 0:
					self.stopreason = "drend failed %d\n" % ret
					sys.stderr.write(self.stopreason + '\n')
					self.softfail = True
					return False
			start_png = os.path.join(self.datadir, stu, stu + "_start.png")
			ba_png = os.path.join("link1", stu + "_ba.png")
			cmd = ["convert", start_png, final2_png, "+append", ba_png]
			print "(cd %s && %s)" % (stu, " ".join(cmd))
			if not self.dry_run:
				subprocess.Popen(cmd, cwd=stu).wait()
			cmd = ["convert", ba_png, "-resize", "500x500", os.path.join("link1", stu + "_ba_500.png")]
			print "(cd %s && %s)" % (stu, " ".join(cmd))
			if not self.dry_run:
				subprocess.Popen(cmd, cwd=stu).wait()
		if (self.bestlog is not None) and mb.they:
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
		return True

	def main(self, argv):
		self.readArgs(argv)
		self.checkSetup()
		self.loadConfigurations()
		for c in self.config.itervalues():
			print c
		#self.readStatedirs()
		
		self.states = self.config.keys()
		
		# run in a different order each time in case we do partial runs, spread the work
		random.shuffle(self.states)

		print " ".join(self.states)

		rootdir = os.getcwd()

		self.stoppath = os.path.join(rootdir, "stop")
		
		if self.dry_run:
			self.numthreads = 1

		if self.numthreads <= 1:
			print "running one thread"
			while not self.shouldstop():
				time.sleep(1)
				for stu in self.states:
					good = self.runstate(stu)
					#print "good=%s shouldstop=%s softfail=%s" % (good, shouldstop(), softfail)
					if (not good) or self.shouldstop():
						break
				if self.dry_run:
					break
		else:
			print "running %d threads" % self.numthreads
			qlock = threading.Lock()
			qpos = [0]
			def runthread(ql, qp):
				while not self.shouldstop():
					time.sleep(1)
					ql.acquire()
					stu = self.states[qp[0]]
					qp[0] = (qp[0] + 1) % len(self.states)
					ql.release()
					self.runstate(stu)
			threads = []
			for x in xrange(0, self.numthreads):
				threads.append(threading.Thread(target=runthread, args=(qlock, qpos)))
			for x in threads:
				x.start()
			for x in threads:
				x.join()

		if not self.dry_run:
			print self.stopreason
			if os.path.exists(self.stoppath):
				os.remove(self.stoppath)

if __name__ == "__main__":
	it = runallstates()
	it.main(sys.argv)
