#!/usr/bin/env python3
#
# Tool for managing server.json served by receiver_wsgi.py

import glob
import json
import os

def dpathset(d, path, value):
    if len(path) == 1:
        d[path[0]] = value
    else:
        if d.get(path[0]) is None:
            d[path[0]] = dict()
        dpathset(d[path[0]], path[1:], value)

def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--datasets', help='directory full of XX_runfiles.tar.gz')
    ap.add_argument('--overlays', help='file with lines like: c.VT_House.disabled=true')
    ap.add_argument('-i', '--in', dest='inpath', required=True)
    ap.add_argument('-o', '--out', dest='outpath', required=True)
    ap.add_argument('--prefix', default='https://bdistricting.com/2020/datasets/')
    args = ap.parse_args()
    prefix = args.prefix
    if not prefix.endswith('/'):
        prefix = prefix + '/'

    with open(args.inpath) as fin:
        config = json.load(fin)

    if args.datasets:
        stateDataUrls = {}
        for path in glob.glob(os.path.join(args.datasets, '*_runfiles.tar.gz')):
            fname = os.path.basename(path)
            stu = fname[:2]
            stateDataUrls[stu] = prefix + fname
        config['durls'] = stateDataUrls

    if args.overlays:
        with open(args.overlays) as fin:
            for line in fin:
                if not line:
                    continue
                line = line.strip()
                if not line:
                    continue
                if line[0] == '#':
                    continue
                path,val = line.split('=')
                path = path.split('.')
                dpathset(config, path, value)

    with open(args.outpath, 'wt') as fout:
        json.dump(config, fout)

if __name__ == '__main__':
    main()
