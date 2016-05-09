#!/usr/bin/env python
#
# create 5x5 populations that split 60/40 (15/10) and then split into
# 5 districts.  find a population that can be cut by districts into
# 2,3,4 seats won by the 40% party?


import random
import re


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
        print('{}\t{}'.format(dist, ec))
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

if __name__ == '__main__':
    trial()

