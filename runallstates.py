#!/usr/bin/python2.5
# Expects to run in the build dir with data/?? containing state data.
# Using setupstatedata.pl in the standard way should do this.

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

has_poll = "poll" in dir(select)
has_select = "select" in dir(select)

datadir = os.environ.get("DISTRICT_DATA")
if datadir is None:
	datadir = os.path.join(os.getcwd(), "data")

bindir = None
exe = None
stdargs = " --blankDists --sLog g/ --statLog statlog --d2 "
start = datetime.datetime.now();
end = None
statearglist = []
numthreads = 1


def IsExecutableDir(path):
	st = os.stat(path)
	# FIXME?, test that using the dir is allowed? 
	return stat.S_ISDIR(st[stat.ST_MODE])

def IsExecutableFile(path):
	return os.access(path, os.X_OK)
#	st = os.stat(path)
	# FIXME?, test that current user may execute
#	return stat.S_ISREG(st[stat.ST_MODE])

def IsReadableFile(path):
	st = os.stat(path)
	# FIXME, check permissions
	return stat.S_ISREG(st[stat.ST_MODE])

i = 1
while i < len(sys.argv):
	arg = sys.argv[i]
	if arg == "--data":
		i += 1
		datadir = sys.argv[i]
	elif arg == "--bin":
		i += 1
		bindir = sys.argv[i]
		if not IsExecutableFile( os.path.join(bindir, "districter2")):
			sys.stderr.write( "bogus bin dir \"%s\" does not contain districter2\n" % bindir )
			sys.exit(1)
	elif arg == "--exe":
		i += 1
		maybe_exe = sys.argv[i]
		if not IsExecutableFile( maybe_exe ):
			sys.stderr.write( "bogus exe \"%s\" is not executable\n" % maybe_exe )
			sys.exit(1)
		exe = maybe_exe
	elif arg == "--runsecs":
		i += 1
		end = start + datetime.timedelta(seconds=float(sys.argv[i]))
	else:
		astu = arg.upper()
		if IsReadableFile(os.path.join(datadir, astu, "basicargs")):
			statearglist.append(astu)
		else:
			sys.stderr.write("%s: bogus arg \"%s\"\n" % (sys.argv[0], arg))
			sys.exit(1)
	i += 1

if exe is None:
	if bindir is None:
		bindir = os.getcwd()
	exe = os.path.join(bindir, "districter2")
	if not IsExecutableFile(exe):
		sys.stderr.write("bogus exe \"%s\" is not executable\n" % exe)
		sys.exit(1)

if not IsExecutableDir(datadir):
	sys.stderr.write("bogus data dir \"%s\"\n" % datadir)

statedirs = []
if statearglist:
	statedirs = map(lambda x: os.path.join(datadir, x), statearglist)
else:
	statedirs = glob.glob(datadir + "/??")

states = []
basicargs = {}
drendcmds = {}

#b_data = re.compile("-B data")

for s in statedirs:
	stu = s[-2:]
	if (not statearglist) and os.path.exists(os.path.join(s, "norun")):
		continue
	fin = open(os.path.join(s, "basicargs"), "r")
	ba = ""
	if fin:
		ba = fin.readline()
		fin.close()
		if ba:
			ba = ba.strip()
			ba = ba.replace("-B data", "-B " + datadir)
	else:
		continue
	basicargs[stu] = ba
	fin = open(os.path.join(s, "drendcmd"), "r")
	if fin:
		dc = fin.readline()
		fin.close()
		if dc:
			dc = dc.strip()
			dc = dc.lstrip("# \t")
			dc = dc.replace("../data", datadir)
			dc = dc.replace("../drend", os.path.join(bindir, "drend"))
			dc = dc.replace("--pngout ", "--pngout link1/")
			dc = dc.replace("--loadSolution ", "--loadSolution link1/")
			drendcmds[stu] = dc
	states.append(stu)

# run in a different order each time in case we do partial runs, spread the work
random.shuffle(states)

print " ".join(states)

def timestamp():
	now = datetime.datetime.now()
	return "%04d%02d%02d_%02d%02d%02d" % (now.year, now.month, now.day,
		now.hour, now.minute, now.second)

nice = ""
if IsExecutableFile("/bin/nice"):
	nice = "/bin/nice -n 20 "
elif IsExecutableFile("/usr/bin/nice"):
	nice = "/usr/bin/nice -n 20 "

rootdir = os.getcwd()

stoppath = os.path.join(rootdir, "stop")

manybest = None
if bindir is not None:
	manybest = os.path.join(bindir, "manybest.pl")
	if not IsExecutableFile(manybest):
		manybest = None

softfail = False

def shouldstop():
	if softfail:
		return True
	if os.path.exists(stoppath):
		return True
	if (end is not None) and (datetime.datetime.now() < end):
		return True
	return False

def poll_run(p, stu):
	poller = select.Poll()
	poller.register(p.stdout, select.POLLIN | select.POLLPRI )
	poller.register(p.stderr, select.POLLIN | select.POLLPRI )
	while p.poll() is None:
		for (fd, event) in poller.poll(500):
			line = fd.readline()
			if p.stdout == fd:
				sys.stdout.write("O " + stu + ": " + line)
			elif p.stderr == fd:
				sys.stdout.write("E " + stu + ": " + line)
			else:
				sys.stdout.write("? " + stu + ": " + line)

def select_run(p, stu):
	while p.poll() is None:
		(il, ol, el) = select.select([p.stdout, p.stderr], [], [], 0.5)
		for fd in il:
			line = fd.readline()
			if not line:
				pass
			elif p.stdout == fd:
				sys.stdout.write("O " + stu + ": " + line)
			elif p.stderr == fd:
				sys.stdout.write("E " + stu + ": " + line)
			else:
				sys.stdout.write("? " + stu + ": " + line)

def runstate(stu):
	if not os.path.exists(stu):
		os.mkdir(stu)
	ha = ""
	fin = open(os.path.join(datadir, stu, "handargs"), "r")
	if fin:
		ha = fin.readline()
		fin.close()
		ha.strip()
		ha = " " + ha
	ctd = os.path.join(stu, timestamp())
	os.mkdir(ctd)
	os.mkdir(os.path.join(ctd,"g"))
	#os.chdir(ctd)
	cmd = nice + exe + stdargs + basicargs[stu] + ha
	print "chdir " + ctd
	print cmd
	p = subprocess.Popen(cmd, shell=True, bufsize=4000, cwd=ctd,
		stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	if has_poll:
		poll_run(p, stu)
	elif has_select:
		select_run(p, stu)
	if p.returncode != 0:
		sys.stderr.write("solver exited with status %d\n" % p.returncode)
		softfail = True
		return False
	statlog = os.path.join(ctd, "statlog")
	fin = open(statlog, "r")
	fout = open(os.path.join(ctd, "statsum"), "w")
	for line in fin:
		if line[0] == "#":
			fout.write(line)
	fout.close()
	fin.close()
	subprocess.check_call(["gzip", statlog])
	subprocess.Popen(["tar", "jcf", "g.tar.bz2", "g"], cwd=ctd).wait()
	subprocess.Popen(["rm", "-rf", "g"], cwd=ctd).wait()
	if manybest:
		cmd = [manybest, "-ngood", "15", "-mvbad", "-rmbad", "-rmempty", "-n", "10"]
		print " ".join(cmd)
		subprocess.Popen(cmd, cwd=stu ).wait()
		drendcmd = drendcmds.get(stu)
		final2_png = os.path.join("link1", stu + "_final2.png")
		if (drendcmd and not 
			os.path.exists(final2_png)):
			print drendcmd
			subprocess.Popen(drendcmd, cwd=stu).wait()
			start_png = os.path.join(datadir, stu, stu + "_start.png")
			ba_png = os.path.join("link1", stu + "_ba.png")
			cmd = ["convert", start_png, final2_png, "+append", ba_png]
			print " ".join(cmd)
			subprocess.Popen(cmd, cwd=stu).wait()
			cmd = ["convert", ba_png, "-resize", "500x500", os.path.join("link1", stu + "_ba_500.png")]
			print " ".join(cmd)
			subprocess.Popen(cmd, cwd=stu).wait()
	return True

if numthreads <= 1:
	print "running one thread"
	while not shouldstop():
		time.sleep(1)
		for stu in states:
			good = runstate(stu)
			print "good=%s shouldstop=%s softfail=%s" % (good, shouldstop(), softfail)
			if (not good) or shouldstop():
				break
else:
	print "running %d threads" % numthreads
	qlock = threading.Lock()
	qpos = [0]
	def runthread(ql, qp):
		while not shouldstop():
			time.sleep(1)
			ql.acquire()
			stu = states[qp[0]]
			qp[0] = (qp[0] + 1) % len(states)
			ql.release()
			runstate(stu)
	threads = []
	for x in xrange(0, numthreads):
		threads.append(threading.Thread(target=runthread, args=(qlock, qpos)))
	for x in threads:
		x.join()

os.remove(stoppath)
