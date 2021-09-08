#!/usr/bin/env python3
#
# Check that I understand the Census race data.
# For each block,
#  assert that:
#   (total population) >= (people of 1 race + people of 2 or more races) >= (sum(people of x races for x in range(1,7)))
#   (people of 1 race) >= (sum of single race reports)
import csv
import os
import re
import sys
import zipfile

geocols = {}
with open('2020_GEO_fields.csv') as fin:
    reader = csv.reader(fin)
    header = None
    for row in reader:
        if header is None:
            header = row
            continue
        geocols[row[1]] = dict(zip(header,row))

bydname = {}
with open('2020_PL1_fields.csv') as fin:
    header = None
    colindexes = None
    reader = csv.reader(fin)
    dcol = None
    for row in reader:
        if header is None:
            header = row
            colindexes = dict([(v,i) for i,v in enumerate(header)])
            dcol = colindexes['DATA DICTIONARY REFERENCE NAME']
            continue
        dname = row[dcol]
        bydname[dname] = dict(zip(header,row))


geo_re = re.compile(r'..geo2020.pl')
pl1_re = re.compile(r'..000012020.pl')

def get_geo_block_logrecno(geoblob):
    reader = csv.reader(geoblob.splitlines(), delimiter='|')
    sumlevcol = int(geocols['SUMLEV']['COLNUM'])
    geologrecnocol = int(geocols['LOGRECNO']['COLNUM'])
    block_logrecnos = []
    #sumlevset = set()
    for row in reader:
        sumlev = row[sumlevcol]
        #sumlevset.add(sumlev)
        if sumlev != '750':
            continue
        logrecno = row[geologrecnocol]
        block_logrecnos.append(logrecno)
    return block_logrecnos

def check_pl1(pl1blob, block_logrecnos):
    blset = set(block_logrecnos)
    reader = csv.reader(pl1blob.splitlines(), delimiter='|')
    tcol = int(bydname['P0010001']['COLNUM'])
    r1col = int(bydname['P0010002']['COLNUM'])

    whitecol = int(bydname['P0010003']['COLNUM'])
    blackcol = int(bydname['P0010004']['COLNUM'])
    aiancol = int(bydname['P0010005']['COLNUM']) # American Indian/Alaska Native
    asiancol = int(bydname['P0010006']['COLNUM'])
    pacificcol = int(bydname['P0010007']['COLNUM']) # Native Hawaiian and Other Pacific Islander alone
    othercol = int(bydname['P0010008']['COLNUM'])
    r2pcol = int(bydname['P0010009']['COLNUM']) # two or more races

    r2col = int(bydname['P0010010']['COLNUM'])
    r3col = int(bydname['P0010026']['COLNUM'])
    r4col = int(bydname['P0010047']['COLNUM'])
    r5col = int(bydname['P0010063']['COLNUM'])
    r6col = int(bydname['P0010070']['COLNUM'])
    lrcol = int(bydname['LOGRECNO']['COLNUM'])

    errcount = 0
    lineno = 0
    ok = 0
    for row in reader:
        lineno += 1
        logrecno = row[lrcol]
        if logrecno not in blset:
            continue
        total = int(row[tcol])
        r1 = int(row[r1col])
        white = int(row[whitecol])
        black = int(row[blackcol])
        aian = int(row[aiancol])
        asian = int(row[asiancol])
        pacific = int(row[pacificcol])
        other = int(row[othercol])
        r2p = int(row[r2pcol])
        r2 = int(row[r2col])
        r3 = int(row[r3col])
        r4 = int(row[r4col])
        r5 = int(row[r5col])
        r6 = int(row[r6col])
        r1t6 = r1+r2+r3+r4+r5+r6
        r1a2p = r1 + r2p
        r1byParts = white + black + aian + asian + pacific + other
        if (total < r1a2p) or (r1a2p < r1t6) or (total < r1t6) or (r1 < r1byParts):
            errcount += 1
            print(':{} sum(races 1..6 = {}, 1+(2+) = {}, one = {}, oneBP = {}, total = {})'.format(lineno, (r1+r2+r3+r4+r5+r6), r1+r2p, r1, r1byParts, total))
            if errcount > 10:
                break
        ok += 1
    print('{} ok'.format(ok))

for arg in sys.argv:
    if not arg.endswith('.zip'):
        continue
    if not os.path.exists(arg):
        sys.stderr.write('{!r}: not a file, skipping\n'.format(arg))
        continue
    print(arg)
    zf = zipfile.ZipFile(arg, 'r')
    paths = zf.namelist()
    for path in paths:
        if geo_re.match(path):
            geoblob = zf.read(path).decode('iso-8859-1')
            block_logrecnos = get_geo_block_logrecno(geoblob)
    for path in paths:
        if pl1_re.match(path):
            pl1blob = zf.read(path).decode('iso-8859-1')
            check_pl1(pl1blob, block_logrecnos)
