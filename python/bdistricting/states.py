#!/usr/bin/python

import csv
import os

states = [
# name, abbrev, fips number
	('Alabama',	'AL',	1),
	('Alaska',	'AK',	2),
	('Arizona',	'AZ',	4),
	('Arkansas',	'AR',	5),
	('California',	'CA',	6),
	('Colorado',	'CO',	8),
	('Connecticut',	'CT',	9),
	('Delaware',	'DE',	10),
	#('Washington D.C.',	'DC',	11),
	('Florida',	'FL',	12),
	('Georgia',	'GA',	13),
	('Hawaii',	'HI',	15),
	('Idaho',	'ID',	16),
	('Illinois',	'IL',	17),
	('Indiana',	'IN',	18),
	('Iowa',	'IA',	19),
	('Kansas',	'KS',	20),
	('Kentucky',	'KY',	21),
	('Louisiana',	'LA',	22),
	('Maine',	'ME',	23),
	('Maryland',	'MD',	24),
	('Massachusetts',	'MA',	25),
	('Michigan',	'MI',	26),
	('Minnesota',	'MN',	27),
	('Mississippi',	'MS',	28),
	('Missouri',	'MO',	29),
	('Montana',	'MT',	30),
	('Nebraska',	'NE',	31),
	('Nevada',	'NV',	32),
	('New Hampshire',	'NH',	33),
	('New Jersey',	'NJ',	34),
	('New Mexico',	'NM',	35),
	('New York',	'NY',	36),
	('North Carolina',	'NC',	37),
	('North Dakota',	'ND',	38),
	('Ohio',	'OH',	39),
	('Oklahoma',	'OK',	40),
	('Oregon',	'OR',	41),
	('Pennsylvania',	'PA',	42),
	#('Puerto Rico',	'PR',	72),
	('Rhode Island',	'RI',	44),
	('South Carolina',	'SC',	45),
	('South Dakota',	'SD',	46),
	('Tennessee',	'TN',	47),
	('Texas',	'TX',	48),
	('Utah',	'UT',	49),
	('Vermont',	'VT',	50),
	('Virginia',	'VA',	51),
	('Washington',	'WA',	53),
	('West Virginia',	'WV',	54),
	('Wisconsin',	'WI',	55),
	('Wyoming',	'WY',	56),
]

ignored_fips = [
        11, # Washington D.C.
        60, # American Samoa
        66, # Guam
        69, # Northern Mariana Islands
        72, # Puerto Rico
        78, # Virgin Islands
]

def nameForPostalCode(code):
	"""Return proper name for two letter postal code."""
	code = code.upper()
	for x in states:
		if x[1] == code:
			return x[0]
	return None

def codeForState(stateName):
	"""Return two letter postal code for proper Name (case sensitive)."""
	snl = stateName.lower()
	for x in states:
		if x[0].lower() == snl:
			return x[1]
	return None

def stateAbbreviations():
	"""Generator over two letter codes."""
	for x in states:
		if (x[1] == 'DC') or (x[1] == 'PR'):
			continue
		yield x[1]

def fipsForPostalCode(code):
	"""Get numeric FIPS code for two letter postal code."""
	code = code.upper()
	for x in states:
		if x[1] == code:
			return x[2]
	return None

def nameForFips(fips):
	"""Get proper name for fips number."""
	for x in states:
		if x[2] == fips:
			return x[1]
	return None

def codeForFips(fips):
	"""Get two letter postal code for fips number."""
	for x in states:
		if x[2] == fips:
			return x[1]
	return None

_legpath = None

# dict from postal code to [(body name, body short name, body count), ...]
_legstats = None

class LegislatureStat(object):
	"""Basic info for a state legislature or congressional delegation."""
	
	def __init__(self, name, shortname, code, count):
		self.name = name
		self.shortname = shortname
		self.code = code
		self.count = count

	def __str__(self):
		return '%s "%s" (%s): %s' % (self.code, self.name, self.shortname, self.count)

	def __repr__(self):
		return 'LegislatureStat(%r, %r, %r, %r)' % (self.name, self.shortname, self.code, self.count)

def legislatureStatsForPostalCode(code):
	"""Return [LegislatureStat, ...].
	Returns None if code is bogus."""
	global _legpath
	global _legstats
	if not _legstats:
		_legstats = {}
		if not _legpath:
			_legpath = os.path.join(os.path.dirname(__file__), 'legislatures2010.csv')
		f = open(_legpath, 'r')
		for line in f:
			line = line.strip()
			if (not line) or (line[0] == '#'):
				continue
			(state, body, count) = line.split(',')
			count = int(count)
			bodyshort = body.split()[0]
			code = codeForState(state)
			ls = LegislatureStat(body, bodyshort, code, count)
			if code not in _legstats:
				_legstats[code] = [ls]
			else:
				_legstats[code].append(ls)
	return _legstats.get(code)

def expandLegName(leglist, name):
	for ls in leglist:
		if ls.shortname == name:
			return ls.name
	return name


def stateConfigToActual(stu, configname):
	"""Map ('MA','Senate') to 'sldu' (or 'cd' or 'sldl' as appropriate)"""
	if configname == 'Congress':
		return 'cd'
	ls = legislatureStatsForPostalCode(stu)
	if ls is None:
		return None
	minseats = None
	confseats = None
	for tls in ls:
		if tls.shortname == 'Congress':
			continue
		if (minseats is None) or (tls.count < minseats):
			minseats = tls.count
		if tls.shortname == configname:
			confseats = tls.count
	if confseats is None:
		raise Exception('unknown configuration %s,%s' % (stu, configname))
	if confseats == minseats:
		# the body with fewer seats is the 'upper' house of legislature
		# Also, Nebraska.
		return 'sldu'
	else:
		return 'sldl'


_projCsvPath = None
_projMap = None

def projectionForPostalCode(code):
        global _projCsvPath
        global _projMap
        if not _projMap:
                if not _projCsvPath:
                        _projCsvPath = os.path.join(os.path.dirname(__file__), 'projections.csv')
                _projMap = {}
                with open(_projCsvPath, 'r') as fin:
                        reader = csv.reader(fin)
                        for row in reader:
                                stl, datum_id = row
                                stl = stl.lower()
                                _projMap[stl] = datum_id
        return _projMap.get(code.lower())
