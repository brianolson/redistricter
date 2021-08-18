#!/usr/bin/env python3
#
# send a work dir from runner

import base64
import gzip
import json
import os
import sys
import time
import urllib.request

def maybeFetch(url, path, max_age_seconds):
    if (not os.path.exists(path)) or ((time.time() - os.path.getmtime(path)) > max_age_seconds):
        print('{} -> {}'.format(url, path))
        urllib.request.urlretrieve(url, path)

def main():
    # handle args like runner.go
    pos = 1
    clientDir = None
    workDir = None
    configUrl = None
    limit = None

    while pos < len(sys.argv):
        arg = sys.argv[pos]
        if arg == '-dir':
            pos += 1
            clientDir = sys.argv[pos]
        elif arg == '-work':
            pos += 1
            workDir = sys.argv[pos]
        elif arg == '-url':
            pos += 1
            configUrl = sys.argv[pos]
        elif arg == '-limit':
            pos += 1
            limit = int(sys.argv[pos])
        else:
            sys.stderr.write('unknown arg {!r}\n'.formatarg)
            sys.exit(1)
            return
        pos += 1

    if workDir is None:
        workDir = os.path.join(clientDir, "work")

    serverConfigPath = os.path.join(workDir, "server_config.json")
    maybeFetch(configUrl, serverConfigPath, 23*3600)
    with open(serverConfigPath) as fin:
        serverConfig = json.load(fin)

    posturl = serverConfig.get('post')
    if not posturl:
        sys.stderr.write('no post url configured\n')
        sys.exit(1)
        return

    count = 0
    with open(os.path.join(workDir, 'bestdb')) as fin:
        for line in fin:
            count += 1
            if (limit is not None) and (count > limit):
                break
            rec = json.loads(line)
            rundir = rec.pop('d')
            havebest = False
            havestatlog = False
            if rec['ok']:
                bestpath = os.path.join(rundir, 'bestKmpp.dsz')
                try:
                    with open(bestpath, 'rb') as fin:
                        rec['bestKmpp.dsz'] = base64.b64encode(fin.read()).decode()
                        havebest = True
                except Exception as e:
                    print('{}: could not read solution, {}'.format(bestpath, e))
            if not havebest:
                statlogpath = os.path.join(rundir, 'statlog.gz')
                try:
                    with gzip.open(statlogpath, 'rt') as fin:
                        lines = [x for x in fin]
                        if len(lines) > 50:
                            lines = lines[-50:]
                        rec['statsum'] = '\n'.join(lines)
                        havestatlog = True
                except Exception as e:
                    print('{}: could not read statlog, {}'.format(bestpath, e))
            if (not havestatlog) and (not havebest):
                print('{}: not enough to report, skipping'.format(rundir))
            req = urllib.request.Request(posturl, data=json.dumps(rec).encode(), headers={'Content-Type':'application/json'}, method='POST')
            try:
                with urllib.request.urlopen(req) as result:
                    raw = result.read()
                    print('({}) {} -> {}: {}'.format(result.status, rundir, posturl, raw))
            except Exception as e:
                print('() {} -> {}'.format(rundir, posturl))
                print(dir(e))
                print(e.fp.read())
                raise




if __name__ == '__main__':
    main()
