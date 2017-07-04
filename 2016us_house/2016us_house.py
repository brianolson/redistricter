#!/usr/bin/env python3
# pip install odfpy
#
# emits:
# '2016_us_house_seats.csv'
#  broken down by each state, how many votes and seats per party
# '2016_us_house_all.csv'
#  row for each race, votes per party

import csv
import io
import logging

logging.basicConfig(level=logging.INFO)
logger=logging.getLogger(__name__)

from odf.opendocument import OpenDocumentSpreadsheet
from odf.opendocument import load


def attrAnyNS(elem, attrName):
    for k, v in elem.attributes.items():
        kns, kname = k
        if kname == attrName:
            return v
    return None

def toxml(elem):
    f = io.StringIO()
    elem.toXml(0, f)
    return f.getvalue()

def childNodeText(elem):
    if elem.nodeType == elem.TEXT_NODE:
        return elem.data
    parts = None
    for x in elem.childNodes:
        v = childNodeText(x)
        if v is not None:
            if parts is None:
                parts = [v]
            else:
                parts.append(v)
    if parts is None:
        return None
    return ''.join(parts)
    #return ''.join([childNodeText(x) for x in elem.childNodes])

def cellValue(elem):
    valueType = attrAnyNS(elem, 'value-type')
    text = childNodeText(elem)
    if valueType == 'float':
        return float(text.replace(',','').strip())
    return text

def sint(x):
    if x is None:
        return 0
    if x == '':
        return 0
    if isinstance(x, float):
        return x
    if isinstance(x, int):
        return x
    return int(x.replace(',','').strip())

def vecaccum(a,b):
    for i, v in enumerate(b):
        try:
            if v:
                a[i] += sint(v)
        except Exception as e:
            logger.error("(a[%s] = %s) += %r", i, a[i], v, exc_info=True)

def wat():
    #if valueType != 'string':
    #    logger.warn("cell type %r: %r", valueType, toxml(elem))
    if elem.childNodes:
        if len(elem.childNodes) != 1:
            logger.warn("long cell: %r", elem.childNodes)
        textp = elem.childNodes[0]
        if textp.tagName == 'text:p':
            if textp.childNodes:
                if len(textp.childNodes) != 1:
                    logger.warn("long textp: %r", textp.childNodes)
                tn = textp.childNodes[0]
                if tn.nodeType != tn.TEXT_NODE:
                    logger.warn("text:p wat? : %r", toxml(textp))
                    return None
                if valueType == 'float':
                    return float(textp.childNodes[0].data.replace(',','').strip())
                return textp.childNodes[0].data
        else:
            logger.warn("cell contents not text:p but %r", textp.tagName)
    return None


doc = load('20161108 election results data/2016 US House.ods')

sheets = list(filter(lambda x: x.tagName == 'table:table', doc.spreadsheet.childNodes))

unopposed = None
stateSheets = {}
for sheet in sheets:
    name = sheet.getAttribute('name')
    if name == 'unopposed':
        unopposed = sheet
        continue
    stateSheets[name] = sheet



def sheetToYX(state):
    yx = {}
    col = 0
    row = 0
    for part in state.childNodes:
        if part.tagName == 'table:table-column':
            colRow = 0
            for colPart in part.childNodes:
                if colPart.tagName == 'table:table-cell':
                    value = cellValue(colPart)
                    if value:
                        yx[(colRow, col)] = value
                    colRow += 1
            col += 1
            continue
        if part.tagName == 'table:table-row':
            rowCol = 0
            for rowPart in part.childNodes:
                if rowPart.tagName == 'table:table-cell':
                    value = cellValue(rowPart)
                    if value:
                        yx[(row, rowCol)] = value
                    rowCol += 1
                    repeat = attrAnyNS(rowPart,'number-columns-repeated')
                    if repeat is not None:
                        repeat = int(repeat) - 1
                        while repeat > 0:
                            if value:
                                yx[(row, rowCol)] = value
                            rowCol += 1
                            repeat -= 1
            row += 1
            continue
    return yx


HEADER = ['State', 'Dist', 'D', 'R', 'G', 'L', 'other', 'notes']

fseats = open('2016_us_house_seats.csv', 'w')
wseats = csv.writer(fseats)
wseats.writerow(['State', 'D', 'R', 'G', 'L', 'other', 'D', 'R', 'G', 'L', 'other'])

fout = open('2016_us_house_all.csv', 'w')
writer = csv.writer(fout)
writer.writerow(HEADER)

seatsum = [0,0,0,0,0]
votesum = [0,0,0,0,0]

minwin = None
maxwin = None
leastvotes = None
maxvotes = None
votesForWinners = 0
votesForOthers = 0

#for stu, state in stateSheets.items():
for stu in sorted(stateSheets.keys()):
    state = stateSheets[stu]
    #logger.info("%s", stu)
    yx = sheetToYX(state)
    maxy = max([c[0] for c in yx.keys()])
    maxx = max([c[1] for c in yx.keys()])
    header = [yx[(0,x)] for x in range(0, maxx+1)]
    seats = [yx.get((2, x), '') for x in range(2, 7)]
    statevotes = [0,0,0,0,0]
    vecaccum(seatsum, seats)
    if header != HEADER:
        logger.error("state %s bad header, got %r wanted %r", stu, header, HEADER)
        continue
    for y in range(3, maxy+1):
        row = [yx.get((y, x), '') for x in range(0, maxx+1)]
        row[1] = int(sint(row[1]))
        votes = list(map(sint, row[2:7]))
        #logger.info("row %r -> votes %r", row[2:7], votes)
        vmax = max(votes)
        if ((minwin is None) or (minwin[0] > vmax)) and (vmax > 1):
            minwin = (vmax, row[0], row[1])
        if (maxwin is None) or (maxwin[0] < vmax):
            maxwin = (vmax, row[0], row[1])
        vsum = sum(votes)
        if ((leastvotes is None) or (leastvotes[0] > vsum)) and (vsum > 1):
            leastvotes = (vsum, row[0], row[1])
        if (maxvotes is None) or (maxvotes[0] < vsum):
            maxvotes = (vsum, row[0], row[1])
        votesForWinners += vmax
        votesForOthers += (vsum - vmax)
        vecaccum(votesum, votes)
        vecaccum(statevotes, votes)
        writer.writerow(row)
    wseats.writerow([yx[(2,0)]] + statevotes + seats)

wseats.writerow(['sums']+votesum+seatsum)
wseats.writerow(['total', sum(votesum), '', '', '', '', sum(seatsum)])

writer.writerow(['','sums']+votesum)
writer.writerow(['','total',sum(votesum)])
writer.writerow(['smallest winner', minwin[1], minwin[2], minwin[0]])
writer.writerow(['biggest winner', maxwin[1], maxwin[2], maxwin[0]])
if leastvotes:
    writer.writerow(['smallest election', leastvotes[1], leastvotes[2], leastvotes[0]])
writer.writerow(['biggest election', maxvotes[1], maxvotes[2], maxvotes[0]])
writer.writerow(['votes for winners', votesForWinners])
writer.writerow(['votes for others', votesForOthers])
fout.close()
fseats.close()
