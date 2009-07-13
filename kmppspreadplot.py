#!/usr/bin/python

"""Recursively scan a directory for 'statsum' files and plot kmpp vs spread"""

__author__ = "Brian Olson"

import getopt
import os
import re
import string
import subprocess
import sys

kmppspread = re.compile(
    r".*Best Km/p: Km/p=([0-9.]+) spread=([0-9.]+).*",
    re.MULTILINE|re.DOTALL)

gnuplot_command = string.Template(
"""set xlabel 'spread'
set ylabel 'Km/p'
set key off
set terminal png
set output '${outname}'
plot '-'
""")

def walk_statsums(out, startpath):
  for root, dirs, files in os.walk(startpath):
    if 'statsum' in files:
      f = open(os.path.join(root, 'statsum'), 'r')
      m = kmppspread.match(f.read(999999))
      f.close()
      if not m:
        sys.stderr.write(
            "failed to parse %s\n" % os.path.join(root, 'statsum'))
        continue
      kmpp = float(m.group(1))
      spread = float(m.group(2))
      #out.write("%f\t%f\n" % (spread, kmpp))
      out.xy(spread, kmpp)
  #out.flush()

def gnuplot_out(png_name):
  sys.stderr.write("opening output \"%s\"\n" % png_name)
  gnuplot_process = subprocess.Popen("gnuplot", stdin=subprocess.PIPE)
  out = gnuplot_process.stdin
  out.write(gnuplot_command.substitute(outname=png_name))
  return out

class gnuplotter(object):
  def __init__(self, fname):
    self.fout = gnuplot_out(fname)

  def xy(self, x, y):
    self.fout.write("%f\t%f\n" % (x, y))

  def close(self):
    self.fout.close()

class svgplotter(object):
  """TODO: make this actually emit valid SVG"""
  def __init__(self, fname):
    self.fname = fname
    self.points = []
    self.minx = None
    self.miny = None
    self.maxx = None
    self.maxy = None
    self.fout = None
    self.width = 1024
    self.height = 768
    self.xoffset = 80
    self.yoffset = 30
    self.scalex = 1.0
    self.scaley = 1.0

  def xy(self, x, y):
    if (self.minx is None) or (self.minx > x):
      self.minx = x
    if (self.maxx is None) or (self.maxx < x):
      self.maxx = x
    if (self.miny is None) or (self.miny > y):
      self.miny = y
    if (self.maxy is None) or (self.maxy < y):
      self.maxy = y
    self.points.append((x,y))

  def tx(self, x):
    return ((x - self.minx) * self.scalex) + self.xoffset

  def ty(self, y):
    return ((self.maxy - y) * self.scaley) + self.yoffset

  def close(self):
    self.fout = open(self.fname, 'w')
    self.scalex = self.width / ((self.maxx - self.minx) * 1.10)
    self.scaley = self.height / ((self.maxy - self.miny) * 1.10)
    self.fout.write(
'''<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="%d" height="%d" xmlns="http://www.w3.org/2000/svg">\n''' % (self.width, self.height))
    dx = self.width * 0.05
    dy = self.height * 0.05
    for p in self.points:
      x = self.tx(p[0])
      y = self.ty(p[1])
      self.fout.write('<circle cx="%f" cy="%f" r="2" />\n' % (x, y))
    minxp = self.tx(self.minx)
    minyp = self.ty(self.miny)
    maxxp = self.tx(self.maxx)
    maxyp = self.ty(self.maxy)
    self.fout.write(
        '<path d="M %f %f L %f,%f" stroke="green" stroke-width="1"/>\n' % (
        minxp, minyp, minxp, minyp + 10))
    self.fout.write(
        '<text x="%f" y="%f" text-anchor="start" dominant-baseline="text-before-edge">%.3f</text>\n' %
        (minxp, minyp + 10, self.minx))

    self.fout.write(
        '<path d="M %f %f L %f,%f" stroke="red" stroke-width="1"/>\n' % (
        maxxp, minyp, maxxp, minyp + 10))
    self.fout.write(
        '<text x="%f" y="%f" text-anchor="end" dominant-baseline="text-before-edge">%.3f</text>\n' %
        (maxxp, minyp + 10, self.maxx))

    self.fout.write(
        '<path d="M %f %f L %f,%f" stroke="green" stroke-width="1"/>\n' % (
        minxp, minyp, minxp - 10, minyp))
    self.fout.write(
        '<text x="%f" y="%f" text-anchor="end" dominant-baseline="bottom">%.3f</text>\n' %
        (minxp - 10, minyp, self.miny))

    self.fout.write(
        '<path d="M %f %f L %f,%f" stroke="red" stroke-width="1"/>\n' % (
        minxp, maxyp, minxp - 10, maxyp))
    self.fout.write(
        '<text x="%f" y="%f" text-anchor="end" dominant-baseline="text-before-edge">%.3f</text>\n' %
        (minxp - 10, maxyp, self.maxy))

    self.fout.write(
        '<text text-anchor="middle" dominant-baseline="text-before-edge" transform="translate(%f, %f) rotate(90)">average km per person</text>\n' %
        (minxp - 5, (maxyp + minyp) / 2.0))
    self.fout.write(
        '<text x="%f" y="%f" text-anchor="middle" dominant-baseline="text-before-edge">district population spread</text>\n' %
        ((minxp + maxxp) / 2.0, minyp + 5))
    
    self.fout.write('</svg>\n')
    self.fout.close()
    self.fout = None

def main(argv):
  try:
    opts, args = getopt.gnu_getopt(argv[1:], 'i:', ['png=', 'svg='])
  except getopt.GetoptError:
    sys.exit(1)
  #out = sys.stdout
  out = None
  for option, optarg in opts:
    if option == '--png':
      #out = gnuplot_out(optarg)
      out = gnuplotter(optarg)
    elif option == '--svg':
      out = svgplotter(optarg)
    elif option == '-i':
      args.append(optarg)
    else:
      sys.stderr.write('wtf? option="%s" optarg="%s"\n' %
                       (option, optarg))
      sys.exit(1)
  if args:
    for x in args:
      walk_statsums(out, x)
  else:
    walk_statsums(out, '.')
  out.close()

if __name__ == '__main__':
  main(sys.argv)
