#!/usr/bin/python

__author__ = "Brian Olson"

"""THIS SCRIPT MAY DOWNLOAD A LOT OF DATA

Some states may have 6 Gigabytes of data to download.
Don't all swamp the Census servers at once now.

Make a directory 'data' under the build directory.

Now run this script (from the installation directory, containing data/) with
a two letter postal abbreviation for a state:
./setupstatedata.py ny
"""

# standard
import abc
import glob
import logging
import optparse
import os
import re
import string
import subprocess
import sys
import tarfile
import threading
import time
import traceback
import urllib.request, urllib.parse, urllib.error
import zipfile

# local
from . import generaterunconfigs
from . import linksfromedges
from .newerthan import newerthan, any_newerthan
from . import shapefile
from . import solution

from .states import *

logger = logging.getLogger(__name__)

class ProcessGlobals(object):
    def __init__(self, options):
        self.options = options
        self.geourls = None
        self.tigerlatest = None
        self.bestYear = None
        self.bestYearEdition = None

    def getTigerLatest(self):
        """For either shapefile or old line-file, return latest base URL."""
        return self.tigerlatest

    def getState(self, name):
        return StateData(self, name, self.options)


COUNTY_RE = re.compile(r'([0-9]{5})_(.*)')


def filterMinSize(seq, minSize=100):
    out = []
    for x in seq:
        if os.path.getsize(x) > minSize:
            out.append(x)
    return out


def checkZipFile(path):
    try:
        zf = zipfile.ZipFile(path, 'r')
        nl = zf.namelist()
        zf.close()
        return bool(nl)
    except zipfile.BadZipfile as e:
        return False


class StateData(metaclass=abc.ABCMeta):
    def __init__(self, pg, st, options):
        self.pg = pg
        self.stl = st.lower()
        self.stu = st.upper()
        self.fips = fipsForPostalCode(self.stu)
        self.name = nameForPostalCode(self.stu)
        self.bestYear = None
        self.turl = None
        self.turl_time = None
        self.ziplist = None
        self.geom = None
        self.options = options
        self.dpath = os.path.join(self.options.datadir, self.stu)
        self.setuplog = open(os.path.join(self.dpath, 'setuplog'), 'a')
        self._zipspath = os.path.join(self.dpath, 'zips')

    def mkdir(self, path, options):
        if not os.path.isdir(path):
            if options.dryrun:
                logger.info('mkdir %s', path)
            else:
                os.mkdir(path)

    def maybeUrlRetrieve(self, url, localpath, contenttype=None):
        if os.path.exists(localpath):
            return localpath
        if self.options.dryrun:
            logger.info('would fetch "%s" -> "%s"', url, localpath)
            return localpath
        logging.info('fetch "%s" -> "%s"', url, localpath)
        (filename, info) = urllib.request.urlretrieve(url, localpath)
        logging.debug('%s info: %s', url, info)
        if (contenttype is not None) and (info['content-type'] != contenttype):
            logging.error('%s came back with wrong content-type %s, wanted %s. if content is OK:\nmv %s_bad %s\nOR skip with:touch %s\n',
                url, info['content-type'], contenttype, localpath, localpath, localpath)
            os.rename(localpath, localpath + '_bad')
            #os.unlink(filename)
            if self.options.strict:
                raise Exception('download failed: ' + url + ' if this is OK, touch ' + localpath)
            return None
        return localpath

    def getTigerBase(self, dpath=None):
        """Determine the base URL for shapefile data for this state.
        Cached in 'data/XX/tigerurl'.
        example url:
        http://www2.census.gov/geo/tiger/TIGER2009/25_MASSACHUSETTS/
        """
        if self.turl is not None:
            return self.turl
        if dpath is None:
            dpath = self.dpath
        turlpath = os.path.join(dpath, 'tigerurl')
        if not os.path.isfile(turlpath):
            tigerlatest = self.pg.getTigerLatest()
            self.bestYear = self.pg.bestYear
            if self.options.shapefile:
                turl = '%s%02d_%s/' % (
                    tigerlatest, self.fips,
                    nameForPostalCode(self.stu).upper().replace(' ', '_'))
            else:
                raise Exception('old tiger line files not supported, must use post-2009 esri shapefile data')
            logger.info('guessing tiger data source "%s", if this is wrong, edit "%s"', turl, turlpath)
            if self.options.dryrun:
                return turl
            self.turl_time = time.time()
            fo = open(turlpath, 'w')
            fo.write(turl)
            fo.write('\n')
            fo.close()
            self.turl = turl
            return turl
        else:
            fi = open(turlpath, 'r')
            turl = fi.read()
            fi.close()
            m = re.search(r'tiger(\d\d\d\d)', turl, re.IGNORECASE)
            self.bestYear = int(m.group(1))
            self.turl = turl.strip()
            return self.turl

    def getTigerZipIndexHtml(self, dpath):
        """Return raw html of zip index listing. (maybe cached)"""
        zipspath = self.zipspath(dpath)
        indexpath = os.path.join(zipspath, 'index.html')
        if not os.path.isfile(indexpath):
            self.mkdir(zipspath, self.options)
            turl = self.getTigerBase(dpath)
            uf = urllib.request.urlopen(turl)
            raw = uf.read()
            uf.close()
            if self.options.dryrun:
                logger.info('would write "%s"', indexpath)
            else:
                of = open(indexpath, 'w')
                of.write(raw)
                of.close()
        else:
            f = open(indexpath, 'r')
            raw = f.read()
            f.close()
        return raw

    def getTigerZipIndex(self, dpath):
        """Return list of basic zip files to download."""
        if self.ziplist is not None:
            return self.ziplist
        raw = self.getTigerZipIndexHtml(dpath)
        self.ziplist = []
        if self.options.shapefile:
            # TODO: in the future, get tabblock only but not tabblock00
            for m in re.finditer(r'href="([^"]*tabblock00[^"]*.zip)"', raw, re.IGNORECASE):
                self.ziplist.append(m.group(1))
        else:
            raise Exception('old tiger line files not supported, must use post-2009 esri shapefile data')
        return self.ziplist

    def getCountyPaths(self):
        """Return list of relative href values for county dirs."""
        raw = self.getTigerZipIndexHtml(self.dpath)
        # NV, VA has some city regions not part of county datasets
        # AK has "Borough", "Census_Area", "Municipality"
        # LA has "Parish"
        re_string = 'href="(%02d\\d\\d\\d_[^"]+(?:County|city|Municipality|Census_Area|Borough|Parish)/?)"' % (fipsForPostalCode(self.stu))
        return re.findall(re_string, raw, re.IGNORECASE)

    def goodZip(self, localpath, url):
        if not os.path.exists(localpath):
            return False
        if os.path.getsize(localpath) == 0:
            # empty files are for skipping
            return True
        ok = checkZipFile(localpath)
        if not ok:
            badpath = localpath + '_bad'
            if os.path.exists(badpath):
                os.unlink(badpath)
            os.rename(localpath, badpath)
            logger.info('bad zip file "%s" moved aside to "%s". (from url %s) to skip: ` rm "%s" && touch "%s" `', localpath, badpath, url, badpath, localpath)
        return ok

    def getEdges(self):
        # http://www2.census.gov/geo/tiger/TIGER2009/25_MASSACHUSETTS/25027_Worcester_County/tl_2009_25027_edges.zip
        counties = self.getCountyPaths()
        base = self.getTigerBase()
        for co in counties:
            m = COUNTY_RE.match(co)
            filename = 'tl_%4d_%s_edges.zip' % (self.bestYear, m.group(1))
            localpath = os.path.join(self.zipspath(), filename)
            url = base + co + filename
            self.maybeUrlRetrieve(url, localpath, 'application/zip')
            ok = self.goodZip(localpath, url)
            if not ok:
                return False
        return True

    def getFaces(self):
        # http://www2.census.gov/geo/tiger/TIGER2009/25_MASSACHUSETTS/25027_Worcester_County/tl_2009_25027_faces.zip
        counties = self.getCountyPaths()
        base = self.getTigerBase()
        for co in counties:
            m = COUNTY_RE.match(co)
            filename = 'tl_%4d_%s_faces.zip' % (self.bestYear, m.group(1))
            localpath = os.path.join(self.zipspath(), filename)
            url = base + co + filename
            self.maybeUrlRetrieve(url, localpath, 'application/zip')
            ok = self.goodZip(localpath, url)
            if not ok:
                return False
        return True

    def zipspath(self, dpath=None):
        if dpath is None:
            return self._zipspath
        return os.path.join(dpath, 'zips')

    def downloadTigerZips(self, dpath):
        """Download basic data as needed to XX/zips/"""
        turl = self.getTigerBase(dpath)
        ziplist = self.getTigerZipIndex(dpath)
        zipspath = self.zipspath(dpath)
        for z in ziplist:
            zp = os.path.join(zipspath, z)
            self.maybeUrlRetrieve(turl + z, zp)
        return ziplist

    def processShapefile(self, dpath):
        """Build .links and .mppb rasterization from shapefile."""
        bestzip = None
        ziplist = self.downloadTigerZips(dpath)
        for zname in ziplist:
            if shapefile.betterShapefileZip(zname, bestzip):
                bestzip = zname
        if bestzip is None:
            logger.info('found no best zipfile to use')
            return None
        zipspath = self.zipspath(dpath)
        assert zipspath is not None
        bestzip = os.path.join(zipspath, bestzip)
        tabblockPath = os.path.join(zipspath, 'tl_2010_%02d_tabblock10.zip' % (self.fips,))
        useFaces = True
        if not useFaces:
            # Disable detailed rendering for now, the blocks don't line up right.
            facesPaths = None
            edgesPaths = None
        else:
            facesPaths = glob.glob(os.path.join(zipspath, '*faces*zip'))
            edgesPaths = glob.glob(os.path.join(zipspath, '*edges*zip'))
            facesPaths = filterMinSize(facesPaths, 100)
            edgesPaths = filterMinSize(edgesPaths, 100)
        #linksname = os.path.join(dpath, self.stl + '101.uf1.links')
        linksname = os.path.join(dpath, 'geoblocks.links')
        mppb_name = os.path.join(dpath, self.stu + '.mppb')
        mask_name = os.path.join(dpath, self.stu + 'blocks.png')
        mppbsm_name = os.path.join(dpath, self.stu + '_sm.mppb')
        masksm_name = os.path.join(dpath, self.stu + 'blocks_sm.png')
        mppblg_name = os.path.join(dpath, self.stu + '_lg.mppb')
        masklg_name = os.path.join(dpath, self.stu + 'blocks_lg.png')
        projname = projectionForPostalCode(self.stu)
        logger.info('%s proj = %s', self.stu, projname)
        linksargs = None
        baseRenderArgs = [
            '--boundx', '1920', '--boundy', '1080',
            '--rastgeom', os.path.join(dpath, 'rastgeom')
        ]
        if projname:
            projectionArgs = ['--proj', projname]
        else:
            projectionArgs = []
        renderArgs = []
        commands = []
        needlinks = True
        # trying links from tabblock, render from faces
        #if (not useFaces) and newerthan(tabblockPath, linksname):
        if newerthan(tabblockPath, linksname):
            commands.append(shapefile.makeCommand(
                [tabblockPath, '--links', linksname],
                self.options.bindir, self.options.strict))
            needlinks = False
        if needlinks and edgesPaths and facesPaths and (any_newerthan(edgesPaths, linksname) or any_newerthan(facesPaths, linksname)):
            logger.info('need %s from edges+faces', linksname)
            lecmd = linksfromedges.makeCommand(facesPaths + edgesPaths + ['--links', linksname], self.options.bindir, self.options.strict)
            commands.append(lecmd)
        elif needlinks and (not edgesPaths) and facesPaths and any_newerthan(facesPaths, linksname):
            logger.info('need %s from faces', linksname)
            linksargs = ['--links', linksname]
        elif needlinks and (not edgesPaths) and (not facesPaths) and newerthan(bestzip, linksname):
            logger.info('need %s from %s', linksname, bestzip)
            linksargs = ['--links', linksname]
        if facesPaths:
            if any_newerthan(facesPaths, mppb_name) or self.options.redraw:
                logger.info('need %s from faces', mppb_name)
                renderArgs += ['--rast', mppb_name]
            if any_newerthan(facesPaths, mask_name) or self.options.redraw:
                logger.info('need %s from faces', mask_name)
                renderArgs += ['--mask', mask_name]
        else:
            if newerthan(bestzip, mppb_name) or self.options.redraw:
                logger.info('need %s from %s', mppb_name, bestzip)
                renderArgs += ['--rast', mppb_name]
            if newerthan(bestzip, mask_name) or self.options.redraw:
                logger.info('need %s from %s', mask_name, bestzip)
                renderArgs += ['--mask', mask_name]

        # Build primary command to make links and/or .mppb raster
        if renderArgs:
            renderArgs = baseRenderArgs + renderArgs + projectionArgs
            if linksargs:
                if not facesPaths:
                    command = shapefile.makeCommand(
                        linksargs + renderArgs + [bestzip], self.options.bindir, self.options.strict)
                    commands.append(command)
                else:
                    command = shapefile.makeCommand(
                        linksargs + [bestzip], self.options.bindir, self.options.strict)
                    commands.append(command)
                    command = shapefile.makeCommand(
                        renderArgs + facesPaths, self.options.bindir, self.options.strict)
                    commands.append(command)
            else:
                if facesPaths:
                    command = shapefile.makeCommand(
                        renderArgs + facesPaths, self.options.bindir, self.options.strict)
                    commands.append(command)
                else:
                    command = shapefile.makeCommand(
                        renderArgs + [bestzip], self.options.bindir, self.options.strict)
                    commands.append(command)
        elif linksargs:
                command = shapefile.makeCommand(
                    linksargs + [bestzip], self.options.bindir, self.options.strict)
                commands.append(command)

        # Maybe make {stu}_sm.mppb
        smargs = []
        if (facesPaths and (any_newerthan(facesPaths, mppbsm_name)  or self.options.redraw)) or (newerthan(bestzip, mppbsm_name) or self.options.redraw):
            logger.info('need %s', mppbsm_name)
            smargs += ['--rast', mppbsm_name]
        if (facesPaths and (any_newerthan(facesPaths, masksm_name) or self.options.redraw)) or (newerthan(bestzip, masksm_name) or self.options.redraw):
            logger.info('need %s', masksm_name)
            smargs += ['--mask', masksm_name]
        if smargs:
            smargs += ['--boundx', '640', '--boundy', '480'] + projectionArgs
            if facesPaths:
                smargs = smargs + facesPaths
            else:
                smargs.append(bestzip)
            command = shapefile.makeCommand(smargs,
                self.options.bindir, self.options.strict)
            commands.append(command)

        # Maybe make {stu}_lg.mppb
        lgargs = []
        if (facesPaths and (any_newerthan(facesPaths, mppblg_name) or self.options.redraw)) or (newerthan(bestzip, mppblg_name) or self.options.redraw):
            logger.info('need %s', mppblg_name)
            lgargs += ['--rast', mppblg_name]
        if (facesPaths and (any_newerthan(facesPaths, masklg_name) or self.options.redraw)) or (newerthan(bestzip, masklg_name) or self.options.redraw):
            logger.info('need %s', masklg_name)
            lgargs += ['--mask', masklg_name]
        if lgargs:
            lgargs += ['--boundx', '3840', '--boundy', '2160'] + projectionArgs
            if facesPaths:
                lgargs = lgargs + facesPaths
            else:
                lgargs.append(bestzip)
            command = shapefile.makeCommand(lgargs,
                self.options.bindir, self.options.strict)
            commands.append(command)

        # Run any accumulated commands in processShapefile()
        for command in commands:
            logger.info('command: %s', ' '.join(command))
            if not self.options.dryrun:
                status = subprocess.call(command, shell=False, stdin=None)
                if status != 0:
                    raise Exception('error (%d) executing: "%s"' % (status, ' '.join(command)))
        return linksname

    def makelinks(self, dpath):
        """Deprecated. Use shapefile bundle."""
        if self.options.shapefile:
            return self.processShapefile(dpath)
        raise Exception('old tiger line files not supported, must use post-2009 esri shapefile data')

    def compileBinaryData(self, dpath=None):
        if dpath is None:
            dpath = self.dpath
        uf1path = os.path.join(dpath, self.stl + '101.uf1')
        linkspath = uf1path + '.links'
        outpath = None
        binpath = os.path.join(self.options.bindir, 'linkfixup')
        cmd = [binpath, '-U', self.stl + '101.uf1']
        if self.options.protobuf:
            cmd.append('-p')
            outpath = self.stl + '.pb'
            cmd.append(outpath)
        else:
            cmd.append('-o')
            outpath = self.stl + '.gbin'
            cmd.append(outpath)
        outpath = os.path.join(dpath, outpath)
        needsbuild = not os.path.isfile(outpath)
        if (not needsbuild) and newerthan(uf1path, outpath):
            logger.info('%s > %s', uf1path, outpath)
            needsbuild = True
        if (not needsbuild) and newerthan(linkspath, outpath):
            logger.info('%s > %s', linkspath, outpath)
            needsbuild = True
        if (not needsbuild) and newerthan(binpath, outpath):
            logger.info('%s > %s', binpath, outpath)
            needsbuild = True
        if not needsbuild:
            return
#        if not (newerthan(uf1path, outpath) or newerthan(linkspath, outpath)):
#            return
        logger.info('cd %s && "%s"', dpath, '" "'.join(cmd))
        if self.options.dryrun:
            return
        start = time.time()
        status = subprocess.call(cmd, cwd=dpath)
        logger.info('data compile took %f seconds', time.time() - start)
        if status != 0:
            raise Exception('error (%d) executing: cd %s && "%s"' % (status, dpath,'" "'.join(cmd)))

    def archiveRunfiles(self):
        """Bundle key data as XX_runfiles.tar.gz"""
        if ((not os.path.isdir(self.options.archive_runfiles)) or
            (not os.access(self.options.archive_runfiles, os.X_OK|os.W_OK))):
            sys.stderr.write('error: "%s" is not a writable directory\n' % self.options.archive_runfiles)
            return None
        destpath = os.path.join(self.options.archive_runfiles, self.stu + '_runfiles.tar.gz')
        dpath = os.path.join(self.options.datadir, self.stu)
        partpaths = [
            os.path.join(dpath, self.stl + '.pb'),
            os.path.join(dpath, self.stu + '.mppb'),
        ] + glob.glob(os.path.join(dpath, 'config', '*'))
        needsupdate = False
        for part in partpaths:
            if (not needsupdate) and newerthan(part, destpath):
                needsupdate = True
        if not needsupdate:
            return destpath
        out = tarfile.open(destpath, 'w|gz')
        out.posix = True
        for part in partpaths:
            arcname = part
            if part.startswith(self.options.datadir):
                arcname = part[len(self.options.datadir):]
                while arcname[0] == '/':
                    arcname = arcname[1:]
            out.add(part, arcname, False)
        out.close()
        return destpath

    def clean(self):
        for (dirpath, dirnames, filenames) in os.walk(self.dpath):
            for fname in filenames:
                fpath = os.path.join(dirpath, fname)
                if fname.lower().endswith('.zip'):
                    logging.debug('not cleaning "%s"', fpath)
                    continue
                if self.options.dryrun or self.options.verbose:
                    logger.info('rm %s', fpath)
                if not self.options.dryrun:
                    os.remove(fpath)

    def dostate(self):
        start = time.time()
        logger.info('start at %s\n', time.ctime(start))
        ok = False
        try:
            if self.options.clean:
                self.clean()
                ok = True
            else:
                ok = self.dostate_inner()
        except:
            errmsg = traceback.format_exc() + ('\n%s error running %s\n' % (self.stu, self.stu))
            logger.info(errmsg)
        logger.info('ok=%s after %f seconds\n', ok, time.time() - start)
        self.setuplog.flush()
        sys.stdout.flush()

    # overridden in crawl2010.py and crawl2020.py
    @abc.abstractmethod
    def dostate_inner(self):
        return False


def getOptionParser():
    default_bindir = os.environ.get('REDISTRICTER_BIN')
    if default_bindir is None:
        default_bindir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..'))
    default_datadir = os.environ.get('REDISTRICTER_DATA')
    if default_datadir is None:
        default_datadir = os.path.abspath(os.path.join(os.getcwd(), 'data'))
    argp = optparse.OptionParser()
# commented out options aren't actually used.
#    argp.add_option('-m', '--make', action='store_true', dest='domaake', default=False)
#    argp.add_option('--nopng', '--without_png', dest='png', action='store_false', default=True)
#    argp.add_option('--unpackall', action='store_true', dest='unpackall', default=False)
    argp.add_option('-n', '--dry-run', action='store_true', dest='dryrun', default=False)
    argp.add_option('--gbin', action='store_false', dest='protobuf', default=True)
    argp.add_option('-d', '--data', dest='datadir', default=default_datadir)
    argp.add_option('--bindir', dest='bindir', default=default_bindir)
    argp.add_option('--getextra', dest='extras', action='append', default=[])
    argp.add_option('--extras_only', dest='extras_only', action='store_true', default=False)
    argp.add_option('--faces', dest='getFaces', action='store_true', default=False, help='fetch and process detailed faces shapefile data.')
    argp.add_option('--edges', dest='getEdges', action='store_true', default=False, help='fetch and process detailed edges shapefile data.')
    argp.add_option('--shapefile', dest='shapefile', action='store_true', default=True)
    argp.add_option('--noshapefile', dest='shapefile', action='store_false')
    argp.add_option('--clean', dest='clean', action='store_true', default=False)
    argp.add_option('--verbose', dest='verbose', action='store_true', default=False)
    argp.add_option('--strict', dest='strict', action='store_true', default=False)
    argp.add_option('--archive-runfiles', dest='archive_runfiles', default=None, help='directory path to store tar archives of run file sets into')
    argp.add_option('--datasets', dest='archive_runfiles', help='directory path to store tar archives of run file sets into')
    argp.add_option('--threads', dest='threads', type='int', default=1, help='number of threads to run')
    argp.add_option('--redraw', action='store_true', default=False, help='do rasterization again')
    return argp

def getOptions():
    argp = getOptionParser()
    return argp.parse_args()


class SyncrhonizedIteratorWrapper(object):
    def __init__(self, it):
        self.it = it.__iter__()
        self.lock = threading.Lock()

    def __iter__(self):
        return self

    def __next__(self):
        self.lock.acquire()
        try:
            out = next(self.it)
        except:
            self.lock.release()
            raise
        self.lock.release()
        return out


def runloop(states, pg, options):
    for a in states:
        start = time.time()
        sd = None
        try:
            sd = pg.getState(a)
            sd.dostate()
        except:
            traceback.print_exc()
            errmsg = traceback.format_exc() + ('\n%s error running %s\n' % (a, a))
            sys.stdout.write(errmsg)
            if sd is not None:
                sd.logf(errmsg)
        sys.stdout.write('%s took %f seconds\n' % (a, time.time() - start))
        sys.stdout.flush()


def main(argv):
    (options, args) = getOptions()
    if options.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    if not os.path.isdir(options.datadir):
        raise Exception('data dir "%s" does not exist' % options.datadir)

    if not options.shapefile:
        raise Exception('old tiger line files not supported, must use post-2009 esri shapefile data')
    pg = ProcessGlobals(options)
    runMaybeThreaded(args, pg, options)


def runMaybeThreaded(stulist, pg, options):
    if options.threads == 1:
        runloop(stulist, pg, options)
    else:
        tlist = []
        statelist = SyncrhonizedIteratorWrapper(stulist)
        for x in range(0, options.threads):
            threadLabel = 't%d' % x
            tlist.append(threading.Thread(target=runloop, args=(statelist, pg, options), name=threadLabel))
        for x in tlist:
            x.start()
        for x in tlist:
            x.join()


if __name__ == '__main__':
    main(sys.argv)
