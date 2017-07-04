#!/usr/bin/env python3

import csv
import sys

def sint(x):
    if x == '':
        return 0
    x = x.replace('.0', '')
    return int(x)

def plotStateSeatsVotes(they):
    width = 1920
    height = 1080
    insetx = 30
    insety = 30
    dw = width - (insetx * 2)
    dh = height - (insety * 2)
    maxSeats = max([x[1] for x in they])
    maxVotes = max([x[2] for x in they])
    parts = ['''<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg">
<rect x="0" y="0" width="{width}" height="{height}" fill="none" stroke="grey" stroke-width="1" />
<g font-size="20">\n'''.format(width=width, height=height)]
    for state, seats, votes in they:
        x = (votes * dw / maxVotes) + insetx
        y = (dh - (seats * dh / maxSeats)) + insety
        parts.append('<text x="%f" y="%f" text-anchor="left">%s</text>\n' % (x, y, state))
    parts.append('</g></svg>\n')
    return ''.join(parts)

def plotStateSeatsVotesBoth(they):
    width = 1920
    height = 1080
    insetx = 60
    insety = 40
    dw = width - (insetx * 2)
    dh = height - (insety * 2)
    maxSeats = max([max(x[1], x[3]) for x in they])
    maxVotes = max([max(x[2], x[4]) for x in they])
    parts = ['''<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg">
<rect x="0" y="0" width="{width}" height="{height}" fill="none" stroke="grey" stroke-width="1" />
<g font-size="18">\n'''.format(width=width, height=height)]
    dseatsum = 0
    dvotesum = 0
    for row in they:
        state = row[0]
        seats = row[1]
        votes = row[2]
        dseatsum += seats
        dvotesum += votes
        x = (votes * dw / maxVotes) + insetx
        y = (dh - (seats * dh / maxSeats)) + insety
        parts.append('<text x="%f" y="%f" text-anchor="middle" stroke-color="#000099" fill="#000099">%s D</text>\n' % (x, y, state))
    rseatsum = 0
    rvotesum = 0
    for row in they:
        state = row[0]
        seats = row[3]
        votes = row[4]
        rseatsum += seats
        rvotesum += votes
        x = (votes * dw / maxVotes) + insetx
        y = (dh - (seats * dh / maxSeats)) + insety
        parts.append('<text x="%f" y="%f" text-anchor="middle" stroke-color="#990000" fill="#990000">%s R</text>\n' % (x, y, state))
    parts.append('</g>\n')
    parts.append('<line x1="{}" y1="{}" x2="{}" y2="{}" stroke="#000099" stroke-width="1" />\n'.format(insetx, dh + insety, dw + insetx, insety + dh - (dh * maxVotes * (dseatsum / dvotesum) / maxSeats)))
    parts.append('<line x1="{}" y1="{}" x2="{}" y2="{}" stroke="#990000" stroke-width="1" />\n'.format(insetx, dh + insety, dw + insetx, insety + dh - (dh * maxVotes * (rseatsum / rvotesum) / maxSeats)))
    allseats = rseatsum + dseatsum
    allvotes = dvotesum + rvotesum
    parts.append('<line x1="{}" y1="{}" x2="{}" y2="{}" stroke="#888888" stroke-width="1" />\n'.format(insetx, dh + insety, dw + insetx, insety + dh - (dh * maxVotes * (allseats / allvotes) / maxSeats)))
    parts.append('</svg>\n')
    return ''.join(parts)

D = []
R = []
DR = []

fin = open('2016_us_house_seats.csv', 'r')
reader = csv.reader(fin)
header = next(reader)
for row in reader:
    state = row[0]
    if state == 'sums':
        break
    votes = list(map(sint, row[1:6]))
    seats = list(map(sint, row[6:11]))
    #print("{} {} {}".format(state, seats, votes))
    # but we're only going to graph D and R seats and votes
    votesum = sum(votes)
    D.append( (state, seats[0], votes[0]) )
    R.append( (state, seats[1], votes[1]) )
    DR.append( (state, seats[0], votes[0], seats[1], votes[1]) )

with open('2016_seats_votes.svg', 'w') as fout:
    fout.write(plotStateSeatsVotesBoth(DR))

sys.exit(0)
with open('2016_D_seats_votes.svg', 'w') as fout:
    fout.write(plotStateSeatsVotes(D))

with open('2016_R_seats_votes.svg', 'w') as fout:
    fout.write(plotStateSeatsVotes(R))
