#!/usr/bin/python

"""HTTP Server for browsing results from runallstates.py"""

__author__ = "Brian Olson"


import BaseHTTPServer
import base64
import os
import SimpleHTTPServer
import threading
import time
import zlib

import plotstatlog


plotlib = None

def getPlotlibJs():
	global plotlib
	if plotlib is not None:
		return plotlib
	bindir = os.path.dirname(__file__)
	for name in ['plotlib_compiled.js', 'plotlib.js']:
		path = os.path.join(bindir, name)
		if os.path.exists(path):
			f = open(path, 'r')
			plotlib = f.read()
			f.close()
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


def htmlDirEntry(rooturl, dirpath, entry):
	fpath = os.path.join(dirpath, entry)
	if os.path.isdir(fpath):
		return """<tr class="ld"><td class="dn"><a href="%s/">%s/</a></td></tr>""" % (
			entry, entry)
	fst = os.stat(fpath)
	size = sizeStr(fst.st_size)
	when = time.strftime("%Y-%m-%d %H:%M:%S",time.gmtime(fst.st_mtime))
	return """<tr class="lf"><td class="fn"><a href="%s">%s</a></td>
<td class="fs">%s</td><td class="ft">%s</td></tr>""" % (
		entry, entry, size, when)


def htmlDirListing(rooturl, dirpath, entries):
	out = ['<table class="dl">']
	for ent in entries:
		if ent.startswith('.'):
			continue
		out.append(htmlDirEntry(rooturl, dirpath, ent))
	out.append('</table>')
	return ''.join(out)


def tail(linesource, lines=10):
	out = []
	for line in linesource:
		out.append(line)
		while len(out) > lines:
			out.pop(0)
	return out


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


class ResultServerHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
	def __init__(self, request, client_address, server, extensions=None):
		self.extensions = extensions
		self.dirExtra = None
		SimpleHTTPServer.SimpleHTTPRequestHandler.__init__(self, request, client_address, server)
	
	def GET_dir(self, path, fpath):
		they = os.listdir(fpath)
		if 'index.html' in they:
			SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)
			return
		they.sort()  # TODO: case-insensitive sort? dirs-first?
		self.send_response(200)
		self.send_header('Content-Type', 'text/html')
		self.end_headers()
		self.wfile.write("""<html><head><title>%s</title><style>%s</style></head><body><h1>%s</h1>""" % (self.path, listing_css, self.path))
		for sf in ['runlog', 'bestlog', 'statlog', 'statsum']:
			if not sf in they:
				continue
			sfStuff = tailFileDiv(fpath, sf, 'log', 'logline', 10)
			if sfStuff:
				self.wfile.write(sfStuff)
		for x in they:
			if x[-4:].lower() == '.png':
				self.wfile.write(imgCallout(x, x))
			if x[:7].lower() == 'statlog':
				writeStatlogDisplay(fpath, x, self.wfile)
		if self.dirExtra:
			self.wfile.write(self.dirExtra)
		self.wfile.write(htmlDirListing('', fpath, they))
		self.wfile.write("""</body></html>\n""")
	
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
		if self.runExtensions():
			return
		if self.path == '/favicon.ico':
			self.send_response(200)
			self.end_headers()
			self.wfile.write(getFavicon())
			return
		path = self.path.lstrip('/')
		cwd = os.path.abspath(os.getcwd())
		fpath = os.path.abspath(os.path.join(cwd, path))
		if not fpath.startswith(cwd):
			self.log_error('bad path %s', self.path)
			return
		if os.path.isdir(fpath):
			self.GET_dir(path, fpath)
			return
		SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

class RuntimeExtensibleHandler(object):
	def __init__(self, extensions=None):
		self.extensions = extensions
	
	def __call__(self, request, client_address, server):
		return ResultServerHandler(request, client_address, server, self.extensions)


def runServer(port=8080):
	reh = RuntimeExtensibleHandler(None)
	httpd = BaseHTTPServer.HTTPServer( ('',port),  reh )
	httpd.serve_forever()


def startServer(port=8080, exitIfLastThread=True, extensions=None):
	"""extensions should be callable and take an argument of
	SimpleHTTPServer.SimpleHTTPRequestHandler"""
	reh = RuntimeExtensibleHandler(extensions)
	httpd = BaseHTTPServer.HTTPServer( ('',port),  reh )
	t = threading.Thread(target=httpd.serve_forever, args=(port,))
	t.setDaemon(exitIfLastThread)
	t.start()
	return (t, httpd)


if __name__ == '__main__':
	import optparse
	argp = optparse.OptionParser()
	argp.add_option('--port', dest='port', type='int', default=8080, help='port to serve stats on via HTTP')
	(options, args) = argp.parse_args()
	runServer(options.port)
