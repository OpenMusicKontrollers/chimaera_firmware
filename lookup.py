#!/usr/bin/env python

from math import *

print ('const float lookup_sqrt [0x800] = {')

for B in range (0x800):
	b = sqrt (B / 0x7ff)
	print ('\t', b, ',')

print ('};')

