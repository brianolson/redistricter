#!/usr/bin/python

import optparse
import os
import re
import string
import sys

from states import *

stulist = list(stateAbbreviations())
stulist.sort()


partrow = string.Template("""<tr><td class="st">${stu}</td><td class="i"><a href="${stu}/${ba_large}"><img src="${stu}/${ba_small}" alt="$statename current and proposed districting"></a></td><td>Current: ${current_kmpp}<br />My Way: ${my_kmpp}</td></tr>\n""")

def build_stulist(resultdir):
	"""Return list of directories that have a 'link1' best result subdir."""
	out = []
	for rd in os.listdir(resultdir):
		if os.path.isdir(rd) and os.path.isdir(os.path.join(rd, 'link1')):
			out.append(x)
	out.sort()
	return out


def get_template(rootdir = None):
	"""Return string.Template object for a result page."""
	if rootdir is None:
		rootdir = os.path.dirname(os.path.abspath(__file__))
	f = open(os.path.join(rootdir, 'st_index_pyt.html'), 'r')
	index_template = string.Template(f.read())
	f.close()
	return index_template

#	return index_template.substitute({
#			'statename': statename,
#			'statenav': statenav,
#			'ba_large': ba_large,
#			'ba_small': ba_small,
#			'avgpop': avgpop,
#			'current_kmpp': current_kmpp,
#			'current_spread': current_spread,
#			'current_std': current_std,
#			'my_kmpp': my_kmpp,
#			'my_spread': my_spread,
#			'my_std': my_std,
#			'extra': extra,
#			'racedata': racedata,
#			})


def readfile(fname, mode='rb'):
	"""Simple wrapper to read a whole file."""
	f = open(fname, mode)
	raw = f.read()
	f.close()
	return raw


def navlist(stu, stulist, rooturl='/dist'):
	"""Return string of HTML navigating to everything in stulist except stu."""
	parts = []
	for x in stulist:
		if x == stu:
			parts.append('<b>' + x + '</b>')
		else:
			parts.append('<a href="' + rooturl + '/' + x + '/">' + x + '</a>')
	return ' '.join(parts)


statsum_pat = re.compile(r'Best Km/p: Km/p=([0-9.]+)\s+spread=([0-9.]+)\s+std=([0-9.]+)', re.MULTILINE|re.DOTALL)
# (kmpp, spread, std) = statsum_pat.search(statsum).groups()


startstats_kmpp_pat = re.compile(r'([0-9.]+)\s+Km/p', re.MULTILINE)
startstats_avg_pat = re.compile(r'avg=([0-9.]+)', re.MULTILINE)
startstats_min_pat = re.compile(r'min=([0-9.]+)', re.MULTILINE)
startstats_max_pat = re.compile(r'max=([0-9.]+)', re.MULTILINE)
startstats_std_pat = re.compile(r'std=([0-9.]+)', re.MULTILINE)
def parse_startstats(raw, desthash=None):
	kmpp = startstats_kmpp_pat.search(raw).group(1)
	_avg = startstats_avg_pat.search(raw).group(1)
	_min = startstats_min_pat.search(raw).group(1)
	_max = startstats_max_pat.search(raw).group(1)
	_std = startstats_std_pat.search(raw).group(1)
	if desthash is not None:
		desthash['avgpop'] = _avg
		desthash['current_kmpp'] = kmpp
		desthash['current_spread'] = int(_max) - int(_min)
		desthash['current_std'] = _std
	return (kmpp, _avg, _min, _max, _std)


def parse_statsum(statsum, desthash=None):
	(kmpp, spread, std) = statsum_pat.search(statsum).groups()
	if desthash is not None:
		desthash['my_kmpp'] = kmpp
		desthash['my_spread'] = spread
		desthash['my_std'] = std
	return (kmpp, spread, std)

putexclude = ['statlog.gz', 'g.tar.bz2']

def main(argv):
	op = optparse.OptionParser()
	op.add_option('--datadir', dest='datadir', default='data')
	op.add_option('--resultdir', dest='resultdir', default='.')
	op.add_option('--putlist', dest='putlist', default=None)
	op.add_option('--parthtml', dest='parthtml', default=None)
	op.add_option('--rooturl', dest='rooturl', default='/dist')
	op.add_option('--dry-run', dest='dryrun', action=store_true, default=False)
#	op.add_option('--bindir', dest='bindir', default='.')
	(options, args) = op.parse_args()
	dolist = []
	for arg in args:
		stu = arg.upper()
		if os.path.isdir(os.path.join(options.resultdir, stu)):
			dolist.append(stu)
		else:
			sys.stderr.write('bogus args: "%s"\n' % '", "'.join(args))
			sys.exit(1)
	stulist = build_stulist(options.resultdir)
	if not dolist:
		dolist = stulist
	index_template = get_template()
	putlist = None
	if options.putlist:
		putlist = open(options.putlist, 'w')
	parthtml = None
	if options.parthtml:
		parthtml = open(options.parthtml, 'w')
	for stu in dolist:
		statename = nameForPostalCode(stu)
		datadir = os.path.join(options.datadir, stu)
		resultdir = os.path.join(options.resultdir, stu)
		if os.path.isdir(datadir) and os.path.isdir(resultdir):
			link1path = os.path.join(resultdir, 'link1')
			outpath = os.path.join(resultdir, 'index.html')
			link1source = os.readlink(link1path)
			if putlist:
				link1abs = os.path.join(resultdir, link1source)
				for x in os.listdir(link1abs):
					if x not in putexclude:
						putlist.write(os.path.join(link1abs, x))
						putlist.write('\n')
				putlist.write(link1path + '\n')
				putlist.write(outpath + '\n')
			outhash = {
				'stu': stu,
				'statename': statename,
				'statenav': navlist(stu, stulist, options.rooturl),
				'ba_large': link1source + '/' + stu + '_ba.png',
				'ba_small': link1source + '/' + stu + '_ba_500.png',
				}
			startstats = readfile(os.path.join(datadir, stu + '_start_stats'))
			parse_startstats(startstats, outhash)
			statsum = readfile(os.path.join(link1path, 'statsum'))
			parse_statsum(statsum, outhash)
			if parthtml:
				parthtml.write(partrow.substitute(outhash))
			try:
				origracepath = os.path.join(datadir, 'race.html')
				myracepath = os.path.join(link1path, 'race.html')
				origrace = readfile(origracepath)
				myrace = readfile(myracepath)
				outhash['racedata'] = (
					'<h2>District demographics</h2><table class="ddemo">' +
					'<tr><th>Current</th><th>My Way</th></tr>' +
					'<tr><td class="ddtd">' + origrace +
					'</td><td class="ddtd">' + myrace + '</td></tr></table>\n')
			except:
				origrace = None
				myrace = None
				outhash['racedata'] = ''
			try:
				extrapath = os.path.join(resultdir, 'extra.html')
				extra = readfile(extrapath)
				outhash['extra'] = extra
			except:
				extra = None
				outhash['extra'] = ''
			out = open(outpath, 'wb')
			out.write(index_template.substitute(outhash))
			out.close()
	navall = open('.navall.html')
	navall.write(navlist(None, stulist, options.rooturl))
	navall.close()
	if options.putlist:
		print "rsync -a -v --rsh=ssh --files-from=%s . bolson\@bolson.org:/www/dist/n/" % options.putlist;

if __name__ == '__main__':
	main(sys.argv)
