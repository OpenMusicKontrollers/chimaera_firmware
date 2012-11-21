#!/usr/bin/env python

from math import *

print ('#include <libmaple/libmaple_types.h>\n')
print ('#include "cmc_private.h"\n')

print ('fix_0_16_t lookup [0x800] __attr_flash = {')

for B in range (0x800):
	b = B / 0x7ff
	print ('\t', b, ',')

print ('};\n\n')

print ('fix_0_16_t lookup_sqrt [0x800] __attr_flash = {')

for B in range (0x800):
	b = sqrt (B / 0x7ff)
	print ('\t', b, ',')

print ('};')

