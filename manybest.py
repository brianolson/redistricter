#!/usr/bin/python

import getopt
import glob
import os
import re
import shutil
import sys

usage = (
"""usage: $0 [-n ngood][-bad badthresh][-d out-dir]
  [-rmbad][-rmempty][-mvbad]
  [statsum files, statlog files, or directories where they can be found]

If no statsum or statlog files are found through arguments,
./*/statsum are processed.
This is a reasonable default if mrun2.pl or runallstates.pl was used.

  -n ngood     Keep the top ngood solutions. Others may be partially or fully purged.
  -bad badthresh  Results with a Km/person score below badthresh may be purged.
  -d out-dir   Where to write out HTML and copy images to to show the best.
  -rmbad       Removes a large amount of intermediate data from bad results.
  -rmempty     Remove entirely solutions that are empty (likely solver bug)
  -mvbad       Move bad solutions (ngood or badthresh) into old/
""")

# same as kmppspreadplot.py
kmppspread = re.compile(
    r".*Best Km/p: Km/p=([0-9.]+) spread=([0-9.]+).*")
kmpp_re_a = re.compile(r".*Best Km\/p:.*Km\/p=([0-9.]+)")
kmpp_re_b = re.compile(r".*Best Km\/p: ([0-9.]+)")

_re_list = [kmppspread, kmpp_re_a, kmpp_re_b]


class NoRunsException(Exception):
	pass

class slog(object):
	"""A log summary object"""
	
	def __init__(self, root, kmpp, spread, png, text):
		self.root = root
		self.kmpp = kmpp
		self.spread = spread
		self.png = png
		self.text = text
	
	def __repr__(self):
		return """slog("%s", "%s", %d, "%s", %d chars of text)""" % (self.root, self.kmpp, self.spread, self.png, len(self.text))


class manybest(object):
	def __init__(self):
		self.odir = "best"
		self.root = None
		self.log_paths = []
		# nlim, size of best table to write
		self.nlim = None
		# ngood, keep only this many before moving them to old/
		self.ngood = None
		self.badkmpp = None
		self.badlist = []
		self.rmbad = False
		self.rmempty = False
		self.mvbad = False
		self.verbose = None
		self.dry_run = False
		self.slogs = None
		self.empties = None
		# list of good runs found
		self.they = None

	def addSlog(self, path):
		if self.root:
			rp = os.path.realpath(path)
			if rp.startswith(self.root):
				self.log_paths.append(path[len(self.root)+1:])
			else:
				self.log_paths.append(path)
		else:
			self.log_paths.append(path)

	def maybeAddSlogDir(self, path):
		pa = os.path.join(path, "statsum")
		if os.access(pa, os.F_OK|os.R_OK):
			self.addSlog(pa)
			return True
		pa = os.path.join(path, "statlog")
		if os.access(pa, os.F_OK|os.R_OK):
			self.addSlog(pa)
			return True
		return False

	def setRoot(self, path):
		self.root = os.path.realpath(path)
		if not os.path.isdir(self.root):
			if self.dry_run:
				return
			raise Error("-root must specify a directory")
		for f in os.listdir(self.root):
			fpath = os.path.join(self.root, f)
			if os.path.isdir(fpath) and not os.path.islink(fpath):
				self.maybeAddSlogDir(fpath)
		self.odir = os.path.join(self.root, self.odir)
	
	def parseOpts(self, argv):
		argv = argv[1:]
		while len(argv) > 0:
			arg = argv.pop(0)
			if arg == "-d":
				self.odir = argv.pop(0)
			elif arg == "-n":
				self.nlim = argv.pop(0)
			elif arg == "-ngood":
				self.ngood = argv.pop(0)
			elif arg == "-bad":
				self.badkmpp = argv.pop(0)
			elif (arg == "-h") or (arg == "--help"):
				print usage
				sys.exit(0)
			elif arg == "-rmbad":
				self.rmbad = True
			elif arg == "-rmempty":
				self.rmempty = True
			elif arg == "-mvbad":
				self.mvbad = True
			elif arg == "-root":
				self.setRoot(argv.pop(0))
			elif arg == "-verbose":
				self.verbose = sys.stderr
			elif arg == "-dryrun":
				self.dry_run = True
				self.verbose = sys.stderr
			elif os.access(arg, os.F_OK|os.R_OK):
				self.addSlog(arg)
			elif os.path.isdir(arg) and self.maybeAddSlogDir(arg):
				pass
			else:
				errstr = "bogus arg \"%s\"\n" % arg
				sys.stderr.write(errstr)
				raise Error(errstr)

	def parseLog(self, fin):
		"""
		fin: iterable source of lines
		return (float kmpp, str[] lines)"""
		lines = []
		kmpp = None
		spread = None
		for line in fin:
			m = None
			for tre in _re_list:
				m = tre.match(line)
				if m:
					break
			if m:
				kmpp = float(m.group(1))
				if m.lastindex >= 2:
					spread = int(m.group(2))
				lines.append(line[1:])
			elif line[0] == "#":
				lines.append(line[1:])
		return (kmpp, spread, lines)

	def skimLogs(self, loglist=None):
		"""return (slog[] they, string[] empties)"""
		they = []
		empties = []
		if loglist is None:
			loglist = self.log_paths
		for fn in loglist:
			root = os.path.dirname(fn)
			if not root:
				raise Error("could not find root of \"$fn\"\n")
			if root == "link1":
				continue
			if self.root:
				fn = os.path.join(self.root, fn)
				if not os.path.isdir(os.path.join(self.root, root)):
					continue
			else:
				if not os.path.isdir(root):
					continue
			if self.verbose:
				self.verbose.write("scanning %s\n" % fn)
			fin = open(fn, "r")
			kmpp, spread, lines = self.parseLog(fin)
			fin.close()
			if kmpp is None:
				empties.append(root)
				continue
			if self.root:
				png_root = os.path.join(self.root, root)
			else:
				png_root = root
			png_list = glob.glob(os.path.join(png_root, '??_final2.png'))
			png = None
			if png_list and len(png_list) == 1:
				png = png_list[0]
			if png is None or not os.path.isfile(png):
				png = os.path.join(png_root, "bestKmpp.png")
			if not os.path.isfile(png):
				sys.stderr.write("no %s\n" % png)
				continue
			if (self.badkmpp is not None) and (kmpp > self.badkmpp):
				badlist.append(root)
			they.append(slog(root, kmpp, spread, png, "<br/>".join(lines)))
		self.they = they
		self.empties = empties
		return (they, empties)

	def copyPngs(self, they):
		i = 1
		for t in they:
			destpngpath = os.path.join(self.odir, "%d.png" % i)
			if self.verbose:
				self.verbose.write("cp %s %s\n" % (t.png, destpngpath))
			if not self.dry_run:
				shutil.copyfile(t.png, destpngpath)
			i += 1
	
	def writeBestsTable(self, they, fpart):
		fpart.write("""<table border="1">""")
		i = 1
		for t in they:
			fpart.write(
				"""<tr><td><img src="%d.png"></td><td>run "%s"<br/>%s</td></tr>\n"""
				% (i, t.root, t.text))
			i += 1
			if (self.nlim is not None) and (i > self.nlim):
				break
		fpart.write("""</table>""")

	def mergeIndexParts(self):
		if self.verbose:
			self.verbose.write("(cd %s; cat .head.html .part.html .tail.html > index.html)\n" % self.odir)
		if self.dry_run:
			return
		findex = open(os.path.join(self.odir, "index.html"), "w")
		for finame in [".head.html", ".part.html", ".tail.html"]:
			fipath = os.path.join(self.odir, finame)
			if not os.path.exists(fipath):
				sys.stderr.write("warning: no file \"%s\"\n" % fipath)
				continue
			fi = open(fipath, "r")
			while True:
				buf = fi.read(30000)
				if len(buf) <= 0:
					break
				findex.write(buf)
			fi.close()
		findex.close()

	def setLink1(self, path):
		if self.verbose:
			self.verbose.write("setLink1(%s)\n" % path)
		if self.root:
			link1path = os.path.join(self.root, "link1")
		else:
			link1path = "link1"
		if os.path.lexists(link1path):
			if os.path.islink(link1path):
				if self.verbose:
					self.verbose.write("rm %s\n" % link1path)
				if not self.dry_run:
					os.unlink(link1path)
			else:
				sys.stderr.write("link1 exists but is not a link\n")
				raise Error("link1 exists but is not a link\n")
		def verboseSymlink(a, b):
			if self.verbose:
				self.verbose.write("ln -s %s %s\n" % (a, b))
			if not self.dry_run:
				os.symlink(a, b)
		if self.root:
			verboseSymlink(path, link1path)
		else:
			path = os.path.realpath(path)
			verboseSymlink(path, link1path)

	def getBadlist(self, they):
		if (self.ngood is not None) and (self.ngood < len(they)):
			return [x.root for x in they[self.ngood:]]
		return None

	def rmBadlistStepdata(self, badlist):
		bgl = []
		for b in badlist:
			if self.root:
				b = os.path.join(self.root, b)
			gdir = os.path.join(b, "g")
			if os.path.exists(gdir):
				bgl.append(gdir)
			garch = os.path.join(b, "g.tar.bz2")
			if os.path.exists(garch):
				bgl.append(garch)
		if bgl:
			if self.verbose:
				self.verbose.write("bad best kmpp:\nrm -rf " + " ".join(bgl) + "\n")
			if not self.dry_run:
				for x in bgl:
					os.unlink(x)

	def moveBadToOld(self, badlist):
		if self.verbose:
			self.verbose.write("move bad best kmpp to old dir\n")
		oldpath = "old"
		if self.root:
			oldpath = os.path.join(self.root, "old")
		if not os.path.isdir(oldpath):
			if self.verbose:
				self.verbose.write("mkdir %s\n" % oldpath)
			if not self.dry_run:
				os.mkdir(oldpath)
		for b in badlist:
			bpath = b
			if self.root:
				bpath = os.path.join(self.root, b)
			oldsub = os.path.join(oldpath, b)
			if self.verbose:
				self.verbose.write("mv %s %s\n" % (bpath, oldsub))
			if not self.dry_run:
				shutil.move(bpath, oldsub)
	
	def handleEmpties(self, empties):
		if empties:
			# don't delete the last one, in case it's still active
			empties.pop(-1)
		if empties:
			if self.verbose:
				self.verbose.write("empty solution:\nrm -rf " + " ".join(empties) + "\n")
			for eroot in empties:
				if self.root:
					eroot = os.path.join(self.root, eroot)
				if not self.dry_run:
					shutil.rmtree(eroot)
		
	def main(self, argv):
		self.parseOpts(argv)
		self.run()

	def run(self):
		if (self.ngood is not None) and (self.nlim is None):
			self.nlim = self.ngood
		if not self.log_paths:
			self.log_paths = glob.glob("*/statsum")
		if not self.log_paths:
			sys.stderr.write("no logs to process\n")
			if self.dry_run:
				return
			sys.stderr.write(usage)
			sys.exit(1)
		self.skimLogs(self.log_paths)
		if not self.they:
			raise NoRunsException("no good runs found\n")
		self.they.sort(cmp=lambda a, b: cmp(a.kmpp, b.kmpp))
		they = self.they
		empties = self.empties
		if self.odir:
			if not os.path.isdir(self.odir):
				os.makedirs(self.odir)
			self.copyPngs(they)
			if self.verbose:
				self.verbose.write("write .part.html\n")
			if not self.dry_run:
				fpart = open(os.path.join(self.odir, ".part.html"), "w")
				self.writeBestsTable(they, fpart)
				fpart.close()
			self.mergeIndexParts()
			self.setLink1(they[0].root)

		badlist = self.getBadlist(they)
		if badlist:
			print "badlist: " + " ".join(badlist)
			if self.rmbad:
				self.rmBadlistStepdata(badlist)
			if self.mvbad:
				self.moveBadToOld(badlist)
		if self.rmempty and empties:
			empties.sort()
			self.handleEmpties(empties)


if __name__ == "__main__":
	it = manybest()
	it.main(sys.argv)
