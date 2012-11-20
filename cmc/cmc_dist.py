#!/usr/bin/env python

from math import *

# XXX choose a according to magnets and sensors: 0.25, 0.5, 1, 2, 3, 4, 5, 6, ...
a = 1

print ('#include <libmaple/libmaple_types.h>\n')
print ('#include "cmc_private.h"\n')

print ('fix_0_16_t dist [0x800] __attr_flash = {')

for B in range (0x800):
	b = B / 0x7ff
	Vb = 0.07388 + 1.33082*sqrt(b) - 0.41212*b

	print ('\t', Vb, ',')

print ('};')
