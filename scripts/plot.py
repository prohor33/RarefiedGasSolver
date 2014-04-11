__author__ = 'dimaxx'

import matplotlib.pyplot as plt
import numpy
from numpy import *

NX = 130
NY = 70
max_files = 2
each = 1
gas_num = 2

# hack
a = NX
NX = NY
NY = a

for gas in range(gas_num):
  data_folder = '../out/gas' + '%i' % gas + '/'
  i = 0
  while i < max_files:
    s = "%i" % i
    D = numpy.fromfile(data_folder+'conc/'+s+'.bin', dtype=float).reshape(NX, NY)
    for x in range(NX):
      for y in range(NY):
        if D[x][y] == 0.0:
          D[x][y] = nan;
    plt.imshow(D, vmin=0.82, vmax=1.1, interpolation='nearest')
    plt.colorbar()
    plt.contour(D, colors='black')
    plt.savefig(data_folder+'conc/pic/'+s+'.png', dpi=100)
    plt.close()
    print("%i of %i" % (i, max_files))
    i += each
  i = 0
  while i < max_files:
    s = "%i" % i
    D = numpy.fromfile(data_folder+'temp/'+s+'.bin', dtype=float).reshape(NX, NY)
    for x in range(NX):
      for y in range(NY):
        if D[x][y] == 0.0:
          D[x][y] = nan;
    plt.imshow(D, vmin=0.8, vmax=1.0, interpolation='nearest')
    plt.colorbar()
    plt.contour(D, colors='black')
    plt.savefig(data_folder+'temp/pic/'+s+'.png', dpi=100)
    plt.close()
    print("%i of %i" % (i, max_files))
    i += each
  i = 0
  while i < max_files:
    s = "%i" % i
    D = numpy.fromfile(data_folder+'pressure/'+s+'.bin', dtype=float).reshape(NX, NY)
    for x in range(NX):
      for y in range(NY):
        if D[x][y] == 0.0:
          D[x][y] = nan;
    plt.imshow(D, vmin=0.64, vmax=1.1, interpolation='nearest')
    plt.colorbar()
    plt.contour(D, colors='black')
    plt.savefig(data_folder+'pressure/pic/'+s+'.png', dpi=100)
    plt.close()
    print("%i of %i" % (i, max_files))
    i += each