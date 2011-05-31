#!/usr/bin/python

import os
import shutil
import sqlite3

# sqlite3 .status.sqlite3

cwd = os.getcwd()

def main():
	bad_config = 'AZ_Senate'
	atticdir = os.path.normpath(os.path.join(cwd, 'Attic', bad_config))
	if not os.path.exists(atticdir):
		print 'mkdir -p "%s"' % (atticdir,)
		os.makedirs(atticdir)
	else:
		print 'atticdir', atticdir
	conn = sqlite3.connect('.status.sqlite3')
	c = conn.cursor()
	rows = c.execute('SELECT id, path FROM submissions WHERE config = ?', (bad_config,))
	c2 = conn.cursor()
	for rid, path in rows:
		print rid, path
		#path = path[0]
		if path.startswith('/'):
			path = path[1:]
		p2 = os.path.normpath(os.path.join(cwd, path))
		#print 'rid=%s path="%s" cwd="%s" p2="%s"' % (rid, path, cwd, p2)
		#print p2
		baseit = os.path.basename(p2)
		dest = os.path.join(atticdir, baseit)
		if os.path.exists(p2):
			if not os.path.exists(dest):
				print '%s => %s' % (p2, dest)
				shutil.move(p2, dest)
			else:
				print 'dest "%s" already exists, maybe you should rm the source:' % (dest,)
				print 'rm "%s"' % (p2,)
		else:
			if not os.path.exists(dest):
				print 'src "%s" already gone' % (p2,)
			else:
				print 'already moved'
		c2.execute('DELETE FROM submissions WHERE id = ?', (rid,))
		print 'deleted from db'
	conn.commit()
	c2.close()
	c.close()

if __name__ == '__main__':
	#print 'cwd=', cwd
	main()
