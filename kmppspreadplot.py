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
      out.write("%f\t%f\n" % (spread, kmpp))
  out.flush()

def gnuplot_out(png_name):
  sys.stderr.write("opening output \"%s\"\n" % png_name)
  gnuplot_process = subprocess.Popen("gnuplot", stdin=subprocess.PIPE)
  out = gnuplot_process.stdin
  out.write(gnuplot_command.substitute(outname=png_name))
  return out

def main(argv):
  try:
    opts, args = getopt.gnu_getopt(argv[1:], "i:", ["png="])
  except getopt.GetopError:
    sys.exit(1)
  out = sys.stdout
  for option, optarg in opts:
    if option == "--png":
      out = gnuplot_out(optarg)
    elif option == "-i":
      args.append(optarg)
    else:
      sys.stderr.write("""wtf? option="%s" optarg="%s"\n""" %
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
