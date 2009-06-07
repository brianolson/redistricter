#!/usr/bin/python

"""Read in census_tables.txt, check it, calculate column offsets into files.

Can generate protobufs containing the data if redata_pb2.py exists
(cd ..; protoc redata.proto --python_out=tabledesc)
"""


# TODO: rework this to parse census-supplied SAS configuration files
# http://www.census.gov/support/2000/SF1/SF1SAS.zip
# http://www.census.gov/support/2000/SF3/SF3SAS.zip
# etc.


import hashlib
import re
try:
	import redata_pb2
	haveProto = True
except:
	haveProto = False

tablere = re.compile(r'^# (P[^.]{1,9})\..*\[(\d+)\].*')

#tableFileName = ''
#tableName = ''
#expectedFieldCount = None

class tableitem(object):
	def __init__(self, colnum, path):
		self.colnum = colnum
		self.path = path

class table(object):
	def __init__(self, inFile, name, fields):
		self.inFile = inFile
		self.name = name
		self.expectedFieldCount = fields
		self.lines = []
		self.comments = []
		self.items = []
	
	def check(self):
		if self.expectedFieldCount != len(self.lines):
			print '%s:%s expected %d fields, found %d' % (
				self.inFile, self.name, self.expectedFieldCount, len(self.lines))
			return False
		return True

def tabCountAndTail(x):
	pos = 0
	while x[pos] == '\t':
		pos += 1
	return (pos, x[pos:])

class tablefile(object):
	def __init__(self, name):
		self.name = name
		self.tables = []
	
	def calculateColumnNumbers(self):
		"""Build tableitems with populated colnum and path"""
		# 0=fileid,1=state,2=chariter,3=cifsn,4=logrecno
		colnum = 5
		for t in self.tables:
			fieldstack = []
			for line in t.lines:
				(level, tail) = tabCountAndTail(line)
				if level == len(fieldstack):
					fieldstack.append(tail)
				elif level < len(fieldstack):
					fieldstack = fieldstack[0:level]
					fieldstack.append(tail)
				else:
					print 'discontiguous levels %d -> %d in %s:%s at tag "%s"' % (
						len(fieldstack), level,
						self.name, t.name, tail)
					fieldstack.append(tail)
				t.items.append(tableitem(colnum, list(fieldstack)))
				colnum += 1

	def makeProto(self):
		p = redata_pb2.TableFileDescription()
		p.name = self.name
		for t in self.tables:
			tp = p.table.add()
			tp.table = t.name
			tp.tableDescription = '\n'.join(t.comments)
			for it in t.items:
				coldesc = tp.column.add()
				coldesc.column = it.colnum
				for tag in it.path:
					coldesc.path.append(tag)
		print repr(p)
		return p
	
	def getProtoBytes(self):
		p = self.makeProto()
		return p.SerializeToString()
	
	def getString(self):
		out = 'file: %s\n' % self.name
		for t in self.tables:
			out += '# %s\n# %s\n' % (t.name, '\n# '.join(t.comments))
			for it in t.items:
				out += '%d\t%s\n' % (it.colnum, ', '.join(it.path))
		return out

def readtabledesc():
	tfiles = []
	currentTfile = None
	currentTable = None

	f = open('census_tables.txt', 'r')
	for line in f:
		if line.startswith('# file'):
			tf = line[6:]
			currentTfile = tablefile(tf.strip())
			tfiles.append(currentTfile)
	#		print 'start table file "%s"' % currentTfile.name
			continue
		m = tablere.match(line)
		if m:
			currentTable = table(currentTfile.name, m.group(1), int(m.group(2)))
	#		print 'found table "%s" (%d fields)' % (
	#			currentTable.name, currentTable.expectedFieldCount)
			currentTfile.tables.append(currentTable)
			continue
		if line.startswith('#'):
			currentTable.comments.append(line[1:].strip());
			continue
		currentTable.lines.append(line.rstrip())
	return tfiles

tfiles = readtabledesc()

# Check consistency
fieldcount = 0
for tf in tfiles:
	for t in tf.tables:
		t.check()
		fieldcount += len(t.lines)
	tf.calculateColumnNumbers()

print 'checked %d tables and %d fields' % (len(tfiles), fieldcount)

for tf in tfiles:
	if not tf.tables:
		continue
	if haveProto:
		outname = tf.name + '.TableFileDescription'
		print 'writing "%s"...' % outname
		out = open(outname, 'wb')
		raw = tf.getProtoBytes()
		print 'sha1: %s' % hashlib.sha1(raw).hexdigest()
		out.write(raw)
		out.close()
	print 'writing "%s.txt"...' % tf.name
	out = open(tf.name + '.txt', 'w')
	out.write(tf.getString())
	out.close()
	print 'done'
