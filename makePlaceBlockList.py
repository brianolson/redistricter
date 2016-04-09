#!/usr/bin/env python

import argparse
import csv
import gzip
import logging
import struct
import sys


logger = logging.getLogger(__name__)

# read the whole CSV file, sort by population descending
def readPlacePops(path):
    popsortable = []
    with open(path) as fin:
        reader = csv.reader(fin)
        for row in reader:
            pop = int(row[0])
            place = row[1]
            name = row[2]
            popsortable.append( (pop, place, name) )
    popsortable.sort(reverse=True)
    return popsortable


def getTopNPlaceCodes(placePopsPath, N=5):
    return map(lambda x: x[1], readPlacePops(placePopsPath)[:N])


def main():
    ap = argparse.ArgumentParser(description='Create a file of gziped uint64 ubid numbers from a place or list of places')
    ap.add_argument('place', nargs='*', help='place numbers to emit ubids for')
    ap.add_argument('-n', type=int, default=5, metavar='N', help='return top N populous places from placepops CSV file')
    ap.add_argument('--placepops', help='use the top N places from this CSV of populations and places')
    ap.add_argument('--places', required=True, help='geoblocks.places (ubid uint64, place uint64) for some state')
    ap.add_argument('--verbose', action='store_true', default=False)
    ap.add_argument('--out', required=True, help='path to write gzipped uint64 ubid list to')
    args = ap.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)

    if not args.place:
        if not args.placepops:
            logger.error('need places on command line or --placepops path')
            return 1
        #args.place = map(lambda x: x[1], readPlacePops(args.placepops)[:5])
        args.place = getTopNPlaceCodes(args.placepops, args.n)
        #logger.debug('places: %r', args.place)
    places = map(int, args.place)
    logger.debug('places: %r', places)

    count = filterPlacesToUbidList(args.places, places, args.out)
    sys.stderr.write('wrote {} ubids\n'.format(count))
    return 0


def filterPlacesToUbidList(placesPath, places, outPath):
    ubids = getPlacesUbids(placesPath, places)
    ubids.sort()

    writeUbidList(outPath, ubids)
    return len(ubids)


def getPlacesUbids(placesPath, places):
    "read geoblocks.places file, filter on set or list of int places, return list of ubids"
    ubids = []
    with gzip.open(placesPath, 'rb') as fin:
        header = fin.read(16)
        version, length = struct.unpack('=QQ', header)
        for _ in xrange(length):
            buf = fin.read(16)
            place_ubid, place = struct.unpack('=QQ', buf)
            if place in places:
                ubids.append(place_ubid)
    return ubids


def writeUbidList(outPath, ubids):
    with gzip.open(outPath, 'wb') as fout:
        # Write (version uint64, len uint64) header
        fout.write(struct.pack('=QQ', 1, len(ubids)))
        for u in ubids:
            fout.write(struct.pack('=Q', u))


if __name__ == '__main__':
    sys.exit(main())
