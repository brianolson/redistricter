#!/usr/bin/python


import cgi
import logging
import os
import re
import sys
import sqlite3
import subprocess
import tarfile
import traceback

import runallstates


def scandir(path):
	"""Return a list of paths of .tar.gz submissions."""
	out = []
	for root, dirnames, filenames in os.walk(path):
		for fname in filenames:
			if fname.endswith('.tar.gz'):
				fpath = os.path.join(root, fname)
				assert fpath.startswith(path)
				innerpath = fpath[len(path):]
				logging.debug('found %s', innerpath)
				out.append((fpath, innerpath))
	return out


def elementAfter(haystack, needle):
	"""For some sequence haystack [a, needle, b], return b."""
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
			if runallstates.ignoreFile(variant):
				logging.debug('ignore file %s/config/"%s"', xx, variant)
				continue
			cpath = os.path.join(datadir, xx, 'config', variant)
			if configPathFilter and (not configPathFilter(cpath)):
				logging.debug('filter out "%s"', cpath)
				continue
			cname = stu + '_' + variant
			configs[cname] = runallstates.configuration(
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
	
	def loadDatadir(self, path=None):
		if path is None:
			path = self.datadir
		loadDatadirConfigurations(self.config, path)
	
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
		c.execute('SELECT * FROM submissions WHERE path == ?', (path,))
		out = c.fetchone()
		c.close()
		return out
	
	def measureSolution(self, solf, configname):
		"""For file-like object of solution and config name, return (kmpp, spread)."""
		#./analyze -B data/MA/ma.pb -d 10 --loadSolution - < rundir/MA_Congress/link1/bestKmpp.dsz
		config = self.config.get(configname)
		if not config:
			logging.warn('config %s not loaded. cannot analyze %s', configname, getattr(solf, 'name'))
			return None
		datapb = elementAfter(config.args, '-P')
		districtNum = elementAfter(config.args, '-d')
		cmd = [os.path.join(self.bindir, 'analyze'),
			'-P', datapb,
			'-d', districtNum,
			'--loadSolution', '-']
		logging.debug('run %r', cmd)
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
	
	def setFromPath(self, fpath, innerpath):
		"""Return True if db was written."""
		tf_mtime = int(os.path.getmtime(fpath))
		tf = tarfile.open(fpath, 'r:gz')
		varfile = tf.extractfile('vars')
		varraw = varfile.read()
		vars = cgi.parse_qs(varraw)
		if 'config' in vars:
			config = vars['config'][0]
		if 'localpath' in vars:
			remotepath = vars['path'][0]
			logging.debug('remotepath=%s', remotepath)
			for stu in self.config.iterkeys():
				if stu in remotepath:
					config = stu
					break
		if not config:
			logging.warn('no config for "%s"', fpath)
			return False
		solfile = tf.extractfile('solution')
		kmppSpread = self.measureSolution(solfile, config)
		if kmppSpread is None:
			logging.warn('failed to analyze solution in "%s"', fpath)
			return False
		logging.debug(
			'%s %d kmpp=%f spread=%f from %s',
			config, tf_mtime, kmppSpread[0], kmppSpread[1], innerpath)
		c = self.db.cursor()
		c.execute('INSERT INTO submissions (vars, unixtime, kmpp, spread, path, config) VALUES ( ?, ?, ?, ?, ?, ? )',
			(varraw, tf_mtime, kmppSpread[0], kmppSpread[1], innerpath, config))
		return True
	
	def updatedb(self, path):
		"""Update db for solutions under path."""
		if not self.db:
			raise Exception('no db opened')
		setAny = False
		for (fpath, innerpath) in scandir(path):
			x = self.lookupByPath(innerpath)
			if x:
				logging.debug('already have %s', innerpath)
				continue
			try:
				ok = self.setFromPath(fpath, innerpath)
				setAny = setAny or ok
			except Exception, e:
				traceback.print_exc()
				logging.warn('failed to process "%s": %r', fpath, e)
		if setAny:
			self.db.commit()
	
	def writeHtml(self, outpath):
		out = open(outpath, 'w')
		out.close()


def main():
	import optparse
	argp = optparse.OptionParser()
	argp.add_option('-d', '--data', '--datadir', dest='datadir', default=runallstates.getDefaultDatadir())
	argp.add_option('--bindir', '--bin', dest='bindir', default=runallstates.getDefaultBindir())
	argp.add_option('--soldir', '--solutions', dest='soldir', default='.', help='directory to scan for solutions')
	argp.add_option('--report', dest='report', default='report.html', help='filename to write html report to.')
	argp.add_option('--verbose', '-v', dest='verbose', action='store_true', default=False)
	(options, args) = argp.parse_args()
	if options.verbose:
		logging.getLogger().setLevel(logging.DEBUG)
	x = SubmissionAnalyzer(dbpath='.status.sqlite3')
	x.datadir = options.datadir
	x.loadDatadir(options.datadir)
	x.bindir = options.bindir
	if options.soldir:
		x.updatedb(options.soldir)
	if options.report:
		x.writeHtml(options.report)


if __name__ == '__main__':
	main()
