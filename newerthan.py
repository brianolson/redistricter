"""module for utility function newerthan(a,b)"""

__author__ = "Brian Olson"

import os

def newerthan(a, b):
	"""Return true if a is newer than b, or a exists and b doesn't."""
	try:
		sa = os.stat(a)
	except:
		return False
	try:
		sb = os.stat(b)
	except:
		return True
	return sa.st_mtime > sb.st_mtime


def _mystat_one(x):
	"""Return (is_missing, st_mtime)"""
	try:
		st = os.stat(x)
		return False, st.st_mtime
	except OSError, oe:
		return True, None


def get_oldest_mtime(b):
	"""Return (any_missing, oldest_mtime)"""
	any_missing = False
	oldest_mtime = None
	if isinstance(b, basestring):
		return _mystat_one(b)
	try:
		for x in b:
			try:
				st = os.stat(x)
				if (oldest_mtime is None) or (st.st_mtime < oldest_mtime):
					oldest_mtime = st.st_mtime
			except OSError, oe:
				any_missing = True
	except TypeError, te:
		# x is not iterable
		return _mystat_one(b)
	return any_missing, oldest_mtime


def any_newerthan(alist, b, noSourceCallback=None):
	"""Return true if b doesn't exist or anything in a is newerthan b.
	b may be a single path or iterable."""
	out, out_mtime = get_oldest_mtime(b)
	for a in alist:
		try:
			sa = os.stat(a)
		except:
			if noSourceCallback is not None:
				noSourceCallback(a)
			return False
		if (out_mtime is not None) and (sa.st_mtime > out_mtime):
			out = True
	return out
