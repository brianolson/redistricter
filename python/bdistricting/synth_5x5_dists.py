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

map6 = '''1 1 1 2 3
4 4 1 2 3
4 4 1 2 3
4 5 2 2 3
5 5 5 5 3'''

maps_sources = [map1, map2, map3, map4, map5, map6]

maps_source_splitter = re.compile(r'[ \n]+')

def compile_map(mapstr):
    return list(map(int, maps_source_splitter.split(mapstr)))

maps = list(map(compile_map, maps_sources))

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
    for dist, distp in distcount.items():
        ec = elem_count(distp)
        #print('{}\t{}'.format(dist, ec))
        wincount, winner = ec[0]
        winners[winner] = winners.get(winner, 0) + 1
    return winners

def randomPopulation():
    population = ['x']*15
    population += ['o']*10

    random.shuffle(population)
    return population

def trial():
    population = randomPopulation()
    print(''.join(population))
    for map in maps:
        winners = count(population, map)
        wc = sorted(winners.items())
        print(wc)

def map_show_vertical_line(map, lx, ly):
    # the vertical line at (lx,ly) is between (lx-1,y) and (lx,ly)
    if not map:
        return False
    return map[(ly * 5) + lx] != map[(ly * 5) + lx - 1]

def map_show_horizontal_line(map, lx, ly):
    # the horizontal line at (lx,ly) is between (lx,y) and (lx,ly-1)
    if not map:
        return False
    return map[(ly * 5) + lx] != map[((ly - 1) * 5) + lx]

def svg(popstr, out=None, lines=True, map=None):
    r = 80
    inset = 400
    minx = inset
    maxx = 2160-inset
    miny = inset
    maxy = 2160-inset
    dx = (maxx-minx)/4.0
    dy = (maxy-miny)/4.0
    # map = maps[2]
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
        ls = 1
        hx = dx / 2.0
        hy = dy / 2.0
        parts.append('<g stroke="#ffffff" stroke-width="9">')
        parts.append('<rect x="{}" y="{}" width="{}" height="{}" fill-opacity="0"/>'.format(minx - hx, miny - hy, dx * 5, dy * 5))
        for x in range(1, 5):
            for y in range(0, 5):
                if not map_show_vertical_line(map, x, y):
                    continue
                cx = minx + (dx * x)
                cy = miny + (dy * y)
                parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}" id="lsv{}_{}"/>'.format(cx-hx, cx-hx, cy-hy, cy+hy, x, y))
                ls += 1
        for y in range(1, 5):
            for x in range(0, 5):
                if not map_show_horizontal_line(map, x, y):
                    continue
                cx = minx + (dx * x)
                cy = miny + (dy * y)
                parts.append('<line x1="{}" x2="{}" y1="{}" y2="{}" id="lsh{}_{}"/>'.format(cx-hx, cx+hx, cy-hy, cy-hy, x, y))
                ls += 1

        parts.append('</g>')
    if map:
        winners = sorted(count(popstr, map).items())
    else:
        winners = {}
        for c in popstr:
            winners[c] = winners.get(c, 0) + 1
        winners = sorted(winners.items())
        winners = [(c, '{} ({}%)'.format(cc, (100 * cc) / len(popstr))) for c, cc in winners]
    if winners:
        oy = 1
        for c, cc in winners:
            color = colors[c]
            cx = minx + (dx * 6.2)
            cy = miny + (dy * oy)
            parts.append('<circle cx="{}" cy="{}" r="{}" style="fill:{}"/>'.format(cx, cy, r, color))
            parts.append('<text x="{}" y="{}" stroke="#ffffff" fill="#ffffff" font-size="{}" alignment-baseline="middle">{}</text>'.format(minx + (dx * 6.7), cy, r * 2, cc))
            oy += 1
    parts.append('</svg>')
    if out is None:
        out = sys.stdout
    out.write('\n'.join(parts))

if __name__ == '__main__':
    #popstr = 'ooxoxoxoxoxxxxxxooxoxxxxo'
    # oooxoxxxxxoxoxoxoxoxxxxxo
    # oooxoxxxxxxxxooxxoxoxxoox
    # ooxxooxxoooxxxxxxoxxooxxx
    if len(sys.argv) > 1:
        popstr = sys.argv[1]
    else:
        popstr = randomPopulation()
    print(''.join(popstr))
    
    svg(popstr, open('/tmp/a.svg','w'))
    for mi, map in enumerate(maps):
        svg(popstr, open('/tmp/m{}.svg'.format(mi),'w'), map=map)
    #trial()

