#!/usr/bin/python
# Expects to run in the build dir with data/?? containing state data.
# Using setupstatedata.pl in the standard way should do this.

import cPickle as pickle
import datetime
import glob
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

nice = getNiceCommandFragment()

def poll_run(p, stu):
	poller = select.poll()
	poller.register(p.stdout, select.POLLIN | select.POLLPRI )
	poller.register(p.stderr, select.POLLIN | select.POLLPRI )
	while p.poll() is None:
		for (fd, event) in poller.poll(500):
			if p.stdout.fileno() == fd:
				line = p.stdout.readline()
				sys.stdout.write("O " + stu + ": " + line)
			elif p.stderr.fileno() == fd:
				line = p.stderr.readline()
				sys.stdout.write("E " + stu + ": " + line)
			else:
				sys.stdout.write("? %s fd=%d\n" % (stu, fd))

def select_run(p, stu):
	while p.poll() is None:
		(il, ol, el) = select.select([p.stdout, p.stderr], [], [], 0.5)
		for fd in il:
			if (p.stdout.fileno() == fd) or (fd == p.stdout):
				line = p.stdout.readline()
				sys.stdout.write("O " + stu + ": " + line)
			elif (p.stderr.fileno() == fd) or (fd == p.stderr):
				line = p.stderr.readline()
				sys.stdout.write("E " + stu + ": " + line)
			else:
				sys.stdout.write("? %s fd=%s\n" % (stu, fd))


class runallstates(object):
	def __init__(self):
		self.datadir = os.environ.get("DISTRICT_DATA")
		if self.datadir is None:
			self.datadir = os.path.join(os.getcwd(), "data")
		self.bindir = None
		self.exe = None
		self.d2args = " --d2 --popRatioFactorPoints 0,1.4,30000,1.4,80000,500,100000,50,120000,500 -g 150000 "
		self.stdargs = " --blankDists --sLog g/ --statLog statlog --maxSpreadFraction 0.01 "
		self.start = datetime.datetime.now();
		self.end = None
		self.statearglist = []
		self.numthreads = 1
		self.extrargs = ""
		self.dry_run = False
		self.verbose = 1
		self.statedirs = []
		self.states = []
		self.basicargs = {}
		self.drendcmds = {}
		self.softfail = False
		self.stoppath = None
		self.runlog = None  # file
		self.bestlog = None  # file
		self.bests = None  # map<stu, manybest.slog>

	def readBestLog(self, path):
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
		try:
			self.bests = self.readBestLog(path)
		except Exception, e:
			sys.stderr.write("readBestLog failed: %s\n" % e)
		if self.bests is None:
			self.bests = {}
		self.bestlog = open(path, "a")

	def openRunLog(self, path):
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
				self.end = start + datetime.timedelta(seconds=float(argv[i]))
			elif arg == "--":
				i += 1
				self.extrargs = " " + argv[i]
			elif arg == "--threads":
				i += 1
				self.numthreads = int(argv[i])
			elif (arg == "--dry-run") or (arg == "-n"):
				self.dry_run = True
			elif arg == "--d2":
				self.extrargs = d2args
			elif arg == "--runlog":
				i += 1
				self.openRunLog(argv[i])
			elif arg == "--bestlog":
				i += 1
				self.openBestLog(argv[i])
			else:
				astu = arg.upper()
				if IsReadableFile(os.path.join(self.datadir, astu, "basicargs")):
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


		if self.statearglist:
			self.statedirs = map(lambda x: os.path.join(self.datadir, x), self.statearglist)
		else:
			self.statedirs = glob.glob(self.datadir + "/??")

	def readStatedirs(self):
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
		for s in self.statedirs:
			stu = s[-2:]
			stl = stu.lower()
			if (not self.statearglist) and os.path.exists(os.path.join(s, "norun")):
				continue
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
					continue
				fin = open(ba_path, "r")
				ba = ""
				if fin:
					ba = fin.readline()
					fin.close()
					if ba:
						ba = ba.strip(" \t\n\r")
						ba = ba.replace("-B data", "-B " + self.datadir)
				else:
					continue
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
			return True
		if (self.end is not None) and (datetime.datetime.now() < self.end):
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
		ha = ""
		ha_path = stu + "_handargs"
		if not os.path.exists(ha_path):
			ha_path = os.path.join(self.datadir, stu, "handargs")
		fin = open(ha_path, "r")
		if fin:
			ha = fin.readline()
			fin.close()
			ha = ha.strip(" \t\n\r")
			ha = " " + ha
		ctd = os.path.join(stu, start_timestamp)
		self.maybe_mkdir(ctd)
		self.maybe_mkdir(os.path.join(ctd,"g"))
		statlog = os.path.join(ctd, "statlog")
		statsum = os.path.join(ctd, "statsum")
		if not self.dry_run:
			fout = open(statlog, "w")
			if not fout:
				sys.stderr.write("could not open \"%s\"\n" % statlog)
				self.softfail = True
				return False
			fout.close()
		cmd = nice + self.exe + self.stdargs + self.basicargs[stu] + ha + self.extrargs
		print "(cd %s && \\\n%s )" % (ctd, cmd)
		if not self.dry_run:
			p = subprocess.Popen(cmd, shell=True, bufsize=4000, cwd=ctd,
				stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			if has_poll:
				poll_run(p, stu)
			elif has_select:
				select_run(p, stu)
			try:
				for line in p.stdin:
					sys.stdout.write("O " + stu + ": " + line)
			except:
				pass
			try:
				for line in p.stderr:
					sys.stdout.write("E " + stu + ": " + line)
			except:
				pass
			if p.returncode != 0:
				sys.stderr.write("solver exited with status %d\n" % p.returncode)
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
				sys.stderr.write("gzip statlog failed %d\n" % ret)
				self.softfail = True
				return False
		cmd = ["tar", "jcf", "g.tar.bz2", "g"]
		if self.dry_run or self.verbose:
			print "(cd %s && %s)" % (ctd, " ".join(cmd))
		if not self.dry_run:
			ret = subprocess.Popen(cmd, cwd=ctd).wait()
			if ret != 0:
				sys.stderr.write("tar g failed %d\n" % ret)
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
		mb.setRoot(stu)
		mb.dry_run = self.dry_run
		#./manybest.py -ngood 15 -mvbad -rmbad -rmempty -n 10
		try:
			mb.run()
		except manybest.NoRunsException:
			pass
		drendcmd = self.drendcmds.get(stu)
		final2_png = os.path.join("link1", stu + "_final2.png")
		if (drendcmd and mb.they and
			not os.path.exists(os.path.join(stu, final2_png))):
			print "(cd %s && %s)" % (stu, drendcmd)
			if not self.dry_run:
				ret = subprocess.Popen(drendcmd, shell=True, cwd=stu).wait()
				if ret != 0:
					sys.stderr.write("drend failed %d\n" % ret)
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
		self.readStatedirs()
		
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
			if os.path.exists(self.stoppath):
				os.remove(self.stoppath)

if __name__ == "__main__":
	it = runallstates()
	it.main(sys.argv)
