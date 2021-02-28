#!/usr/bin/python

"""
Reads legislatures.csv and writes data/??/config/*
"""

import csv
import json
import logging
import os
import sys

from states import codeForState

logger = logging.getLogger(__name__)

def run(datadir='data', stulist=None, dryrun=False, newerthan=None, csvpath=None):
        if csvpath is None:
                csvpath = os.path.join(os.path.dirname(__file__), 'legislatures2010.csv')
    congress_total = 0
    # csv is
    # State Name, Legislative Body Name, Seats
    f = open(csvpath, 'rt')
    fc = csv.reader(f)
    for row in fc:
        stu = codeForState(row[0])
        if (stulist is not None) and (stu not in stulist):
            logger.error('%s: unknown state %r', csvpath, row[0])
            continue
        configdir = os.path.join(datadir, stu, 'config')
        if not os.path.isdir(configdir):
            os.makedirs(configdir)
        name_part = row[1].split()[0]
        if name_part == 'Congress':
            congress_total += int(row[2])
        outname = os.path.join(configdir, name_part)
        if newerthan and not newerthan(csvpath, outname):
            if dryrun:
                print('"%s" up to date' % (outname))
            continue
        if dryrun:
            print('would write "%s"' % (outname))
        else:
            with open(outname, 'w') as out:
                out.write("""datadir: $DATA/%s\ncommon: -d %s\n""" % (stu, row[2]))
                if (row[2] == '1') or (row[2] == 1):
                    out.write('disabled\n')
            with open(outname + '.json', 'w') as out:
                ob = {
                    'common':{'kwargs':{'-d':row[2]}}
                }
                json.dump(ob, out)
    return congress_total

if __name__ == '__main__':
    congress_total = run(dryrun=True)
    print('congress_total == %s, should be 435' % congress_total)
