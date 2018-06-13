#!/usr/bin/env python3
# touch work/stop to gracefully stop with a cycle is done
# touch work/reload to make this script exec itself at that time
# These are also settable by buttons on the web interface.
# (these files are deleted after they're detected so that they only apply once)
#
# Some interesting options to pass to the underlying script after '--':
#--verbose
#--diskQuota=1000000000
#
# Additional arguments to the solver can be set in work/configoverride

import gzip
import optparse
import os
import select
import subprocess
import sys
import threading
import time

def read_flagfile(op, linesource, options=None, old_args=None):
    """Return (options, args) as per OptionParser.parse_args()."""
    lines = []
    for line in linesource:
        line = line.strip()
        if not line:
            continue
        if line[0] == '#':
            continue
        lines.append(line)
    (options, args) = op.parse_args(args=lines, values=options)
    if old_args:
        args = old_args + args
    return (options, args)

def main():
    op = optparse.OptionParser()
    op.add_option('--port', dest='port', type='int', default=9988)
    op.add_option('--threads', dest='threads', type='int', default=2)
    op.add_option('--flagfile', dest='flagfile', default=None, help='reads specified file of flags, one per line, or ./flagfile if it exists')
    op.add_option('--log', dest='logname', default=None)
    op.add_option('--bindir', default=None)
    op.add_option('--server', default=None)
    (options, args) = op.parse_args()

    if options.flagfile:
        (options, args) = read_flagfile(op, open(options.flagfile, 'r'), options, args)
    elif os.path.exists('flagfile'):
        (options, args) = read_flagfile(op, open('flagfile', 'r'), options, args)
    rootdir = os.path.dirname(os.path.abspath(__file__))
    bindir = options.bindir or os.path.join(rootdir, 'bin')
    datadir = os.path.join(os.getcwd(), 'data')
    workdir = os.path.join(os.getcwd(), 'work')

    if not os.path.isdir(datadir):
        os.mkdir(datadir)
    if not os.path.isdir(workdir):
        os.mkdir(workdir)

    runallstates = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'runallstates.py')
    if options.server:
        args.append('--server=' + options.server)

    cmd = [runallstates,
    '--bestlog=bestlog',
    '--runlog=runlog',
    '--d2',
    '--fr=4/9',
#  '--server=http://bots.bdistricting.net/rd_datasets/',
    '--port=%d' % (options.port,),
    '--threads=%d' % (options.threads,),
    '--datadir=' + datadir,
    '--bindir=' + bindir] + args

    proc = subprocess.Popen(cmd, cwd=workdir, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    print('status should be available on')
    print('http://localhost:%d/' % (options.port,))

    log = None
    if options.logname:
        if options.logname == '-':
            logpath = None
            log = sys.stdout
        else:
            logpath = options.logname
    else:
        logpath = os.path.join(workdir, 'dblog_')
    if log is None:
        log = RotatingLogWriter(logpath)
    log.write('# cmd: cd %s && "%s"\n' % (workdir, '" "'.join(cmd)))
    piped_run(proc, log)
    log.close()

    reloadmarker = os.path.join(workdir, 'reload')
    if os.path.exists(reloadmarker):
        os.unlink(reloadmarker)
        os.execv(__file__, sys.argv)



class RotatingLogWriter(object):
    def __init__(self, prefix):
        self.prefix = prefix
        self.outname = None
        self.out = None
        self.currentDay = None
        self.lastMarkerTime = None

        now = time.localtime()
        self.startOut(now)

    def startOut(self, now):
        self.outname = self.prefix + ('%04d%02d%02d.gz' % (now[0], now[1], now[2]))
        self.currentDay = now[2]
        self.out = gzip.open(self.outname, 'at', 9)

    def write(self, data):
        now = time.time()
        lnow = time.localtime(now)
        if (not self.lastMarkerTime) or ((now - self.lastMarkerTime) > 60):
            self.lastMarkerTime = now
            self.out.write(time.strftime('# %Y%m%d_%H%M%S\n', lnow))
        self.out.write(data)
        if lnow[2] != self.currentDay:
            self.out.close()
            self.startOut(lnow)

    def close(self):
        self.out.close()

def lastlines(arr, limit, line):
    """Maintain up to limit lines in arr as fifo."""
    if len(arr) >= limit:
        start = (len(arr) - limit) + 1
        arr = arr[start:]
    arr.append(line)
    return arr

class PipeReader(threading.Thread):
    def __init__(self, fin, out, limit=10, prefix=''):
        threading.Thread.__init__(self)
        self.fin = fin
        self.out = out
        self.lastlines = []
        self.limit = limit
        self.prefix = prefix
    def run(self):
        for line in self.fin:
            if isinstance(line, bytes):
                line = line.decode()
            self.out.write(self.prefix + line)
            lastlines(self.lastlines, self.limit, line)

def thread_reader_run(proc, out):
    outreader = PipeReader(proc.stdout, out, prefix='O: ')
    errreader = PipeReader(proc.stderr, out, prefix='E: ')
    outreader.start()
    errreader.start()
    retcode = proc.wait()
    outreader.join()
    errreader.join()
    return outreader.lastlines, errreader.lastlines


# TODO: deprecated, delete dead code
def poll_run(p, out):
    """Read err and out from a process, copying to sys.stdout.
    Return last 10 lines of error in list."""
    poller = select.poll()
    poller.register(p.stdout, select.POLLIN | select.POLLPRI )
    poller.register(p.stderr, select.POLLIN | select.POLLPRI )
    lastolines = []
    lastelines = []
    while p.poll() is None:
        for (fd, event) in poller.poll(500):
            if p.stdout.fileno() == fd:
                line = p.stdout.readline()
                if line:
                    out.write("O: " + str(line))
                    lastlines(lastolines, 10, line)
            elif p.stderr.fileno() == fd:
                line = p.stderr.readline()
                if line:
                    out.write("E: " + str(line))
                    lastlines(lastelines, 10, line)
            else:
                out.write("? fd=%d\n" % (fd,))
    return (lastolines, lastelines)

# TODO: deprecated, delete dead code
def select_run(p, out):
    """Read err and out from a process, copying to sys.stdout.
    Return last 10 lines of error in list."""
    lastolines = []
    lastelines = []
    while p.poll() is None:
        (il, ol, el) = select.select([p.stdout, p.stderr], [], [], 0.5)
        for fd in il:
            if (p.stdout.fileno() == fd) or (fd == p.stdout):
                line = p.stdout.readline()
                if line:
                    out.write("O: " + line)
                    lastlines(lastolines, 10, line)
            elif (p.stderr.fileno() == fd) or (fd == p.stderr):
                line = p.stderr.readline()
                if line:
                    out.write("E: " + line)
                    lastlines(lastelines, 10, line)
            else:
                out.write("? fd=%d\n" % (fd,))
    return (lastolines, lastelines)

has_poll = "poll" in dir(select)
has_select = "select" in dir(select)

def piped_run(p, out):
    """Returns (outlines, errorlines)."""
    if True:
        thread_reader_run(p, out)
    elif has_poll:
        return poll_run(p, out)
    elif has_select:
        return select_run(p, out)
    else:
        assert False, 'everything is ruined forever'


if __name__ == '__main__':
    main()
