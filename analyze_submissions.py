#!/usr/bin/python


import cgi
import os
import re
import sys
import sqlite3
import subprocess
import tarfile

import runallstates


def scandir(path):
	"""Return a list of paths of .tar.gz submissions."""
	out = []
	for root, dirnames, filenames in os.walk(path):
		for fname in filenames:
			if fname.endswith('.tar.gz'):
				out.append(os.path.join(root, fname))
	return out


def elementAfter(haystack, needle):
	isNext = False
	for x in haystack:
		if isNext:
			return x
		if x == needle:
			isNext = True
	return None


# Example analyze output:
# generation 0: 21.679798418 Km/person
# population avg=634910 std=1707.11778
# max=638656 (dist# 10)  min=632557 (dist# 7)  median=634306 (dist# 6)

kmppRe = re.compile(r'([0-9.]+)\s+Km/person')
maxMinRe = re.compile(r'max=([0-9]+).*min=([0-9]+)')


def loadDatadirConfigurations(configs, datadir, statearglist=None, configPathFilter=None):
	"""Store to configs[config name]."""
	for xx in os.listdir(datadir):
		if not os.path.isdir(os.path.join(datadir, xx)):
			logging.debug('data/"%s" not a dir', xx)
			continue
		stu = xx.upper()
		if statearglist and stu not in statearglist:
			logging.debug('"%s" not in state arg list', stu)
			continue
		configdir = os.path.join(datadir, stu, 'config')
		if not os.path.isdir(configdir):
			logging.debug('no %s/config', xx)
			continue
		for variant in os.listdir(configdir):
			if ignoreFile(variant):
				logging.debug('ignore file %s/config/"%s"', xx, variant)
				continue
			cpath = os.path.join(datadir, xx, 'config', variant)
			if configPathFilter and (not configPathFilter(cpath)):
				logging.debug('filter out "%s"', cpath)
				continue
			cname = stu + '_' + variant
			configs[cname] = configuration(
				name=cname,
				datadir=os.path.join(datadir, xx),
				config=cpath,
				dataroot=datadir)
			logging.debug('set config "%s"', cname)


class SubmissionAnalyzer(object):
	def __init__(self, dbpath=None):
		self.bindir = runallstates.getDefaultBindir()
		self.datadir = runallstates.getDefaultDatadir()
		# map from STU/config-name to configuration objects
		self.config = {}
		self.dbpath = dbpath
		# sqlite connection
		self.db = None
		self.stderr = sys.stderr
		self.stdout = sys.stdout
		if self.dbpath:
			self.opendb(self.dbpath)
	
	def opendb(self, path):
		self.db = sqlite3.connect(path)
		c = self.db.cursor()
		# TODO?: make this less sqlite3 specific sql
		c.execute('CREATE TABLE IF NOT EXISTS submissions (id INTEGER PRIMARY KEY AUTOINCREMENT, vars TEXT, unixtime INTEGER, kmpp REAL, spread INTEGER, path TEXT, config TEXT)')
		c.execute('CREATE INDEX IF NOT EXISTS submissions_path ON submissions (path)')
		c.execute('CREATE INDEX IF NOT EXISTS submissions_config ON submissions (config)')
		c.execute('CREATE TABLE IF NOT EXISTS vars (name TEXT PRIMARY KEY, value TEXT)')
		c.close()
		self.db.commit()
	
	def lookupByPath(self, path):
		"""Return db value for path."""
		c = self.db.cursor()
		c.execute('SELECT * FROM submissions WHERE path == ?', path)
		out = c.fetchone()
		c.close()
		return out
	
	def measureSolution(self, solf, configname):
		"""For file-like object of solution and config name, return (kmpp, spread)."""
		#./analyze -B data/MA/ma.pb -d 10 --loadSolution - < rundir/MA_Congress/link1/bestKmpp.dsz
		config = self.config[configname]
		datapb = elementAfter(config.args, '-P')
		districtNum = elementAfter(config.args, '-d')
		cmd = [os.path.join(self.bindir, 'analyze'),
			'-P', datapb,
			'-d', districtNum,
			'--loadSolution', '-']
		p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
		p.stdin.write(solf.read())
		p.stdin.close()
		retcode = p.wait()
		if retcode != 0:
			self.stderr.write('error %d running "%s"\n' % (retcode, ' '.join(cmd)))
			return None
		raw = p.stdout.read()
		m = kmppRe.search(raw)
		if not m:
			self.stderr.write('failed to find kmpp in analyze output:\n%s\n' % raw)
			return None
		kmpp = float(m.group(1))
		m = maxMinRe.search(raw)
		if not m:
			self.stderr.write('failed to find max/min in analyze output:\n%s\n' % raw)
			return None
		max = int(m.group(1))
		min = int(m.group(2))
		spread = max - min
		return (kmpp, spread)
	
	def setFromPath(self, fpath):
		tf_mtime = int(os.path.getmtime(fpath))
		tf = tarfile.open(fpath, 'r:gz')
		varfile = tf.extractfile('vars')
		varraw = varfile.read()
		vars = cgi.parse_qs(varraw)
		if 'config' in vars:
			config = vars['config']
		if 'localpath' in vars:
			remotepath = vars['path']
			for stu in self.config.iterkeys():
				if stu in remotepath:
					config = stu
					break
		if not config:
			self.stderr.write('no config for "%s"' % fpath)
			return
		solfile = tf.extractfile('solution')
		kmppSpread = self.measureSolution(solfile, config)
		if kmppSpread is None:
			self.stderr.write('failed to analyze solution in "%s"' % fpath)
			return
		c = self.db.cusor()
		c.execute('INSERT INTO submissions (vars, unixtime, kmpp, spread, path, config) VALUES ( ?, ?, ?, ?, ?, ? )',
			(varraw, tf_mtime, kmppSpread[0], kmppSpread[1], fpath, config))
	
	def updatedb(self, path):
		"""Update db for solutions under path."""
		if not self.db:
			raise Exception('no db opened')
		for fpath in scandir(path):
			x = self.lookupByPath(fpath)
			if x:
				# already have it
				continue
			try:
				self.setFromPath(fpath)
			except Exception, e:
				self.stderr.write('failed to process "%s": %r' % (fpath, e))
	
	def run(self):
		pass


def main():
	import optparse
	x = SubmissionAnalyzer(dbpath='.status.sqlite3')
	x.run()


if __name__ == '__main__':
	main()
