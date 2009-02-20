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

kmpp_re_a = re.compile(r".*Best Km\/p:.*Km\/p=([0-9.]+)")
kmpp_re_b = re.compile(r".*Best Km\/p: ([0-9.]+)")

class slog(object):
	def __init__(self, root, kmpp, png, text):
		self.root = root
		self.kmpp = kmpp
		self.png = png
		self.text = text
	
	def __repr__(self):
		return """slog("%s", "%s", "%s", %d chars of text)""" % (self.root, self.kmpp, self.png, len(self.text))

class manybest(object):
	def __init__(self):
		self.odir = "best"
		self.slogs = []
		self.any = False
		self.nlim = None
		self.ngood = None
		self.badkmpp = None
		self.badlist = []
		self.rmbad = False
		self.rmempty = False
		self.mvbad = False

	def parseOpts(self, argv):
		argv.pop(0)
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
			elif os.access(arg, os.F_OK|os.R_OK):
				self.slogs.append(arg)
				self.any = True
			elif os.access(arg + "/statsum", os.F_OK|os.R_OK):
				self.slogs.append(arg + "/statsum")
				self.any = True
			elif os.access(arg + "/statlog", os.F_OK|os.R_OK):
				self.slogs.append(arg + "/statlog")
				self.any = True
			else:
				errstr = "bogus arg \"%s\"\n" % arg
				sys.stderr.write(errstr)
				raise errstr
	
	def skimLogs(self, loglist):
		"""return (slog[] they, string[] empties)"""
		they = []
		empties = []
		for fn in loglist:
			root = os.path.dirname(fn)
			if not root:
				raise "could not find root of \"$fn\"\n"
			if root == "link1":
				continue
			if not os.path.isdir(root):
				continue
			fin = open(fn, "r")
			lines = []
			kmpp = None
			for line in fin:
				m = kmpp_re_a.match(line)
				if not m:
					m = kmpp_re_b.match(line)
				if m:
					kmpp = float(m.group(1))
					lines.append(line[1:])
				elif line[0] == "#":
					lines.append(line[1:])
			fin.close()
			if kmpp is None:
				empties.append(root)
				continue
			png = root + "/bestKmpp.png"
			if not os.path.isfile(png):
				sys.stderr.write("no %s\n" % png)
				continue
			if (self.badkmpp is not None) and (kmpp > self.badkmpp):
				badlist.append(root)
			they.append(slog(root, kmpp, png, "<br/>".join(lines)))
		return (they, empties)
	
	def main(self, argv):
		self.parseOpts(argv)
		if (self.ngood is not None) and (self.nlim is None):
			self.nlim = self.ngood
		if not self.any:
			self.slogs = glob.glob("*/statsum")
		if not self.slogs:
			sys.stderr.write("no logs to process\n")
			sys.stderr.write(usage)
			sys.exit(1)
		they, empties = self.skimLogs(self.slogs)
		if not they:
			raise Exception("no good runs found\n")
		they.sort(cmp=lambda a, b: cmp(a.kmpp, b.kmpp))
		if not os.path.isdir(self.odir):
			os.makedirs(self.odir)
		fpart = open(os.path.join(self.odir, ".part.html")
		fpart.write("""<table border="1">""")
		i = 1
		for t in they:
			print "%9f %s" % (t.kmpp, t.root)
			shutil.copyfile(t.png, os.path.join(self.odir, "%d.png" % i))
			fpart.write(
				"""<tr><td><img src="%d.png"></td><td>run "%s"<br/>%s</td></tr>\n"""
				% (i, t.root, t.text))
			i += 1
			if (self.nlim is not None) and (i > self.nlim):
				break
		fpart.write("""</table>""")
		fpart.close()
		findex = open(os.path.join(self.odir, "index.html"), "w")
		for finame in [".head.html", ".part.html", ".tail.html"]:
			fi = open(os.path.join(self.odir, finame), "r")
			while True:
				buf = fi.read(30000)
				if len(buf) <= 0:
					break
				findex.write(buf)
			fi.close()
		if os.path.exists("link1"):
			if os.path.islink("link1"):
				os.unlink("link1")
			else:
				sys.stderr.write("link1 exists but is not a link\n")
				raise Error("link1 exists but is not a link\n")
		os.symlink(they[0].root, "link1")
		
		if (self.ngood is not None) and (self.ngood < len(they)):
			badlist = [x.root for x in they[self.ngood:]]
			print "badlist: " + " ".join(badlist)
#		print repr(they)

if __name__ == "__main__":
	it = manybest()
	it.main(sys.argv)

perl = """

@bgl = ();
foreach $b ( @badlist ) {
  if ( -e "${b}/g" ) {
    push @bgl, "${b}/g";
  }
  if ( -e "${b}/g.tar.bz2" ) {
    push @bgl, "${b}/g.tar.bz2";
  }
}
if ( @bgl ) {
  print "bad best kmpp:\n";
  @cmd = ("rm", "-rf", @bgl);
  if ( $rmbad ) {
    system @cmd;
  } else {
    print join(" ", @cmd) . "\n";
  }
}
if ( @empties ) {
  # don't delete the last one, in case it's still active
  pop @empties;
}
if ( @empties ) {
  print "empty solution:\n";
  @cmd = ("rm", "-rf", @empties);
  if ( $rmempty ) {
    system @cmd;
  } else {
    print join(" ", @cmd) . "\n";
  }
}
if ( @badlist ) {
  print "move bad best kmpp to old dir\n";
  @cmd = ("mv", @badlist, "old");
  if ( $mvbad ) {
    if ( ! -d "old" ) {
      mkdir("old") or die "could not mkdir(\"old\"): $!";
    }
    system @cmd;
  } else {
    print join(" ", @cmd) . "\n";
  }
}
"""
