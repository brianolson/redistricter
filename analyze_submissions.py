#!/usr/bin/python
# TODO: move results into directories per config?
# solutions/YYYYMMDD/HHMMSS_IP_XXX.tar.gz
# ->
# processed/XX_Config/YYYYMMDD/HHMMSS_IP_XXX.tar.gz
# TODO: emit config information for cliens for threshold of kmpp to send to server.
# TODO: emit web directory of best-so-far results.
# TODO: extract kmpp_spread.svg from these.

import cgi
import logging
import os
import random
import re
import shutil
import sys
import sqlite3
import string
import subprocess
import tarfile
import time
import traceback
import zipfile

from kmppspreadplot import svgplotter
from newerthan import newerthan
import resultspage
import runallstates
import states

srcdir_ = os.path.dirname(os.path.abspath(__file__))
legpath_ = os.path.join(srcdir_, 'legislatures.csv')


_ga_cache = None
def _google_analytics():
	global _ga_cache
	if _ga_cache is None:
		gapath = os.path.join(srcdir_, 'google_analytics')
		if not os.path.exists(gapath):
			_ga_cache = ''
			return _ga_cache
		_ga_cache = open(gapath, 'rb').read()
	return _ga_cache

def localtime():
	return time.strftime('%Y-%m-%d %H:%M:%S %Z', time.localtime())


def scandir(path):
	"""Yield (fpath, innerpath) of .tar.gz submissions."""
	for root, dirnames, filenames in os.walk(path):
		for fname in filenames:
			if fname.endswith('.tar.gz'):
				fpath = os.path.join(root, fname)
				assert fpath.startswith(path)
				innerpath = fpath[len(path):]
				#logging.debug('found %s', innerpath)
				yield (fpath, innerpath)


def elementAfter(haystack, needle):
	"""For some sequence haystack [a, needle, b], return b."""
	isNext = False
	for x in haystack:
		if isNext:
			return x
		if x == needle:
			isNext = True
	return None


def extractSome(fpath, names):
	"""From .tar.gz at fpath, get members in list names.
	Return {name; value}."""
	out = {}
	tf = tarfile.open(fpath, 'r:gz')
	for info in tf:
		if info.name in names:
			out[info.name] = tf.extractfile(info).read()
	return out


def atomicLink(src, dest):
	assert dest[-1] != os.sep
	#assert os.path.exists(src)
	if not os.path.exists(src):
		logging.error('atomicLink: %s does not exist', src)
		return
	if os.path.exists(dest) and os.path.samefile(src, dest):
		return
	tdest = dest + str(random.randint(100000,999999))
	os.link(src, tdest)
	os.rename(tdest, dest)
	if os.path.exists(tdest):
		logging.warn('temp link %s still exists, unlinking', tdest)
		os.unlink(tdest)


def configToName(cname):
	(px, body) = cname.split('_', 1)
	return states.nameForPostalCode(px) + ' ' + body


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
			#logging.debug('"%s" not in state arg list', stu)
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
	def __init__(self, options, dbpath=None):
		self.options = options
		# map from STU/config-name to configuration objects
		self.config = {}
		self.dbpath = dbpath
		# sqlite connection
		self.db = None
		self.stderr = sys.stderr
		self.stdout = sys.stdout
		if self.dbpath:
			self.opendb(self.dbpath)
		self.pageTemplate = None
	
	def getPageTemplate(self, rootdir=None):
		if self.pageTemplate is None:
			if rootdir is None:
				rootdir = srcdir_
			f = open(os.path.join(rootdir, 'new_st_index_pyt.html'), 'r')
			self.pageTemplate = string.Template(f.read())
			f.close()
		return self.pageTemplate
	
	def loadDatadir(self, path=None):
		if path is None:
			path = self.options.datadir
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
	
	def measureSolution(self, solraw, configname):
		"""For file-like object of solution and config name, return (kmpp, spread)."""
		#./analyze -B data/MA/ma.pb -d 10 --loadSolution - < rundir/MA_Congress/link1/bestKmpp.dsz
		config = self.config.get(configname)
		if not config:
			logging.warn('config %s not loaded. cannot analyze', configname)
			return None
		datapb = config.args['-P']
		districtNum = config.args['-d']
		cmd = [os.path.join(self.options.bindir, 'analyze'),
			'-P', datapb,
			'-d', districtNum,
			'--loadSolution', '-']
		logging.debug('run %r', cmd)
		p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
		p.stdin.write(solraw)
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
		tfparts = extractSome(fpath, ('vars', 'solution'))
		if not 'vars' in tfparts:
			logging.warn('no "vars" in "%s"', fpath)
			return False
		vars = cgi.parse_qs(tfparts['vars'])
		config = None
		if 'config' in vars:
			config = vars['config'][0]
		if (not config) and ('localpath' in vars):
			remotepath = vars['path'][0]
			logging.debug('remotepath=%s', remotepath)
			for stu in self.config.iterkeys():
				if stu in remotepath:
					config = stu
					break
		if not config:
			logging.warn('no config for "%s"', fpath)
			return False
		if 'solution' in tfparts:
			kmppSpread = self.measureSolution(tfparts['solution'], config)
			if kmppSpread is None:
				logging.warn('failed to analyze solution in "%s"', fpath)
				return False
		else:
			kmppSpread = (None, None)
		logging.debug(
			'%s %d kmpp=%s spread=%s from %s',
			config, tf_mtime, kmppSpread[0], kmppSpread[1], innerpath)
		c = self.db.cursor()
		c.execute('INSERT INTO submissions (vars, unixtime, kmpp, spread, path, config) VALUES ( ?, ?, ?, ?, ?, ? )',
			(tfparts['vars'], tf_mtime, kmppSpread[0], kmppSpread[1], innerpath, config))
		return True
	
	def updatedb(self, path):
		"""Update db for solutions under path."""
		if not self.db:
			raise Exception('no db opened')
		setAny = False
		for (fpath, innerpath) in scandir(path):
			x = self.lookupByPath(innerpath)
			if x:
				#logging.debug('already have %s', innerpath)
				continue
			try:
				ok = self.setFromPath(fpath, innerpath)
				setAny = setAny or ok
				logging.info('added %s', innerpath)
			except Exception, e:
				traceback.print_exc()
				logging.warn('failed to process "%s": %r', fpath, e)
				if not self.options.keepgoing:
					break
		if setAny:
			self.db.commit()
	
	def getConfigCounts(self):
		"""For all configurations, return dict mapping config name to a dict {'count': number of solutions reported} for it.
		It's probably handy to extend that dict with getBestSolutionInfo below.
		"""
		c = self.db.cursor()
		rows = c.execute('SELECT config, count(*) FROM submissions GROUP BY config')
		configs = {}
		for config, count in rows:
			configs[config] = {'count': count, 'config': config}
		return configs
	
	def getBestSolutionInfo(self, cname, data):
		"""Set fields in dict 'data' for the best solution to configuration 'cname'."""
		c = self.db.cursor()
		rows = c.execute('SELECT kmpp, spread, id, path FROM submissions WHERE config = ? ORDER BY kmpp DESC LIMIT 1', (cname,))
		rowlist = list(rows)
		assert len(rowlist) == 1
		row = rowlist[0]
		data['kmpp'] = row[0]
		data['spread'] = row[1]
		data['id'] = row[2]
		data['path'] = row[3]
	
	def getBestConfigs(self):
		configs = self.getConfigCounts()
		for cname, data in configs.iteritems():
			self.getBestSolutionInfo(cname, data)
		return configs
	
	def writeConfigOverride(self, outpath):
		out = open(outpath, 'w')
		bestconfigs = self.getBestConfigs()
		counts = []
		for cname, data in bestconfigs.iteritems():
			counts.append(data['count'])
		totalruncount = sum(counts)
		mincount = min(counts)
		maxcount = max(counts)
		def rweight(count):
			return 5.0 - (4.0 * (count - mincount) / (maxcount - mincount))
		cnames = self.config.keys()
		cnames.sort()
		for cname in cnames:
			sendAnything = False
			if cname not in bestconfigs:
				sendAnything = True
			elif bestconfigs[cname]['kmpp'] is None:
				sendAnything = True
			if sendAnything:
				out.write('%s:sendAnything\n' % (cname,))
				out.write('%s:weight:5.0\n' % (cname,))
			else:
				data = bestconfigs[cname]
				out.write('%s:sendAnything: False\n' % (cname,))
				out.write('%s:weight:%f\n' % (cname, rweight(data['count'])))
			# TODO: tweak weight/kmppSendTheshold/spreadSendTheshold automatically
			#out.write('%s:disabled\n')
		mpath = outpath + '_manual'
		if os.path.exists(mpath):
			mf = open(mpath, 'r')
			for line in mf:
				out.write(line)
		out.close()
	
	def newestWinner(self, configs):
		newestconfig = None
		for cname, data in configs.iteritems():
			if (newestconfig is None) or (data['id'] > newestconfig['id']):
				newestconfig = data
		return newestconfig
	
	def writeHtml(self, outpath, configs=None):
		if configs is None:
			configs = self.getBestConfigs()
		newestconfig = self.newestWinner(configs)
		clist = configs.keys()
		clist.sort()
		out = open(outpath, 'w')
		out.write("""<!doctype html>
<html><head><title>solution report</title><link rel="stylesheet" href="report.css" /></head><body><h1>solution report</h1><p class="gentime">Generated %s</p>
""" % (localtime(),))
		out.write("""<div style="float:left"><div></div>""" + self.statenav(None, configs) + """</div>\n""")
		out.write("""<p>Newest winning result: <a href="%s/">%s</a><br /><img src="%s/map500.png"></p>\n""" % (newestconfig['config'], newestconfig['config'], newestconfig['config']))
		for cname in clist:
			data = configs[cname]
			if not data['kmpp']:
				out.write("""<div><a href="%s/kmpp.svg">%s</a></div>\n""" % (cname, cname))

		out.write('<table><tr><th>config name</th><th>num<br>solutions<br>reported</th><th>best kmpp</th><th>spread</th><th>id</th><th>path</th></tr>\n')
		for cname in clist:
			data = configs[cname]
			out.write('<tr><td><a href="%s/">%s</a></td><td>%d</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>\n' % (
				cname, cname, data['count'], data['kmpp'], data['spread'], data['id'], data['path']))
		out.write('</table>\n')
		out.write('</html></body>\n')
		out.close()
	
	def doDrend(self, cname, data, solutionDszRaw, pngpath):
		args = dict(self.config[cname].drendargs)
		args.update({'--loadSolution': '-', '--pngout': pngpath})
		cmd = [os.path.join(self.options.bindir, 'drend')] + runallstates.dictToArgList(args)
		logging.debug('run %r', cmd)
		p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
		p.stdin.write(solutionDszRaw)
		p.stdin.close()
		retcode = p.wait()
		if retcode != 0:
			self.stderr.write('error %d running "%s"\n' % (retcode, ' '.join(cmd)))
			return None
	
	def statenav(self, current, configs=None):
		statevars = {}
		if configs:
			citer = configs.iterkeys()
		else:
			citer = self.config.iterkeys()
		for cname in citer:
			(st, variation) = cname.split('_', 1)
			if st not in statevars:
				statevars[st] = [variation]
			else:
				statevars[st].append(variation)
		outl = []
		for name, stu, house in states.states:
			if stu not in statevars:
				#logging.warn('%s not in current results', stu)
				continue
			variations = statevars[stu]
			if 'Congress' in variations:
				variations.remove('Congress')
				variations.sort()
				variations.insert(0, 'Congress')
			vlist = []
			isCurrent = False
			for v in variations:
				stu_v = stu + '_' + v
				if stu_v == current:
					isCurrent = True
					vlist.append('<td><b>%s</b></td>' % (v,))
				else:
					vlist.append('<td><a href="%s">%s</a></td>' % (self.options.rooturl + '/' + stu_v + '/', v))
			if isCurrent:
				dclazz = 'slgC'
			else:
				dclazz = 'slg'
			outl.append('<tr class="%s"><td>%s</td>%s</tr>' % (dclazz, name, ' '.join(vlist)))
		return '<table class="snl">' + ''.join(outl) + '</table>'
	
	def measureRace(self, cname, solution, htmlout):
		config = self.config[cname]
		#zipname = 'VA/zips/va2010.pl.zip'
		stl = cname[0:2].lower()
		zipname = os.path.join(config.datadir, 'zips', stl + '2010.pl.zip')
		if not os.path.exists(zipname):
			logging.error('could not measure race without %s', zipname)
			return
		zf = zipfile.ZipFile(zipname, 'r')
		part1name = stl + '000012010.pl'
		#part1f = zf.open(part1name, 'r')
		#p1csv = csv.reader(part1f)
		numd = config.args['-d']
		pbfile = config.args['-P']

		analyzebin = os.path.join(self.options.bindir, 'analyze')
		cmd = [analyzebin, '--compare', ':5,7,8,9,10,11,12,13',
			'--labels', 'total,white,black,native,asian,pacific,other,mixed',
			'--dsort', '1', '--notext',
			'--html', htmlout,
			'-P', pbfile, '-d', numd, '--loadSolution', solution]

		p = subprocess.Popen(cmd, shell=False, stdin=subprocess.PIPE)
		p.stdin.write(zf.read(part1name))
		p.stdin.close()
		p.wait()

	def processFailedSubmissions(self, configs, cname):
		c = self.db.cursor()
		rows = c.execute('SELECT id, path FROM submissions WHERE config = ? AND kmpp IS NULL', (cname,))
		dumpBinLog = os.path.join(self.options.bindir, 'dumpBinLog')
		points = []
		dumpBinLogRE = re.compile(r'.*kmpp=([.0-9]+).*minPop=([0-9]+).*maxPop=([0-9]+)')
		rand = random.Random()
		for (rid, tpath) in rows:
			tpath = self.cleanupSolutionPath(tpath)
			data = extractSome(tpath, ('binlog',))
			if 'binlog' not in data:
				continue
			cmd = [dumpBinLog]
			p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
			p.stdin.write(data['binlog'])
			p.stdin.close()
			raw = p.stdout.read()
			p.wait()
			allpoints = []
			for line in raw.splitlines():
				 m = dumpBinLogRE.match(line)
				 if m:
					 kmpp = float(m.group(1))
					 minPop = int(m.group(2))
					 maxPop = int(m.group(3))
					 spread = maxPop - minPop
					 allpoints.append((spread, kmpp))
			if len(allpoints) > 20:
				half = len(allpoints) / 2
				allpoints = allpoints[half:]
			elif len(allpoints) < 10:
				continue
			points.extend(rand.sample(allpoints, 10))
		outdir = self.options.outdir
		kmpppath = os.path.join(outdir, cname, 'kmpp.svg')
		logging.info('writing %s', kmpppath)
		out = svgplotter(kmpppath)
		for (spread, kmpp) in points:
			out.xy(spread, kmpp)
		out.close()
		return kmpppath

	def cleanupSolutionPath(self, tpath):
		if tpath[0] == os.sep:
			tpath = tpath[len(os.sep):]
		tpath = os.path.join(self.options.soldir, tpath)
		return tpath
	
	def buildReportDirForConfig(self, configs, cname, data):
		"""Write report/$config/{index.html,map.png,map500.png,solution.dsz}
		"""
		if self.options.configlist and (cname not in self.options.configlist):
			logging.debug('skipping %s not in configlist', cname)
			return
		outdir = self.options.outdir
		sdir = os.path.join(outdir, cname, str(data['id']))
		if not os.path.isdir(sdir):
			os.makedirs(sdir)
		logging.debug('%s -> %s', cname, sdir)
		ihpath = os.path.join(sdir, 'index.html')
		mappath = os.path.join(sdir, 'map.png')
		needsIndexHtml = self.options.redraw or self.options.rehtml or (not os.path.exists(ihpath))
		needsDrend = self.options.redraw or (not os.path.exists(mappath))
		if not (needsIndexHtml or needsDrend):
			logging.debug('nothing to do for %s', cname)
			return
		
		tpath = self.cleanupSolutionPath(data['path'])
		tfparts = extractSome(tpath, ('solution', 'statsum'))
		
		if 'solution' in tfparts:
			# write solution.dsz
			solpath = os.path.join(sdir, 'solution.dsz')
			if not os.path.exists(solpath):
				logging.debug('write %s', solpath)
				dszout = open(solpath, 'wb')
				dszout.write(tfparts['solution'])
				dszout.close()
			
			# TODO: run `measurerace` to get demographic analysis
			racehtml = os.path.join(sdir, 'race.html')
			if self.options.redraw or (not os.path.exists(racehtml)):
				self.measureRace(cname, solpath, racehtml)
			
			# Make images map.png and map500.png
			if needsDrend:
				self.doDrend(cname, data, tfparts['solution'], mappath)
			map500path = os.path.join(sdir, 'map500.png')
			if newerthan(mappath, map500path):
				subprocess.call(['convert', mappath, '-resize', '500x500', map500path])
		else:
			logging.error('no solution for %s', cname)
			self.processFailedSubmissions(configs, cname)
		
		# index.html
		(kmpp, spread, std) = resultspage.parse_statsum(tfparts['statsum'])
		if (kmpp is None) or (spread is None) or (std is None):
			logging.error('bad statsum for %s', cname)
			return
		st_template = self.getPageTemplate()
		# TODO: permalink
		permalink = self.options.rooturl + '/' + cname + '/' + str(data['id']) + '/'
		racedata = ''
		if os.path.exists(racehtml):
			racedata = '<h3>Population Race Breakdown Per District</h3>'
			racedata += open(racehtml, 'r').read()
		extrapath = os.path.join(outdir, cname, 'extra.html')
		extrahtml = ''
		if os.path.exists(extrapath):
			extrahtml = open(extrapath, 'r').read()
		statename = configToName(cname)
		out = open(ihpath, 'w')
		out.write(st_template.substitute(
			statename=statename,
			statenav=self.statenav(cname, configs),
			ba_large='map.png',
			ba_small='map500.png',
			# TODO: get avgpop for state
			avgpop='',
			current_kmpp='',
			current_spread='',
			current_std='',
			my_kmpp=str(kmpp),
			my_spread=str(int(float(spread))),
			my_std=str(std),
			extra=extrahtml,
			racedata=racedata,
			rooturl=self.options.rooturl,
			google_analytics=_google_analytics(),
		))
		out.close()
		for x in ('map.png', 'map500.png', 'index.html', 'solution.dsz'):
			atomicLink(os.path.join(sdir, x), os.path.join(outdir, cname, x))
	
	def buildBestSoFarDirs(self, configs=None):
		"""$outdir/$XX_yyy/$id/{index.html,ba_500.png,ba.png,map.png,map500.png}
		With hard links from $XX_yyy/* to $XX_yyy/$id/* for the current best."""
		outdir = self.options.outdir
		if not os.path.isdir(outdir):
			os.makedirs(outdir)
		if configs is None:
			configs = self.getBestConfigs()
		for cname, data in configs.iteritems():
			self.buildReportDirForConfig(configs, cname, data)
		# build top level index.html
		result_index_html_path = os.path.join(srcdir_, 'result_index_pyt.html')
		f = open(result_index_html_path, 'r')
		result_index_html_template = string.Template(f.read())
		f.close()
		newestconfig = self.newestWinner(configs)['config']
		newestname = configToName(newestconfig)
		index_html_path = os.path.join(outdir, 'index.html')
		index_html = open(index_html_path, 'w')
		index_html.write(result_index_html_template.substitute(
			statenav=self.statenav(None, configs),
			rooturl=self.options.rooturl,
			localtime=localtime(),
			nwinner=newestconfig,
			nwinnername=newestname,
			google_analytics=_google_analytics(),
		))
		index_html.close()
		logging.debug('wrote %s', index_html_path)
		reportcssSource = os.path.join(srcdir_, 'report.css')
		reportcssDest = os.path.join(outdir, 'report.css')
		if newerthan(reportcssSource, reportcssDest):
			logging.debug('%s -> %s', reportcssSource, reportcssDest)
			shutil.copy2(reportcssSource, reportcssDest)


def main():
	import optparse
	argp = optparse.OptionParser()
	default_bindir = runallstates.getDefaultBindir()
	argp.add_option('-d', '--data', '--datadir', dest='datadir', default=runallstates.getDefaultDatadir(default_bindir))
	argp.add_option('--bindir', '--bin', dest='bindir', default=default_bindir)
	argp.add_option('--keep-going', '-k', dest='keepgoing', default=False, action='store_true', help='like make, keep going after failures')
	argp.add_option('--soldir', '--solutions', dest='soldir', default='.', help='directory to scan for solutions')
	argp.add_option('--do-update', dest='doupdate', default=True)
	argp.add_option('--no-update', dest='doupdate', action='store_false')
	argp.add_option('--report', dest='report', default='report.html', help='filename to write html report to.')
	argp.add_option('--outdir', dest='outdir', default='report', help='directory to write html best-so-far displays to.')
	argp.add_option('--configoverride', dest='configoverride', default=None, help='where to write configoverride file')
	argp.add_option('--verbose', '-v', dest='verbose', action='store_true', default=False)
	argp.add_option('--rooturl', dest='rooturl', default='file://' + os.path.abspath('.'))
	argp.add_option('--redraw', dest='redraw', action='store_true', default=False)
	argp.add_option('--rehtml', dest='rehtml', action='store_true', default=False)
	argp.add_option('--config', dest='configlist', action='append', default=[])
	(options, args) = argp.parse_args()
	if options.verbose:
		logging.getLogger().setLevel(logging.DEBUG)
	x = SubmissionAnalyzer(options, dbpath='.status.sqlite3')
	logging.debug('loading datadir')
	x.loadDatadir(options.datadir)
	logging.debug('done loading datadir')
	if options.soldir and options.doupdate:
		x.updatedb(options.soldir)
	configs = None
	if options.report or options.outdir:
		configs = x.getBestConfigs()
	if options.configoverride:
		x.writeConfigOverride(options.configoverride)
	if options.report:
		x.writeHtml(options.report, configs)
	if options.outdir:
		x.buildBestSoFarDirs(configs)


if __name__ == '__main__':
	main()
