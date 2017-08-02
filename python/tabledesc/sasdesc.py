#!/usr/bin/python


"""Parse Census provided SAS files that describe data tables.
Produce more interesting formats.
Census file URLs:
http://www.census.gov/support/2000/SF1/SF1SAS.zip
http://www.census.gov/support/2000/SF3/SF3SAS.zip

./sasdesc.py < SF101.Sas > SF101.html
"""


import logging
import optparse
import os
import re
import sys
import xml.dom.minidom
import zipfile


# SF105.Sas doesn't have a semicolon ending its LABEL statement. Bug?
labelpat = re.compile(r"LABEL\s+(?:(\S+='[^']+')|/\*.*\*/|\s+)+(?:;|INPUT)", re.MULTILINE|re.DOTALL)
commentpat = re.compile('/\*(.*?)\*/', re.MULTILINE|re.DOTALL)
necpat = re.compile(r"(?:\S+='[^']+'|/\*.*?\*/)", re.MULTILINE|re.DOTALL)

nepat = re.compile(r"([^=]+)='([^']+)'", re.MULTILINE|re.DOTALL)


#x = re.compile(r'LABEL(.*);', re.MULTILINE|re.DOTALL)

#tokenpat = re.compile(r"(\S+?(?:'[^']+')?)", re.MULTILINE|re.DOTALL)
#qpat = re.compile(r"('[^']+')", re.MULTILINE|re.DOTALL)
#neqpat = re.compile(r"(\S+='[^']+')", re.MULTILINE|re.DOTALL)
#l2pat = re.compile(r"LABEL\s+(?:\s*(\S+='[^']+')\s*)+;", re.MULTILINE|re.DOTALL)


class record(object):
	def __init__(self, comment=None, codename=None, path=None):
		#self.index = None
		self.comment = comment
		self.codename = codename
		# self.path[-1] is the name of this element
		self.path = path
	
	def xml_string(self, index=None):
		domi = xml.dom.minidom.getDOMImplementation()
		doc = domi.createDocument(None, 'record_doc', None)
		record = doc.createElement('record')
		if index is not None:
			ip = doc.createElement('index')
			ip.appendChild(doc.createTextNode(str(index)))
		comment = doc.createElement('comment')
		comment.appendChild(doc.createTextNode(self.comment))
		record.appendChild(comment)
		codename = doc.createElement('codename')
		codename.appendChild(doc.createTextNode(self.codename))
		record.appendChild(codename)
		path = doc.createElement('path')
		for x in self.path:
			pe = doc.createElement('pe')
			pe.appendChild(doc.createTextNode(x))
			path.appendChild(pe)
		record.appendChild(path)
		return record.toxml()
	
	def as_hash(self, index=None):
		out = {
			'comment': self.comment,
			'codename': self.codename,
			'path': self.path,
		}
		if index is not None:
			out['index'] = index
		return out
	
	def html_table_row(self, index=None):
		if index is None:
			index = ''
		# index, codename, 
		return '<tr><td>%s</td><td>%s</td><td class="in%d">%s</td></tr>' % (
			index, self.codename, len(self.path), self.path[-1])


def countws(s):
	count = 0
	for c in s:
		if c == ' ':
			count += 1
		elif c == '\t':
			count += 8
		else:
			return count


brpat = re.compile('(?:&nbsp;|<br>|\s)+', re.IGNORECASE)


def namefilter(s):
	"""Remove some crazy formatting from the census supplied .Sas files."""
	s = s.strip()
	s = brpat.sub(' ', s)
	return s


class sasdesc(object):
	def __init__(self):
		self.records = None

	def read(self, fname):
		f = open(fname, 'r')
		raw = f.read()
		self.read_data(raw)
	
	def read_data(self, raw):
		m = labelpat.search(raw)
		labelexp=m.group(0)
		logging.debug('got %d bytes of label expression' % len(labelexp))
		tokens=necpat.findall(labelexp)
		records = []
		comments = []
		commentmode = 1
		comment = None
		indents = [0]
		path = ['']
		
		for tok in tokens:
			logging.debug(tok)
			if tok[:2] == '/*':
				m = commentpat.match(tok)
				cbody = m.group(1).strip()
				if commentmode == 1:
					comment = None
					comments = [cbody]
					commentmode = 2
				else:
					comments.append(cbody)
				logging.debug(repr((commentmode, comments)))
			else:
				commentmode = 1
				if comment is None:
					comment = '\n'.join(comments)
					logging.debug('comment: ' + comment)
				m = nepat.match(tok)
				codename = m.group(1)
				name = m.group(2)
				ind = countws(name)
				logging.debug('indents: ' + repr(indents))
				logging.debug('path: ' + repr(path))
				logging.debug('+ ' + name)
				while ind < indents[-1]:
					indents.pop()
					path.pop()
				if ind > indents[-1]:
					indents.append(ind)
					path.append(namefilter(name))
				else:
					indents[-1] = ind
					path[-1] = namefilter(name)
				logging.debug('new indents: ' + repr(indents))
				logging.debug('new path: ' + repr(path))
				records.append(record(comment, codename, list(path)))
		self.records = records
	
	def commentList(self):
		cl = []
		# not the most efficient, but easy.
		for r in self.records:
			if r.comment not in cl:
				cl.append(r.comment)
		return cl
		
	def write_html_table(self, out):
		lastcomment = None
		out.write('<table><tr><th>csv column</th><th>codename</th><th>description</th></tr>\n')
		for (i, rec) in enumerate(self.records):
			if rec.comment != lastcomment:
				out.write(
					'\n<tr><td colspan="3" class="ch">%s</td></tr>\n'
					% rec.comment.replace('\n', '<br />'))
				lastcomment = rec.comment
			out.write(rec.html_table_row(i))
		out.write('</table>\n')
	
	def write_html_doc(self, out):
		out.write("""<!DOCTYPE html>
	<html><head><title>census data table keys</title><link rel="stylesheet" type="text/css" href="dt.css" /><style>td.ch{border-bottom: solid 1px black;padding-top:5px;}""")
	#td.in2{padding-left:3em;}td.in3{padding-left:6em;}td3.in4{padding-left:9em;}...
		for x in xrange(2, 9):
			out.write('td.in%d{padding-left:%dem;}' % (x, (x-1)*3))
		out.write("""</style></head><body bgcolor="#ffffff" text="#000000">""")
		self.write_html_table(out)
		out.write('</body></html>\n')


def main(argv):
	op = optparse.OptionParser()
	op.add_option('--outdir', dest='outdir', default='.')
	op.add_option('--loglevel', dest='loglevel', default=logging.INFO)
	op.add_option('--doindex', dest='doindex', action='store_true', default=False)
	(options, args) = op.parse_args()
	
	ziplist = []
	flist = []
	logging.basicConfig(level=int(options.loglevel))
	for arg in args:
		if arg.lower().endswith('.zip') and os.path.isfile(arg):
			ziplist.append(arg)
		elif os.path.isfile(arg):
			flist.append(arg)
		else:
			sys.stderr.write('bogus arg "%s"\n' % arg)
			sys.exit(1)
			return 1
	if (not ziplist) and (not flist):
		# from stdin to stdout
		raw = sys.stdin.read()
		it = sasdesc()
		it.read_data(raw)
		it.write_html_doc(sys.stdout)
		sys.stdout.flush()
		return 0
	outlist = []
	for x in ziplist:
		zf = zipfile.ZipFile(x, 'r')
		for name in zf.namelist():
			if name.lower().endswith('.sas'):
				basename = os.path.basename(name)
				nameroot = basename[:-4]
				raw = zf.read(name)
				sys.stderr.write('%s %s (%d)\n' % (x, name, len(raw)))
				desc = sasdesc()
				desc.read_data(raw)
				outlist.append((nameroot + '.html', desc.commentList()))
				outpath = os.path.join(options.outdir,nameroot + '.html')
				sys.stderr.write('\t-> %s\n' % outpath)
				out = open(outpath, 'w')
				desc.write_html_doc(out)
				out.close()
		zf.close()
	for name in flist:
		infile = open(name, 'r')
		raw = infile.read()
		infile.close()
		sys.stderr.write('%s (%d)\n' % (name, len(raw)))
		basename = os.path.basename(name)
		if basename.lower().endswith('.sas'):
			nameroot = basename[:-4]
		else:
			nameroot = basename
		desc = sasdesc()
		desc.read_data(raw)
		outlist.append((nameroot + '.html', desc.commentList()))
		outpath = os.path.join(options.outdir,nameroot + '.html')
		sys.stderr.write('\t-> %s\n' % outpath)
		out = open(outpath, 'w')
		desc.write_html_doc(out)
		out.close()
	if options.doindex:
		ifname = os.path.join(options.outdir, 'index.html')
		sys.stderr.write('-> %s\n' % ifname)
		indexfile = open(ifname, 'w')
		indexfile.write("""<!DOCTYPE html>
<html><head><title>census data table keys</title>
<link rel="stylesheet" type="text/css" href="dt.css" />
<style>td.ch{border-bottom: solid 1px black;padding-top:5px;}</style></head>
<body bgcolor="#ffffff" text="#000000">
<h1>Census Data Table Keys</h1>
<table border="0" class="tflist">
<tr><th>file</th><th>contents</th></tr>
""")
		for fname, commentList in outlist:
			indexfile.write("""<tr><td><a href="%s">%s</a></td><td><div class="c">%s</div></td></tr>\n""" % (
				fname, fname, '</div><div class="c">'.join(commentList)))
		indexfile.write("""</table></body></html>\n""")
		indexfile.close()


if __name__ == '__main__':
	main(sys.argv)
