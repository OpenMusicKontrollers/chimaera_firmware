#!/usr/bin/env python

from math import *

# XXX choose a according to magnets and sensors: 0.25, 0.5, 1, 2, 3, 4, 5, 6, ...
a = 1

print ('#include <libmaple/libmaple_types.h>\n')
print ('#include "cmc_private.h"\n')

print ('ufix32_t dist [0x800] __attr_flash = {')

for B in range (0x800):
	b = B / 0x7ff
	Vb = (a+1)/a * (1 - 1/sqrt(b*((a+1)^2-1)+1))
	print ('\t', Vb, ',')

print ('};')
