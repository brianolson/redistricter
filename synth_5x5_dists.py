#!/usr/bin/env python
#
# create 5x5 populations that split 60/40 (15/10) and then split into
# 5 districts.  find a population that can be cut by districts into
# 2,3,4 seats won by the 40% party?


import random
import re
import sys


# five horizontal line districts
map1 = '''1 1 1 1 1
2 2 2 2 2
3 3 3 3 3
4 4 4 4 4
5 5 5 5 5'''

# five vertical line districts
map2 = '''1 2 3 4 5
1 2 3 4 5
1 2 3 4 5
1 2 3 4 5
1 2 3 4 5'''

map3 = '''1 1 1 2 2
1 1 2 2 2
3 3 4 5 5
3 3 4 5 5
3 4 4 4 5'''

map4 = '''1 1 1 2 2
1 1 2 2 2
3 3 3 4 4
3 3 4 4 4
5 5 5 5 5'''

map5 = '''1 1 1 2 2
1 1 2 2 2
3 3 3 4 4
3 3 5 4 4
5 5 5 5 4'''

maps_sources = [map1, map2, map3, map4, map5]

maps_source_splitter = re.compile(r'[ \n]+')

def compile_map(mapstr):
    return map(int, maps_source_splitter.split(mapstr))

maps = map(compile_map, maps_sources)

def elem_count(they):
    out = {}
    for x in they:
        out[x] = out.get(x, 0) + 1
    outl = [(count,v) for v,count in out.items()]
    return sorted(outl, reverse=True)

def count(pop, map):
    distcount = {}
    for party, dist in zip(pop, map):
        distcount[dist] = distcount.get(dist, []) + [party]
    winners = {}
    for dist, distp in distcount.iteritems():
        ec = elem_count(distp)
        #print('{}\t{}'.format(dist, ec))
        wincount, winner = ec[0]
        winners[winner] = winners.get(winner, 0) + 1
    return winners

def trial():
    population = ['x']*15
    population += ['o']*10

    random.shuffle(population)
    print(''.join(population))
    for map in maps:
        winners = count(population, map)
        wc = sorted(winners.items())
        print(wc)

def svg(popstr, out=None, lines=True):
    r = 80
    inset = 400
    minx = inset
    maxx = 2160-inset
    miny = inset
    maxy = 2160-inset
    dx = (maxx-minx)/4.0
    dy = (maxy-miny)/4.0
    colors = {'x': '#009000', 'o': '#900090'}
    parts = ['''<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg
   width="3840"
   height="2160"
   id="svg2"
   xmlns="http://www.w3.org/2000/svg" version="1.1">''']
    parts.append('<!-- {} -->'.format(popstr))
    parts.append('''<rect x="0" y="0" width="3840" height="2160" fill="#000000"/>''')
    x = 0
    y = 0
    for c in popstr:
        color = colors[c]
        cx = minx + (dx * x)
        cy = miny + (dy * y)
        parts.append('<circle cx="{}" cy="{}" r="{}" style="fill:{}"/>'.format(cx, cy, r, color))
        x += 1
        if x == 5:
            y += 1
            x = 0
    if lines:
        hx = dx / 2.0
        hy = dy / 2.0
        parts.append('<g stroke="#ffffff" stroke-width="9">')
        for x in xrange(0, 5):
            for y in xrange(0, 5):
                cx = minx + (dx * x)
                cy = miny + (dy * y)
                parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}"/>'.format(cx-hx, cx+hx, cy-hy, cy-hy))
                parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}"/>'.format(cx-hx, cx-hx, cy-hy, cy+hy))
            y = 5
            cx = minx + (dx * x)
            cy = miny + (dy * y)
            parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}"/>'.format(cx-hx, cx+hx, cy-hy, cy-hy))
            #parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}"/>'.format(cx-hx, cx-hx, cy-hy, cy+hy))

        x = 5
        cx = minx + (dx * x)
        for y in xrange(0, 5):
            cy = miny + (dy * y)
            #parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}"/>'.format(cx-hx, cx+hx, cy-hy, cy-hy))
            parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}"/>'.format(cx-hx, cx-hx, cy-hy, cy+hy))

        parts.append('</g>')
    parts.append('</svg>')
    if out is None:
        out = sys.stdout
    out.write('\n'.join(parts))

if __name__ == '__main__':
    svg('ooxoxoxoxoxxxxxxooxoxxxxo',open('/var/www/html/b/a.svg','w'))
    #trial()

