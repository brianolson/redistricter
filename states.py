#!/usr/bin/python

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
	('Washington D.C.',	'DC',	11),
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

def nameForPostalCode(code):
	"""Return proper name for two letter postal code."""
	code = code.upper()
	for x in states:
		if x[1] == code:
			return x[0]
	return None

def codeForState(stateName):
	"""Return two letter postal code for proper Name (case sensitive)."""
	for x in states:
		if x[0] == stateName:
			return x[1]
	return None

def stateAbbreviations():
	"""Generator over two letter codes."""
	for x in states:
		if x[1] == 'DC':
			continue
		yield x[1]

def fipsForPostalCode(code):
	"""Get numeric FIPS code for two letter postal code."""
	code = code.upper()
	for x in states:
		if x[1] == code:
			return x[2]
	return None

def codeForFips(fips):
	"""Get two letter postal code for fips number."""
	for x in states:
		if x[2] == fips:
			return x[1]
	return None
