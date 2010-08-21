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
