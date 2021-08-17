#!/usr/bin/python
#
# WSGI (PEP 0333) based upload receiver
#
# usage:
#  REDISTRICTER_SOLUTIONS=/tmp/solutions gunicorn --bind=127.0.0.1:7319 bdistricting.receiver_wsgi:application

import cgi
import cgitb
from io import StringIO
import json
import glob
import logging
import os
import random
import sys
import tarfile
import time


logger = logging.getLogger(__name__)

if (os.getenv('SERVER_SOFTWARE') or '').startswith('gunicorn') and (__name__ != '__main__'):
    logger = logging.getLogger('gunicorn.error')


# variables read from the environment
kSoldirEnvName = 'REDISTRICTER_SOLUTIONS'


# random selection from these makes event id
SUFFIX_LETTERS_ = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0213456789'

rand = random.Random()

server_json_path = os.path.join(os.environ[kSoldirEnvName], 'server.json')

def serverConfig():
    # TODO: caching? just let the filesystem do it and assume everything is fast-enough?
    with open(server_json_path) as fin:
        return json.load(fin)


def makeEventId(remote_addr):
    "return e.g. 20161110/221815_72.74.165.240_Uuc"
    nowstr = time.strftime('%Y%m%d/%H%M%S', time.gmtime())
    outparts = [nowstr, '_']
    if remote_addr:
        outparts.append(str(remote_addr))
        outparts.append('_')
    outparts.append(rand.choice(SUFFIX_LETTERS_))
    outparts.append(rand.choice(SUFFIX_LETTERS_))
    outparts.append(rand.choice(SUFFIX_LETTERS_))
    return ''.join(outparts)


def paramToFile(name, var, outdir):
    if not var:
        return
    fout = open(os.path.join(outdir, name), 'wb')
    if type(var) >= str:
        fout.write(var)
    elif hasattr(var, 'file'):
        d = var.file.read(1000000)
        while len(d) > 0:
            fout.write(d)
            d = var.file.read(1000000)
    else:
        fout.write(var.value)
    fout.close()


def falseOrLen(x):
    if x is None:
        return 'None'
    return str(len(x))


def stringTruth(x):
    """Values 'f' 'false' '0' '' None (case independent) are False, anything else is True."""
    if x is None:
        return False
    if x == '':
        return False
    if x == '0':
        return False
    lx = x.lower()
    if lx == 'false':
        return False
    if lx == 'f':
        return False
    return True


def error_text(environ, start_response, code, message):
    start_response(str(code), [('Content-Type', 'text/plain')])
    if isinstance(message, str):
        message = message.encode()
    return [message]


def maybeset(d, k, v):
    if v is None:
        return
    d[k] = v

MAX_BODY = 1000000


def application(environ, start_response):
    if environ['REQUEST_METHOD'] == 'GET':
        return http_get(environ, start_response)
    if environ['REQUEST_METHOD'] != 'POST':
        return error_text(environ, start_response, 400, 'bad method')
    # new json:
    remote_addr = environ.get('REMOTE_ADDR')
    debug = remote_addr == '127.0.0.1' or remote_addr == '::1'
    content_type = environ.get('CONTENT_TYPE')
    if content_type != 'application/json':
        logger.info('bad content type %r, %r', content_type, environ.keys())
        return error_text(environ, start_response, 400, 'wrong type {!r}'.format(content_type))
    raw = environ['wsgi.input'].read(MAX_BODY)
    if len(raw) == MAX_BODY:
        return error_text(environ, start_response, 400, 'bad submission')
    # we should get:
    # {
    #   "n": config name, // e.g. MI_Congress
    #   "s": start time java ms,
    #   "t": stop time java ms,
    #   "r": run time float seconds,
    #   "b": {"Kmpp": float, "Spread": float, "Std": float},
    #   "ok": bool,
    #   "bestKmpp.dsz": base64...,
    #   "statsum": text,
    #
    #   "vars":{"config": cname}, // prior schema
    #   "binlog": base64..., // deprecated/forgotten
    # }
    ob = json.loads(raw)
    if ('bestKmpp.dsz' not in ob) and ('statsum' not in ob):
        return error_text(environ, start_response, 400, 'empty submission')
    # TODO: more validation of good upload
    # TODO: rate limit per submitting host
    dest_dir = os.environ[kSoldirEnvName]
    eventid = makeEventId(remote_addr) # eventid contains a / at yyyymmdd/HHMMSS
    outpath = os.path.join(dest_dir, eventid + '.json')
    outdir = os.path.dirname(outpath)
    if not os.path.isdir(outdir):
        os.makedirs(outdir, exist_ok=True)
    with open(outpath, 'wb') as fout:
        fout.write(raw)
    headers = [('Content-Type', 'application/json')]
    start_response('200 OK', headers)
    ret = {
        'id': eventid,
    }
    return [json.dumps(ret).encode()]


def dpathset(d, path, value):
    if len(path) == 1:
        d[path[0]] = value
    else:
        if d.get(path[0]) is None:
            d[path[0]] = dict()
        dpathset(d[path[0]], path[1:], value)


# return config json
# read config.json
# overlay url,post,durls from server.json
def http_get(environ, start_response):
    debug = False
    if debug:
        cgitb.enable()
    server_dir = os.environ[kSoldirEnvName]
    # districter configs, e.g. IL_Congress
    with open(os.path.join(server_dir, 'config.json')) as fin:
        allconf = json.load(fin)
    allconf['ts'] = int(time.time())
    server = serverConfig()
    maybeset(allconf, 'url', server.get('url'))
    maybeset(allconf, 'post', server.get('post'))
    maybeset(allconf, 'durls', server.get('durls'))
    overlays = server.get('overlays', [])
    for cmd in overlays:
        path, value = cmd
        path = path.split('.')
        dpathset(allconf, path, value)
    headers = []
    headers.append( ('Content-Type', 'application/json') )

    start_response('200 OK', headers)
    return [json.dumps(allconf).encode()]


if __name__ == '__main__':
    import optparse
    import wsgiref.simple_server

    argp = optparse.OptionParser()
    argp.add_option('--host', dest='host', default='', help='Host: this responds to')
    argp.add_option('--port', '-p', dest='port', default=8080, type='int')
    argp.add_option('--solution-dir', dest='soldir', default=None)
    (options, args) = argp.parse_args()
    if options.soldir:
        os.environ[kSoldirEnvName] = options.soldir
    if kSoldirEnvName not in os.environ:
        os.environ[kSoldirEnvName] = '/tmp/solutions'
    httpd = wsgiref.simple_server.make_server(
        options.host,
        options.port,
        application)
#        wsgiref.simple_server.demo_app)
    httpd.base_environ[kSoldirEnvName] = os.environ[kSoldirEnvName]
    httpd.serve_forever()
