#!/usr/bin/python

"""HTTP Server for browsing results from runallstates.py"""

__author__ = "Brian Olson"


import http.server
import base64
import cgi
import os
import re
import http.server
import threading
import traceback
import time
import zlib

from . import kmppspreadplot
from . import plotstatlog


plotlib = None
plotlibModifiedTime = None

def getPlotlibJs():
	global plotlib
	global plotlibModifiedTime
	bindir = os.path.dirname(__file__)
	for name in ['plotlib_compiled.js', 'plotlib.js']:
		path = os.path.join(bindir, name)
		if os.path.exists(path):
			plstat = os.stat(path)
			if (plotlib is not None) and (plotlibModifiedTime is not None) and (plotlibModifiedTime >= plstat.st_mtime):
				return plotlib
			f = open(path, 'r')
			plotlib = f.read()
			f.close()
			plotlibModifiedTime = plstat.st_mtime
			return plotlib
	return None


def sizeStr(bytes):
	if bytes > 10000000000000:
		return '%d TB' % (bytes / 1000000000000)
	if bytes > 1000000000000:
		return '%.2f TB' % (bytes / 1000000000000.0)
	if bytes > 100000000000:
		return '%d GB' % (bytes / 1000000000)
	if bytes > 10000000000:
		return '%d GB' % (bytes / 1000000000)
	if bytes > 1000000000:
		return '%.2f GB' % (bytes / 1000000000.0)
	if bytes > 100000000:
		return '%d MB' % (bytes / 1000000)
	if bytes > 10000000:
		return '%d MB' % (bytes / 1000000)
	if bytes > 1000000:
		return '%.2f MB' % (bytes / 1000000.0)
	if bytes > 100000:
		return '%d kB' % (bytes / 1000)
	if bytes > 10000:
		return '%d kB' % (bytes / 1000)
	return '%d' % (bytes)


def countStatsumInDir(dir):
	count = 0
	for (path, dirs, files) in os.walk(dir):
		if 'statsum' in files:
			count += 1
	return count


class htmlDirListing(object):
	def __init__(self, rooturl, dirpath, entries):
		self.rooturl = rooturl
		self.dirpath = dirpath
		self.entries = entries

	def htmlDirEntryForDir(self, entry, fpath):
		return """<tr class="ld"><td class="dn"><a href="%s/">%s/</a></td></tr>""" % (
			entry, entry)

	def htmlDirEntry(self, entry):
		fpath = os.path.join(self.dirpath, entry)
		if os.path.isdir(fpath):
			return self.htmlDirEntryForDir(entry, fpath)
		fst = os.stat(fpath)
		size = sizeStr(fst.st_size)
		when = time.strftime("%Y-%m-%d %H:%M:%S",time.gmtime(fst.st_mtime))
		return """<tr class="lf"><td class="fn"><a href="%s">%s</a></td>
<td class="fs">%s</td><td class="ft">%s</td></tr>""" % (
			entry, entry, size, when)

	def __str__(self):
		out = ['<table class="dl">']
		for ent in self.entries:
			if ent.startswith('.'):
				continue
			out.append(self.htmlDirEntry(ent))
		out.append('</table>')
		return ''.join(out)



class htmlRootDirListing(htmlDirListing):
	def __init__(self, rooturl, dirpath, entries, doCountStatsums=False):
		htmlDirListing.__init__(self, rooturl, dirpath, entries)
		# Cauntion, filesystem intensive, lots of recursive os.walk()ing.
		self.doCountStatsums = doCountStatsums
	
	def htmlDirEntryForDir(self, entry, fpath):
		nobest = ''
		countstr = ''
		if self.doCountStatsums:
			count = countStatsumInDir(fpath)
			if count:
				countstr = ' - %d runs' % count
		if not os.path.exists(os.path.join(fpath, 'best')):
			nobest = ' - <b>no best</b> - <a href="%s/kmpp_spread.svg">kmpp_spread.svg</a>' % entry
		return """<tr class="ld"><td class="dn"><a href="%s/">%s/</a>%s%s</td></tr>""" % (
			entry, entry, countstr, nobest)
	
	def __str__(self):
		if self.doCountStatsums:
			prefix = '<div><a href="/">no counts</a></div>'
		else:
			prefix = '<div><a href="/?count=1">count statsums</a></div>'
		return prefix + super(htmlRootDirListing, self).__str__()


def tail(linesource, lines=10):
	special = []
	out = []
	for line in linesource:
		if line[0] == '#':
			special.append(line)
			continue
		out.append(line)
		while len(out) > lines:
			out.pop(0)
	return special + out


# base64.b64encode(zlib.compress(open('favicon.ico','rb').read(), 9))
favicon_ico_zlib_b64 = 'eNpjYGAEQgEBAQYQ0GBkYBAD0UAMElEAYkYGFrDcAQZM8P//fwbWz38YmFL+M7Bx3GVo4fjEsJTBjeEKUO7VaiYGke3yDIIpDAysOmEMVoUTGA4cOMDQ0NDA4ODgANZvbMDAbMBgwMzADCIMmCEcAyAF5RuD5IGqcACofohiLDRcHqwak6Y1AAD7Wxzg'
favicon_ico = None


def getFavicon():
	global favicon_ico
	if favicon_ico is None:
		favicon_ico = zlib.decompress(base64.b64decode(favicon_ico_zlib_b64))
	return favicon_ico


listing_css = (
"""td.fs{font-family:'courier';padding-left:1em;text-align:right;}"""
"""td.ft{font-family:'courier';padding-left:1em;text-align:right;}"""
"""div.ib{display:inline-block;}"""  # just an inline block
"""div.ic{display:inline-block;}"""  # image container
"""img.pt{border:1px solid blue;}"""
"""div.log{width:80ex;font-family:monospace;"""
"""border:1px solid black;padding:2px;display:inline-block;}"""
"""canvas.graph{border:1px solid #333;}"""
"""div.gst{margin:3px;padding-left:15px;}"""
"""div.logline{}""")
#width:400px;height:300px;

def imgCallout(url, name):
	"""For some image file in the directory, show a small version of it."""
	if name is None:
		name = url
	return """<div class="ic"><a href="%s"><img class="pt" src="%s" width="100px"><br>%s</a></div>""" % (
		url, url, name)


def tailFileDiv(dirpath, name, boxclass, lineclass, numlines=10):
	"""For some log file, show the last few lines of it in a div."""
	lastlines = tail(open(os.path.join(dirpath, name), 'r'), numlines)
	out = []
	if lastlines:
		out.append("""<div class="%s"><div>%s:</div>""" % (boxclass, name))
		for line in lastlines:
			line = line.strip()
			out.append("""<div class="%s">%s</div>""" % (lineclass, line))
		out.append("""</div>""")
		return ''.join(out)
	return None


def linkifyPath(x):
	if os.path.exists(x):
		if os.path.isdir(x) and not x.endswith('/'):
			x = x + '/'
		return '<a href="%s">%s</a>' % (x, x)
	return x


def linkifyBestlogReplacer(m):
	x = m.group(0)
	return linkifyPath(x)


RUN_PATH_RE = re.compile(r'[A-Z][A-Z][_A-Za-z]*/[0-9_]+')


def linkifyBestlog(bestlogText):
	return RUN_PATH_RE.sub(linkifyBestlogReplacer, bestlogText)


def linkifyRunlogReplacer(m):
	x = m.group(0)
	x = x.replace(' ', '/')
	return linkifyPath(x)


RUNLOG_PATH_RE = re.compile(r'[A-Z][A-Z][_A-Za-z]* [0-9_]+')


def linkifyRunlog(runlogText):
	return RUNLOG_PATH_RE.sub(linkifyRunlogReplacer, runlogText)


def writeStatlogDisplay(dirpath, name, out):
	js = getPlotlibJs()
	if not js:
		return
	stats = plotstatlog.statlog(os.path.join(dirpath, name))
	out.write("""<div class="ib"><div>km/p</div><canvas width="400" height="300" class="graph" id="g_kmpp"></canvas></div>
<div class="ib"><div>std</div><canvas width="400" height="300" class="graph" id="g_std"></canvas></div>
<div class="ib"><div>spread</div><canvas width="400" height="300" class="graph" id="g_spread"></canvas></div>
<div class="ib" style="display:none"><div>nodist</div><canvas width="400" height="300" class="graph" id="g_nodist"></canvas></div>
<script>
""")
	out.write(js)
	out.write('window.redistricter_statlog=')
	stats.writeJson(out)
	out.write(""";
lineplot(document.getElementById('g_kmpp'), window.redistricter_statlog['kmpp']);
lineplot(document.getElementById('g_std'), window.redistricter_statlog['std']);
lineplot(document.getElementById('g_spread'), window.redistricter_statlog['spread']);
if (window.redistricter_statlog['nodist']) {
	var nodist = document.getElementById('g_nodist');
	nodist.parentNode.style.display = 'inline-block';
	lineplot(nodist, window.redistricter_statlog['nodist']);
}
</script>""")


class ResultServerHandler(http.server.SimpleHTTPRequestHandler):
	def __init__(self, request, client_address, server, extensions=None, actions=None):
		self.extensions = extensions
		self.dirExtra = None
		self.query = {}
		#print 'request=' + repr(request)
		#print 'dir(self)=' + repr(dir(self))
		self.actions = actions
		if self.actions is None:
			self.actions = {}
		self.extensions_map.update({
			'.png': 'image/png',
			'.jpg': 'image/jpeg',
			'.svg': 'image/svg+xml',
			'statsum': 'text/plain',
			'statlog': 'text/plain',
			'.gz': 'application/gzip',
			'.bz2': 'application/bzip2',
#			'': '',
})
		http.server.SimpleHTTPRequestHandler.__init__(self, request, client_address, server)

	def write(self, x):
		if isinstance(x, str):
			self.wfile.write(x.encode('utf-8'))
		elif hasattr(x, '__str__'):
			self.wfile.write(str(x).encode('utf-8'))
		else:
			self.wfile.write(x)
	
	def GET_dir(self, path, fpath):
		they = os.listdir(fpath)
		if 'index.html' in they:
			http.server.SimpleHTTPRequestHandler.do_GET(self)
			return
		they.sort()  # TODO: case-insensitive sort? dirs-first?
		self.send_response(200)
		self.send_header('Content-Type', 'text/html')
		self.end_headers()
		self.write("""<!doctype html>\n<html><head><title>%s</title><style>%s</style></head><body><h1>%s</h1>""" % (self.path, listing_css, self.path))
		for sf in ['runlog', 'bestlog', 'statlog', 'statsum']:
			if not sf in they:
				continue
			sfStuff = tailFileDiv(fpath, sf, 'log', 'logline', 10)
			if sfStuff:
				if sf == 'bestlog':
					sfStuff = linkifyBestlog(sfStuff)
				if sf == 'runlog':
					sfStuff = linkifyRunlog(sfStuff)
				self.write(sfStuff)
		for x in they:
			if x[-4:].lower() == '.png':
				self.write(imgCallout(x, x))
			if x[:7].lower() == 'statlog':
				writeStatlogDisplay(fpath, x, self.wfile)
		if self.dirExtra:
			self.write(self.dirExtra)
		self.write("""<div><a href="kmpp_spread.svg">kmpp_spread.svg</a></div>""")
		if path == '':
			for action in self.actions.values():
				self.write(action.html)
			self.write(htmlRootDirListing('', fpath, they, self.query.get('count') != None))
		else:
			self.write(htmlDirListing('', fpath, they))
		self.write("""</body></html>\n""")
	
	def GET_kmppspreadplot(self, path, fpath):
		try:
			self.GET_kmppspreadplot_inner(path, fpath)
		except Exception as e:
			e.printStackTrace()

	def GET_kmppspreadplot_inner(self, path, fpath):
		self.send_response(200)
		self.send_header('Content-Type', 'image/svg+xml')
		self.end_headers()
		svgout = kmppspreadplot.svgplotter('kmppspreadplot.svg', self.wfile)
		kmppspreadplot.walk_statsums(svgout, fpath, True)
		svgout.close()
		
	def runExtensions(self):
		if self.extensions is None:
			return False
		# Could something be both callable and iterable? Probably. Uh, don't do that.
		if hasattr(self.extensions, '__call__') and self.extensions(self):
			return True
		if hasattr(self.extensions, '__iter__'):
			for ex in self.extensions:
				if hasattr(ex, '__call__') and ex(self):
					return True
		return False
	
	def do_GET(self):
		pathQuery = self.path.split('?', 1)
		self.path = pathQuery[0]
		if len(pathQuery) > 1:
			self.query = cgi.parse_qs(pathQuery[1])
		if self.runExtensions():
			return
		if self.path == '/favicon.ico':
			self.send_response(200)
			self.end_headers()
			self.write(getFavicon())
			return
		path = self.path.lstrip('/')
		cwd = os.path.abspath(os.getcwd())
		fpath = os.path.abspath(os.path.join(cwd, path))
		if not fpath.startswith(cwd):
			self.log_error('bad path %s', self.path)
			return
		if path.endswith('/kmpp_spread.svg'):
			self.GET_kmppspreadplot(path, os.path.dirname(fpath))
			return
		if os.path.isdir(fpath):
			self.GET_dir(path, fpath)
			return
		http.server.SimpleHTTPRequestHandler.do_GET(self)
	
	def do_POST(self):
		pathQuery = self.path.split('?', 1)
		dest = '/'
		if pathQuery[0] == '/action':
			query = {}
			if len(pathQuery) > 1:
				query = cgi.parse_qs(pathQuery[1])
			dest = query.get('dest', [dest])
			dest = dest[0]
			action = query.get('a')
			if action:
				action = action[0]
			if action and (action in self.actions):
				try:
					self.actions[action]()
				except:
					self.send_response(500)
					self.end_headers()
					self.write(traceback.format_exc())
					return
		else:
			self.send_response(400)
			self.end_headers()
			self.write('bogus action "%s"\n' % (self.path,))
			return
		self.send_response(303)
		self.send_header('Location', dest)
		self.end_headers()
		return


class TouchAction(object):
	def __init__(self, path, name, paramname):
		self.path = path
		self.name = name
		self.paramname = paramname
	
	@property
	def html(self):
		return '''<div><form action="/action?a=%s" method="POST"><input type="submit" value="%s"></form></div>''' % (self.paramname, self.name)
	
	def __call__(self, *args, **kwargs):
		if not os.path.exists(self.path):
			f = open(self.path, 'w')
			f.write(time.strftime('%Y%m%d_%H%M%S'))
			f.close()
		else:
			f = open(self.path, 'ab')
			f.close()
	
	def setDict(self, x):
		x[self.paramname] = self

class RuntimeExtensibleHandler(object):
	def __init__(self, extensions=None, actions=None):
		self.extensions = extensions
		self.actions = actions
		if self.actions is None:
			self.actions = {}
	
	def __call__(self, request, client_address, server):
		return ResultServerHandler(request, client_address, server, self.extensions, self.actions)


def runServer(port=8080):
	reh = RuntimeExtensibleHandler(None)
	httpd = http.server.HTTPServer( ('',port), reh )
	httpd.serve_forever()


def startServer(port=8080, exitIfLastThread=True, extensions=None, actions=None):
	"""extensions should be callable and take an argument of
	SimpleHTTPServer.SimpleHTTPRequestHandler"""
	reh = RuntimeExtensibleHandler(extensions, actions)
	httpd = http.server.HTTPServer( ('',port), reh )
	t = threading.Thread(target=httpd.serve_forever, args=())
	t.setDaemon(exitIfLastThread)
	t.start()
	return (t, httpd)


if __name__ == '__main__':
	import optparse
	argp = optparse.OptionParser()
	argp.add_option('--port', dest='port', type='int', default=8080, help='port to serve stats on via HTTP')
	argp.add_option('--count-runs', '--count', dest='countRuns', action='store_true', default=False, help='count statsum results under result dirs and display them in root result display. can be slow.')
	(options, args) = argp.parse_args()
	runServer(options.port)
