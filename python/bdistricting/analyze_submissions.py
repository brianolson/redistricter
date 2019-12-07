#!/usr/bin/env python
##!/usr/bin/python

import cgi
import csv
import gzip
import json
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
import urllib.request, urllib.parse, urllib.error
import zipfile

#import djangotemplates
import jinjatemplates as djangotemplates
from kmppspreadplot import svgplotter
from newerthan import newerthan, any_newerthan
import resultspage
import runallstates
import states

srcdir_ = os.path.dirname(os.path.abspath(__file__))

_resources = ('report.css', 'tweet.ico', 'spreddit7.gif')

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
        if 'Attic' in dirnames:
            dirnames.remove('Attic')
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
    if not fpath:
        return {}
    out = {}
    try:
        tf = tarfile.open(fpath, 'r:gz')
        for info in tf:
            if info.name in names:
                out[info.name] = tf.extractfile(info).read()
    except:
        pass
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
    legstats = states.legislatureStatsForPostalCode(px)
    body = states.expandLegName(legstats, body)
    return states.nameForPostalCode(px) + ' ' + body


def urljoin(*args):
    parts = []
    prevSlash = False
    first = True
    for x in args:
        if not x:
            continue
        if first:
            # don't change leading slash quality
            first = False
        elif prevSlash:
            if x[0] == '/':
                x = x[1:]
        else:
            if x[0] != '/':
                parts.append('/')
        prevSlash = x[-1] == '/'
        parts.append(x)
    return ''.join(parts)


def stripDjangoishComments(text):
    """Return text with {# ... #} stripped out (multiline)."""
    commentFilter = re.compile(r'{#.*?#}', re.DOTALL|re.MULTILINE)
    return commentFilter.sub('', text)


def templateFromFile(f):
    raw = f.read()
    cooked = stripDjangoishComments(raw)
    return string.Template(cooked)


_analyze_re = re.compile(r'.*?([0-9.]+)\s+Km/person.*avg=([0-9.]+).*std=([0-9.]+).*max=([0-9.]+).*min=([0-9.]+).*median=([0-9.]+).*', re.MULTILINE|re.DOTALL)


def parseAnalyzeStats(rawblob):
    """Return (kmpp, spread, std)"""
    # e.g.:
    #generation 0: 38.093901615 Km/person
    #population avg=696345 std=0.395847391
    #max=696345 (dist# 1)  min=696344 (dist# 8)  median=696345 (dist# 14)
    m = _analyze_re.match(rawblob)
    if not m:
        return (None, None, None)
    logging.debug('groups=%r', m.groups())
    kmpp = float(m.group(1))
    std = float(m.group(3))
    maxp = float(m.group(4))
    minp = float(m.group(5))
    return (kmpp, maxp - minp, std)
    

# Example analyze output:
# generation 0: 21.679798418 Km/person
# population avg=634910 std=1707.11778
# max=638656 (dist# 10)  min=632557 (dist# 7)  median=634306 (dist# 6)

kmppRe = re.compile(r'([0-9.]+)\s+Km/person')
maxMinRe = re.compile(r'max=([0-9]+).*min=([0-9]+)')


def measure_race(stl, numd, pbfile, solution, htmlout, zipname, exportpath=None, bindir=None, printcmd=None):
    """Writes html to htmlout. Returns blob of text with stats. Pass blob to parseAnalyzeStats if desired."""
    if not os.path.exists(zipname):
        logging.error('could not measure race without %s', zipname)
        return
    part1name = stl + '000012010.pl'
    try:
        with zipfile.ZipFile(zipname, 'r') as zf:
            part1data = zf.read(part1name)
    except:
        logging.error('could not measure race, failed reading %r/%r', zipname, part1name, exc_info=True)
        return None
    if bindir is None:
        bindir = srcdir_

    analyzebin = os.path.join(bindir, 'analyze')
    cmd = [analyzebin, '--compare', ':5,7,8,9,10,11,12,13',
           '--labels', 'total,white,black,native,asian,pacific,other,mixed',
           '--dsort', '1', '--notext',
           '--html', htmlout,
           '-P', pbfile, '-d', numd, '--loadSolution', solution]
    if exportpath:
        cmd += ['--export', exportpath]

    if printcmd:
        printcmd(cmd)
    try:
        p = subprocess.Popen(cmd, shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        p.stdin.write(part1data)
        p.stdin.close()
        p.wait()
        analyze_text = p.stdout.read()
        return analyze_text
    except:
        logging.error('failed analyzing race in subprocess %r', cmd, exc_info=True)
    return None


def getStatesCsvSources(actualsDir):
    """For directory of 00_XX_*.txt return map {stu, source csv filename}"""
    stDistFiles = {}
    anyError = False

    distdirall = os.listdir(actualsDir)
    
    statefileRawRe = re.compile('.._(..)_.*\.txt', re.IGNORECASE)

    for fname in distdirall:
        m = statefileRawRe.match(fname)
        if m:
            stu = m.group(1).upper()
            if states.nameForPostalCode(stu) is not None:
                # winner
                old = stDistFiles.get(stu)
                if old is None:
                    stDistFiles[stu] = fname
                else:
                    logging.error('collision %s -> %s AND %s', stu, old, fname)
                    anyError = True
    
    return stDistFiles, anyError


def csvToSimpleCsv(csvpath, outpath):
    """Convert CSV with district 'number' that could be '00A' '01A' 'MISC' to simple numeric district numbers."""
    fin = open(csvpath, 'rb')
    reader = csv.reader(fin)
    row = next(reader)
    # expect header row either:
    # BLOCKID,CD113
    # BLOCKID,DISTRICT,NAME
    assert row[0] == 'BLOCKID'
    assert ((row[1] == 'CD113') or (row[1] == 'DISTRICT'))
    unmapped = []
    districts = set()
    for row in reader:
        unmapped.append( (row[0], row[1]) )
        districts.add(row[1])
    fin.close()
    districts.discard('ZZZ')
    dl = list(districts)
    dl.sort()
    dmap = {}
    for i, dname in enumerate(dl):
        dmap[dname] = i + 1    # 1 based csv file district numbering
    logging.debug('%s has districts %r', csvpath, sorted([(i,v) for v,i in dmap.items()]))
    fout = open(outpath, 'wb')
    writer = csv.writer(fout)
    for blockid, dname in unmapped:
        districtNumber = dmap.get(dname)
        if districtNumber is not None:
            writer.writerow( (blockid, districtNumber) )
    fout.close()


_drendpath = None


def drendpath():
    global _drendpath
    if _drendpath is None:
        _drendpath = os.path.join(srcdir_, 'drend')
        if not os.path.exists(_drendpath):
            logging.error('no drend binary at %r', drendpath)
            sys.exit(1)
    return _drendpath


def noSource(sourceName):
    logging.error('missing source %s', sourceName)


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
        
        # map from STU/config-name to runallstates.configuration objects
        self.config = {}
        
        self.dbpath = dbpath
        # sqlite connection
        self.db = None
        self.stderr = sys.stderr
        self.stdout = sys.stdout
        if self.dbpath:
            self.opendb(self.dbpath)
        self.pageTemplate = None
        self.dirTemplate = None
        self.socialTemplate = None
        # cache for often used self.statenav(None, configs)
        self._statenav_all = None
        self._actualsMaps = {}
        # mode for sharing template
        self.safeSocialShare = True

        self.summaryCsvFile = None
        self.summaryCsvWriter = None

    def actualsSource(self, actualSet, stu):
        """Lazy loading accessor to find source CSV files for actualsdir/{set}/??_{stu}_*.txt"""
        maps = self._actualsMaps.get(actualSet)
        if maps is None:
            maps, _ = getStatesCsvSources(os.path.join(self.options.actualdir, actualSet))
            self._actualsMaps[actualSet] = maps
        return maps[stu]
    
    def getSocial(self, pageabsurl, cgipageabsurl):
        context = dict(
            pageabsurl=pageabsurl,
            cgipageabsurl=cgipageabsurl,
            rooturl=self.options.rooturl,
            socialshare=self.safeSocialShare,
            )
        return djangotemplates.render('social_django.html', context)

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
            return (None,None)
        datapb = config.args['-P']
        districtNum = config.args['-d']
        cmd = [os.path.join(self.options.bindir, 'analyze'),
            '-P', datapb,
            '-d', districtNum,
               '-notext',
            '-r', '-']
        logging.debug('run %r', cmd)
        p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
        p.stdin.write(solraw)
        p.stdin.close()
        retcode = p.wait()
        if retcode != 0:
            self.stderr.write('error %d running "%s"\n' % (retcode, ' '.join(cmd)))
            return (None,None)
        raw = p.stdout.read()
        m = kmppRe.search(raw)
        if not m:
            self.stderr.write('failed to find kmpp %r in analyze output:\n%s\n%s\n' % (kmppRe.pattern, ' '.join(cmd), raw))
            return (None,None)
        kmpp = float(m.group(1))
        m = maxMinRe.search(raw)
        if not m:
            self.stderr.write('failed to find max/min %r in analyze output:\ns\n%s\n' % (makMinRe.pattern, ' '.join(cmd), raw))
            return (None,None)
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
            for stu in self.config.keys():
                if stu in remotepath:
                    config = stu
                    break
        if not config:
            logging.warn('no config for "%s"', fpath)
            return False
        if 'solution' in tfparts:
            kmppSpread = self.measureSolution(tfparts['solution'], config)
            if (not kmppSpread) or (kmppSpread[0] is None):
                logging.warn('failed to analyze solution for %s in "%s"', config, fpath)
                c = self.db.cursor()
                c.execute('INSERT INTO submissions (vars, unixtime, path, config) VALUES ( ?, ?, ?, ? )',
            (tfparts['vars'], tf_mtime, innerpath, config))
                # TODO: set attempt count in 'vars' and retry up to N times
                return True # db was written
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
                # TODO: start thread to run setFromPath asynchronously and run several of the measureSolution() subprocesses in parallel? This has not been an actual performance inconvenience, but only might become so when using a cluster to generate many-many solutions in a short amount of time, increasing load on the coordinator node that verifies the quality of the solutions. If we trust the compute nodes to honestly measure solutions we don't actually need that. This was designed as a hedge against @home-style potentially untrusted client enviornments, but now we'd probably just run a cluster of cheap aws-spot/google-interruptable VMs.
                ok = self.setFromPath(fpath, innerpath)
                setAny = setAny or ok
                logging.info('added %s', innerpath)
            except Exception as e:
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
        rows = c.execute('SELECT kmpp, spread, id, path FROM submissions WHERE config = ? AND kmpp IS NOT NULL ORDER BY kmpp ASC LIMIT 1', (cname,))
        rowlist = list(rows)
        if not rowlist:
            data['kmpp'] = None
            data['spread'] = None
            data['id'] = None
            data['path'] = None
            return
        assert len(rowlist) == 1, '{!r} {!r}'.format(rowlist, cname)
        row = rowlist[0]
        data['kmpp'] = row[0]
        data['spread'] = row[1]
        data['id'] = row[2]
        data['path'] = row[3]
    
    def getBestConfigs(self):
        configs = self.getConfigCounts()
        for cname, data in configs.items():
            self.getBestSolutionInfo(cname, data)
        return configs
    
    def writeConfigOverride(self, outpath):
        out = open(outpath, 'w')
        bestconfigs = self.getBestConfigs()
        counts = []
        for cname, data in bestconfigs.items():
            counts.append(data['count'])
        totalruncount = sum(counts)
        mincount = min(counts)
        maxcount = max(counts)
        maxweight = 10.0
        def rweight(count):
            return maxweight - ((maxweight - 1.0) * (count - mincount) / (maxcount - mincount))
        cnames = sorted(self.config.keys())
        for cname in cnames:
            sendAnything = False
            if cname not in bestconfigs:
                sendAnything = True
            elif bestconfigs[cname].get('kmpp') is None:
                sendAnything = True
            if sendAnything:
                out.write('%s:sendAnything\n' % (cname,))
                out.write('%s:weight:%f\n' % (cname, maxweight))
            else:
                data = bestconfigs[cname]
                out.write('%s:sendAnything: False\n' % (cname,))
                out.write('%s:weight:%f\n' % (cname, rweight(data['count'])))
            if (cname in bestconfigs) and (bestconfigs[cname]['count'] >= 10):
                c = self.db.cursor()
                rows = c.execute('SELECT kmpp FROM submissions WHERE config = ? AND kmpp > 0 ORDER BY kmpp ASC LIMIT 10', (cname,))
                rows = list(rows)
                if rows and (len(rows) == 10):
                    kmpplimit = float(rows[-1][0])
                    out.write('%s:kmppSendThreshold:%f\n' % (cname, kmpplimit))
                else:
                    logging.warn('%s count=%s but fetched %s', cname, bestconfigs[cname]['count'], len(rows))
            # TODO: tweak spreadSendThreshold automatically ?
        mpath = outpath + '_manual'
        if os.path.exists(mpath):
            mf = open(mpath, 'r')
            for line in mf:
                if (not line) or (line[0] == '#'):
                    continue
                out.write(line)
        out.close()
    
    def newestWinner(self, configs):
        newestconfig = None
        for cname, data in configs.items():
            if not data.get('kmpp'):
                continue
            if (newestconfig is None) or (data['id'] > newestconfig['id']):
                newestconfig = data
        return newestconfig
    
    def writeHtml(self, outpath, configs=None):
        if configs is None:
            configs = self.getBestConfigs()
        newestconfig = self.newestWinner(configs)

        # TODO: django templates?
        out = open(outpath, 'w')
        out.write("""<!doctype html>
<html><head><title>solution report</title><link rel="stylesheet" href="report.css" /></head><body><h1>solution report</h1><p class="gentime">Generated %s</p>
""" % (localtime(),))
        out.write("""<div style="float:left"><div></div>""" + self.statenav(None, configs) + """</div>\n""")
        out.write("""<p>Newest winning result: <a href="%s/">%s</a><br /><img src="%s/map500.png"></p>\n""" % (newestconfig['config'], newestconfig['config'], newestconfig['config']))

        firstNoSolution = True
        clist = sorted(configs.keys())
        for cname in clist:
            data = configs[cname]
            if not data.get('kmpp'):
                if firstNoSolution:
                    firstNoSolution = False
                    out.write("""<h2>no solution</h2>\n<table>""")
                out.write("""<tr><td><a href="%s/kmpp.svg">%s</a></td><td>%s</td></tr>\n""" % (cname, cname, data['count']))
        if not firstNoSolution:
            out.write("""</table>\n""")

        out.write('<table><tr><th>config name</th><th>num<br>solutions<br>reported</th><th>best kmpp</th><th>spread</th><th>id</th><th>path</th></tr>\n')
        for cname in clist:
            data = configs[cname]
            if not data.get('kmpp'):
                continue
            out.write('<tr><td><a href="%s/">%s</a></td><td>%d</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>\n' % (
                cname, cname, data['count'], data['kmpp'], data['spread'], data['id'], data['path']))
        out.write('</table>\n')
        out.write('</html></body>\n')
        out.close()
        self.copyResources()
    
    def doDrend(self, cname, data, pngpath, dszpath=None, solutionDszRaw=None, altmppb=None, highlight=None):
        args = dict(self.config[cname].drendargs)
        args.pop('--loadSolution', None)
        args['--pngout'] = pngpath
        if altmppb:
            args['--mppb'] = altmppb
        if dszpath:
            args['-r'] = dszpath
        elif solutionDszRaw:
            args['-r'] = '-'
        else:
            self.stderr.write('error: need dsz or raw dsz bytes for doDrend\n')
            return None
        cmd = [os.path.join(self.options.bindir, 'drend')] + runallstates.dictToArgList(args)
        if highlight:
            cmd += ['--hubidz', highlight]
        logging.debug('run %r', cmd)
        if solutionDszRaw:
            p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
            p.stdin.write(solutionDszRaw)
            p.stdin.close()
        else:
            p = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, shell=False)
        retcode = p.wait()
        if retcode != 0:
            self.stderr.write('config args {!r}\n'.format(self.config[cname].drendargs))
            self.stderr.write('error %d running "%s"\n' % (retcode, ' '.join(cmd)))
            return None
    
    def statenav(self, current, configs):
        if (current is None) and self._statenav_all:
            return self._statenav_all
        statevars = {}
        for cname, data in configs.items():
            if not data.get('kmpp'):
                continue
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
            if 'Congress' not in variations:
                vlist.append('<td class="nc">-</td>')
            for v in variations:
                stu_v = stu + '_' + v
                if stu_v == current:
                    isCurrent = True
                    vlist.append('<td><b>%s</b></td>' % (v,))
                else:
                    vlist.append('<td><a href="%s">%s</a></td>' % (urljoin(self.options.rooturl, stu_v) + '/', v))
            if isCurrent:
                dclazz = 'slgC'
            else:
                dclazz = 'slg'
            outl.append('<tr class="%s"><td><a href="%s/">%s</a></td>%s</tr>' % (dclazz, urljoin(self.options.rooturl, stu), name, ' '.join(vlist)))
        text = '<table class="snl">' + ''.join(outl) + '</table>'
        if current is None:
            self._statenav_all = text
        return text
    
    def statedir(self, stu, configs):
        stu = stu.upper()
        name = states.nameForPostalCode(stu)
        if not name:
            logging.error('no name for postal code %s', stu)
            return
        variations = []
        for cname, data in configs.items():
            if not data.get('kmpp'):
                continue
            (st, variation) = cname.split('_', 1)
            if st != stu:
                continue
            variations.append(variation)
        if not variations:
            logging.warn('no active variations for %s', stu)
        if 'Congress' in variations:
            variations.remove('Congress')
            variations.sort()
            variations.insert(0, 'Congress')
        legstats = states.legislatureStatsForPostalCode(stu)
        bodyrows = []
        firstvar = None
        for variation in variations:
            if not firstvar:
                firstvar = stu + '_' + variation
            bodyname = variation
            numd = 'X'
            for ls in legstats:
                if ls.shortname == variation:
                    bodyname = ls.name
                    numd = ls.count
            #bodyname = states.expandLegName(legstats, variation)
            configurl = '%s%s_%s/' % (self.options.rooturl, stu, variation)
            tr = """<tr><td><div><a href="%s">%s %s</a></div><div><a href="%s"><img src="%smap500.png" height="150"></a> %s districts</div></td></tr>""" % (configurl, name, bodyname, configurl, configurl, numd)
            bodyrows.append(tr)
        outdir = self.options.outdir
        sdir = os.path.join(outdir, stu)
        ihtmlpath = os.path.join(sdir, 'index.html')
        #st_template = self.getDirTemplate()
        pageabsurl = urljoin(self.options.siteurl, self.options.rooturl, stu) + '/'
        cgipageabsurl = urllib.parse.quote_plus(pageabsurl)
        cgiimageurl = urllib.parse.quote_plus(urljoin(self.options.siteurl, self.options.rooturl, firstvar, 'map500.png'))
        
        if not os.path.isdir(sdir):
            os.makedirs(sdir)
        extrahtml = ''
        extrahtmlpath = os.path.join(sdir, 'extra.html')
        if os.path.isfile(extrahtmlpath):
            extrahtml = open(extrahtmlpath, 'r').read()
        
        out = open(ihtmlpath, 'w')
        out.write(
            djangotemplates.render(
                'state_dir.html',
                dict(
            statename=name,
            stu=stu,
            statenav=self.statenav(None, configs),
            bodyrows='\n'.join(bodyrows),
            extra=extrahtml,
            rooturl=self.options.rooturl,
            cgipageabsurl=cgipageabsurl,
            cgiimageurl=cgiimageurl,
            google_analytics=_google_analytics(),
            social=self.getSocial(pageabsurl, cgipageabsurl),
            socialshare=self.safeSocialShare,
        )))
        out.close()
    
    def measureRace(self, cname, solution, htmlout, exportpath):
        config = self.config[cname]
        stl = cname[0:2].lower()
        zipname = os.path.join(config.datadir, 'zips', stl + '2010.pl.zip')
        if not os.path.exists(zipname):
            logging.error('could not measure race without %s', zipname)
            return
        numd = config.args['-d']
        pbfile = config.args['-P']

        measure_race(stl, numd, pbfile, solution, htmlout, zipname, exportpath, self.options.bindir)
    
    def processFailedSubmissions(self, configs, cname):
        c = self.db.cursor()
        rows = c.execute('SELECT id, path FROM submissions WHERE config = ? AND kmpp IS NULL', (cname,))
        dumpBinLog = os.path.join(self.options.bindir, 'dumpBinLog')
        points = []
        dumpBinLogRE = re.compile(r'.*kmpp=([.0-9]+).*minPop=([0-9]+).*maxPop=([0-9]+)')
        rand = random.Random()
        bestSpreadPoints = []
        for (rid, tpath) in rows:
            tpath = self.cleanupSolutionPath(tpath)
            data = extractSome(tpath, ('binlog',))
            if 'binlog' not in data:
                continue
            logging.info('processing %s binlog from %s', cname, tpath)
            cmd = [dumpBinLog]
            p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
            #p.stdin.write(data['binlog'])
            #p.stdin.close()
            #raw = p.stdout.read()
            (raw, _unused) = p.communicate(data['binlog'])
            allpoints = []
            bestSpreadPoint = None
            for line in raw.splitlines():
                 m = dumpBinLogRE.match(line)
                 if m:
                     kmpp = float(m.group(1))
                     minPop = int(m.group(2))
                     maxPop = int(m.group(3))
                     spread = maxPop - minPop
                     allpoints.append((spread, kmpp))
                     if (bestSpreadPoint is None) or (spread < bestSpreadPoint[0]):
                                                 bestSpreadPoint = (spread, kmpp)
            if bestSpreadPoint:
                bestSpreadPoints.append(bestSpreadPoint)
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
        if bestSpreadPoints:
            out.comment(repr(bestSpreadPoints))
        out.close()
        return kmpppath
    
    def cleanupSolutionPath(self, tpath):
        if not tpath:
            return None
        if tpath[0] == os.sep:
            tpath = tpath[len(os.sep):]
        tpath = os.path.join(self.options.soldir, tpath)
        return tpath
    
    def buildReportDirForConfig(self, configs, cname, data, stu):
        """Write report/$config/{index.html,map.png,map500.png,solution.dsz}
        """
        if self.options.configlist and (cname not in self.options.configlist):
            logging.debug('skipping %s not in configlist %r', cname, self.options.configlist)
            return
        stl = stu.lower()
        outdir = self.options.outdir
        sdir = os.path.join(outdir, cname, str(data['id']))
        if not os.path.isdir(sdir):
            os.makedirs(sdir)
        logging.debug('%s -> %s', cname, sdir)
        ihpath = os.path.join(sdir, 'index.html')
        mappath = os.path.join(sdir, 'map.png')
        mapLgPath = os.path.join(sdir, 'map_lg.png')
        config = self.config[cname]
        mppb_lg_path = os.path.join(config.datadir, stu + '_lg.mppb')
        if not os.path.exists(mppb_lg_path):
            logging.debug('missing %s', mppb_lg_path)
            mppb_lg_path = None
        highlight_path = os.path.join(config.datadir, 'highlight.ubidz')
        if not os.path.exists(highlight_path):
            logging.debug('missing %s', highlight_path)
            highlight_path = None
        needsLargeMap = (mppb_lg_path is not None) and (self.options.redraw or not os.path.exists(mapLgPath))
        needsIndexHtml = self.options.redraw or self.options.rehtml or (not os.path.exists(ihpath))
        needsDrend = self.options.redraw or (not os.path.exists(mappath))
        if not (needsIndexHtml or needsDrend or needsLargeMap):
            logging.debug('nothing to do for %s', cname)
            return
        
        tpath = self.cleanupSolutionPath(data['path'])
        tfparts = extractSome(tpath, ('solution', 'statsum'))
        actualMapPath = None
        actualMap500Path = None
        actualHtmlPath = None
        current_kmpp = None
        current_spread = None
        current_std = None
        
        if 'solution' in tfparts:
            # write solution.dsz
            solpath = os.path.join(sdir, 'solution.dsz')
            if not os.path.exists(solpath):
                logging.debug('write %s', solpath)
                dszout = open(solpath, 'wb')
                dszout.write(tfparts['solution'])
                dszout.close()
            
            racehtml = os.path.join(sdir, 'race.html')
            solutioncsvgz = os.path.join(sdir, 'solution.csv.gz')
            if self.options.redraw or newerthan(solpath, racehtml) or newerthan(solpath, solutioncsvgz):
                # TODO: there could be smarter logic here to run faster if only one piece is needed.
                self.measureRace(cname, solpath, racehtml, solutioncsvgz)
            
            solutionzip = os.path.join(sdir, 'solution.zip')
            if newerthan(solutioncsvgz, solutionzip):
                try:
                    zin = gzip.open(solutioncsvgz, 'rb')
                    solutioncsv = zin.read()
                    zin.close()
                    zcsvname = str(cname + '.csv')
                    logging.debug('got %d bytes from %r, to zipfile entry %r', len(solutioncsv), solutioncsvgz, zcsvname)
                    oz = zipfile.ZipFile(solutionzip, 'w', zipfile.ZIP_DEFLATED)
                    oz.writestr(zcsvname, solutioncsv)
                    oz.close()
                except Exception as e:
                    logging.error('failed %r -> %r: %s', solutioncsvgz, solutionzip, traceback.format_exc())
                    if os.path.exists(solutionzip):
                        try:
                            os.unlink(solutionzip)
                        except:
                            pass
            
            # Make images map.png and map500.png
            if needsDrend:
                self.doDrend(cname, data, mappath, dszpath=solpath, highlight=highlight_path)
            map500path = os.path.join(sdir, 'map500.png')
            if newerthan(mappath, map500path):
                subprocess.call(['convert', mappath, '-resize', '500x500', map500path])

            if needsLargeMap:
                self.doDrend(cname, data, mapLgPath, dszpath=solpath, altmppb=mppb_lg_path, highlight=highlight_path)

            # use actual maps if available
            if self.options.actualdir:
                # ensure setup
                actualSet = states.stateConfigToActual(stu, cname.split('_',1)[1])
                drendargs = config.drendargs
                zipname = os.path.join(config.datadir, 'zips', stl + '2010.pl.zip')
                (current_kmpp, current_spread, current_std) = self.processActualsSource(os.path.join(self.options.actualdir, actualSet), cname, stu, self.actualsSource(actualSet, stu), drendargs['-P'], drendargs['--mppb'], zipname, mppb_lg_path, highlight=highlight_path)

                actualMapPath = os.path.join(self.options.actualdir, actualSet, stu + '.png')
                actualMapLgPath = os.path.join(self.options.actualdir, actualSet, stu + '_lg.png')
                actualMap500Path = os.path.join(self.options.actualdir, actualSet, stu + '500.png')
                actualHtmlPath = os.path.join(self.options.actualdir, actualSet, stl + '.html')
        else:
            logging.error('no solution for %s', cname)
            self.processFailedSubmissions(configs, cname)
        
        # index.html
        (kmpp, spread, std) = resultspage.parse_statsum(tfparts.get('statsum'))
        if (kmpp is None) or (spread is None) or (std is None):
            logging.error('bad statsum for %s', cname)
            return
        kmpp = float(kmpp)
        spread = int(float(spread))
        std = float(std)
        # TODO: permalink
        permalink = os.path.join(self.options.rooturl, cname, str(data['id'])) + '/'
        racedata = ''
        if os.path.exists(racehtml):
            #racedata = '<h3>Population Race Breakdown Per District</h3>' +
            racedata = open(racehtml, 'r').read()
        extrapath = os.path.join(outdir, cname, 'extra.html')
        extrahtml = ''
        if os.path.exists(extrapath):
            extrahtml = open(extrapath, 'r').read()
        statename = configToName(cname)
        pageabsurl = urljoin(self.options.siteurl, self.options.rooturl, cname) + '/'
        cgipageabsurl = urllib.parse.quote_plus(pageabsurl)
        cgiimageurl = urllib.parse.quote_plus(urljoin(self.options.siteurl, self.options.rooturl, cname, 'map500.png'))
        actualHtmlData = None
        if actualHtmlPath:
            ahin = open(actualHtmlPath, 'rb')
            actualHtmlData = ahin.read()
            ahin.close
        self.summaryCsv(stu, statename, kmpp, spread, std, current_kmpp, current_spread, current_std)
        
        context = dict(
            statename=statename,
            stu=stu,
            statenav=self.statenav(cname, configs),
            ba_large='map.png',
            ba_small='map500.png',
            current_large=actualMapPath,
            current_small=actualMap500Path,
            current_demographics=actualHtmlData,
            # TODO: get avgpop for state
            avgpop='',
            current_kmpp=current_kmpp,
            current_spread=current_spread,
            current_std=current_std,
            my_kmpp=kmpp,
            my_spread=spread,
            my_std=std,
            extra=extrahtml,
            racedata=racedata,
            rooturl=self.options.rooturl,
            dirabsurl=pageabsurl,
            pageabsurl=pageabsurl,
            cgipageabsurl=cgipageabsurl,
            cgiimageurl=cgiimageurl,
            google_analytics=_google_analytics(),
            socialshare=self.safeSocialShare,
            social=self.getSocial(pageabsurl, cgipageabsurl),
        )
        if actualMapPath and actualMap500Path:
            context['current_large'] = stu + '.png'
            context['current_small'] = stu + '500.png'
            atomicLink(actualMapPath, os.path.join(sdir, stu + '.png'))
            atomicLink(actualMapLgPath, os.path.join(sdir, stu + '_lg.png'))
            atomicLink(actualMap500Path, os.path.join(sdir, stu + '500.png'))
            atomicLink(actualMapPath, os.path.join(outdir, cname, stu + '.png'))
            atomicLink(actualMapLgPath, os.path.join(outdir, cname, stu + '_lg.png'))
            atomicLink(actualMap500Path, os.path.join(outdir, cname, stu + '500.png'))
        out = open(ihpath, 'w')
        out.write(djangotemplates.render('st_index_django.html', context))
        out.close()
        for x in ('map.png', 'map500.png', 'map_lg.png', 'index.html', 'solution.dsz', 'solution.csv.gz', 'solution.zip'):
            atomicLink(os.path.join(sdir, x), os.path.join(outdir, cname, x))

    def processActualsSource(self, actualsDir, cname, stu, sourceCsvFname, pb, mppb, zipname, mppb_lg_path, highlight=None):
        """process 00_XX_SLDU.txt into intermediate XX.csv, output {XX.png,xx.html,xx_stats.txt}
        return (kmpp, spread, std)
        """
        stl = stu.lower()

        # actuals block source and intermediate csv
        csvpath = os.path.join(actualsDir, sourceCsvFname)
        simplecsvpath = os.path.join(actualsDir, stu + '.csv')

        # output html and png
        htmlout = os.path.join(actualsDir, stl + '.html')
        pngout = os.path.join(actualsDir, stu + '.png')
        pngLgOut = os.path.join(actualsDir, stu + '_lg.png')
        png500out = os.path.join(actualsDir, stu + '500.png')
        analyzeout = os.path.join(actualsDir, stl + '_stats.txt')
        analyzeText = None
        districts = self.config[cname].args['-d']

        #if any_newerthan( (pb, mppb, csvpath, zipname), (pngout, png500out, pngLgOut, htmlout, analyzeout), noSourceCallback=noSource):
        if True:
            if newerthan(csvpath, simplecsvpath):
                csvToSimpleCsv(csvpath, simplecsvpath)

            # normal size
            if self.options.redraw or any_newerthan( (pb, mppb, simplecsvpath), pngout):
                cmd = [drendpath(), '-d', districts, '-P', pb, '--mppb', mppb, '--csv-solution', simplecsvpath, '--pngout', pngout]
                if highlight:
                    cmd += ['--hubidz', highlight]
                logging.info('%r', cmd)
                p = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, shell=False)
                retcode = p.wait()
                if retcode != 0:
                    logging.error('cmd `%s` retcode %s log %s', cmd, retcode, p.stdout.read())
                    sys.exit(1)

            # large image
            if self.options.redraw or any_newerthan( (pb, mppb_lg_path, simplecsvpath), pngLgOut):
                cmd = [drendpath(), '-d', districts, '-P', pb, '--mppb', mppb_lg_path, '--csv-solution', simplecsvpath, '--pngout', pngLgOut]
                if highlight:
                    cmd += ['--hubidz', highlight]
                logging.info('%r', cmd)
                p = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, shell=False)
                retcode = p.wait()
                if retcode != 0:
                    logging.error('cmd `%s` retcode %s log %s', cmd, retcode, p.stdout.read())
                    sys.exit(1)

            if any_newerthan( (pb, mppb, simplecsvpath, zipname), (htmlout, analyzeout)):
                analyzeText = measure_race(stl, districts, pb, simplecsvpath, htmlout, zipname, printcmd=lambda x: logging.info('%r', x))
                atout = open(analyzeout, 'w')
                atout.write(analyzeText)
                atout.close()
            if newerthan(pngout, png500out):
                subprocess.call(['convert', pngout, '-resize', '500x500', png500out])

        if analyzeText is None:
            atin = open(analyzeout, 'r')
            analyzeText = atin.read()
            atin.close()
        current_kmpp, current_spread, current_std = parseAnalyzeStats(analyzeText)
        if (current_kmpp is None) or (current_spread is None) or (current_std is None):
            logging.warn('failed to get current stats for %s from %s', stl, analyzeout)
        return current_kmpp, current_spread, current_std
    
    def buildBestSoFarDirs(self, configs=None):
        """$outdir/$XX_yyy/$id/{index.html,ba_500.png,ba.png,map.png,map500.png}
        With hard links from $XX_yyy/* to $XX_yyy/$id/* for the current best.
        Also build the top level index.html"""
        outdir = self.options.outdir
        if not os.path.isdir(outdir):
            os.makedirs(outdir)
        if configs is None:
            configs = self.getBestConfigs()
        stutodo = set()
        for cname, data in configs.items():
            (stu, rest) = cname.split('_', 1)
            self.buildReportDirForConfig(configs, cname, data, stu)
            stutodo.add(stu)
        for stu in stutodo:
            self.statedir(stu, configs)

        # build top level index.html
        newestconfig = self.newestWinner(configs)['config']
        newestname = configToName(newestconfig)
        pageabsurl = urljoin(self.options.siteurl, self.options.rooturl)
        cgipageabsurl = urllib.parse.quote_plus(pageabsurl)
        cgiimageurl = urllib.parse.quote_plus(urljoin(self.options.siteurl, self.options.rooturl, newestconfig, 'map500.png'))
        
        index_html_path = os.path.join(outdir, 'index.html')
        index_html = open(index_html_path, 'w')
        index_html.write(djangotemplates.render('root_index.html',dict(
            statenav=self.statenav(None, configs),
            rooturl=self.options.rooturl,
            localtime=localtime(),
            nwinner=newestconfig,
            nwinnername=newestname,
            cgipageabsurl=cgipageabsurl,
            cgiimageurl=cgiimageurl,
            configjson=json.dumps(self.availableConfigs(configs)),
            google_analytics=_google_analytics(),
            social=self.getSocial(pageabsurl, cgipageabsurl),
            socialshare=self.safeSocialShare,
        )))
        index_html.close()
        logging.debug('wrote %s', index_html_path)
        self.copyResources()

    def availableConfigs(self, configs):
        # return list [ (nice name, path), ...] for js
        out = []
        for cname, data in configs.items():
            if not data.get('kmpp'):
                continue
            stu, var = cname.split('_', 1)
            name = states.nameForPostalCode(stu)
            out.append( (name + ' ' + var, cname) )
        return out

    def copyResources(self):
        outdir = self.options.outdir
        for resourcename in _resources:
            resourceSource = os.path.join(srcdir_, resourcename)
            resourceDest = os.path.join(outdir, resourcename)
            if newerthan(resourceSource, resourceDest):
                logging.debug('%s -> %s', resourceSource, resourceDest)
                shutil.copy2(resourceSource, resourceDest)

    def summaryCsv(self, stu, statename, my_kmpp, my_spread, my_std, current_kmpp, current_spread, current_std):
        if self.summaryCsvWriter is None:
            return
        self.summaryCsvWriter.writerow([stu, statename, my_kmpp, my_spread, my_std, current_kmpp, current_spread, current_std])

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
    argp.add_option('--rooturl', dest='rooturl', default='file://' + os.path.abspath('.'), help='root suitable for making relative urls off of')
    argp.add_option('--siteurl', dest='siteurl', default='http://bdistricting.com/', help='for fully qualified absolute urls')
    argp.add_option('--redraw', dest='redraw', action='store_true', default=False)
    argp.add_option('--rehtml', dest='rehtml', action='store_true', default=False)
    argp.add_option('--config', dest='configlist', action='append', default=[])
    argp.add_option('--actuals', dest='actualdir', default=None, help='contains /{cd,sldu,sldl}')
    argp.add_option('--summary', dest='summarypath', default=None, help='path to summary.csv')
    (options, args) = argp.parse_args()
    if options.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    options.configlist = options.configlist + args
    x = SubmissionAnalyzer(options, dbpath='.status.sqlite3')
    if options.summarypath:
        x.summaryCsvFile = open(options.summarypath, 'wt')
        x.summaryCsvWriter = csv.writer(x.summaryCsvFile)
        x.summaryCsvWriter.writerow(['stu', 'state and delegation', 'my kmpp', 'my spread', 'my std', 'current kmpp', 'current spread', 'current std'])
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
