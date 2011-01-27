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

def any_newerthan(alist, b):
	"""Return true if b doesn't exist or anything in a is newerthan b."""
	try:
		sb = os.stat(b)
	except:
		return True
	for a in alist:
		try:
			sa = os.stat(a)
		except:
			# item in alist doesn't exist, maybe raise an Exception?
			return False
		if sa.st_mtime > sb.st_mtime:
			return True
	return False
