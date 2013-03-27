#!/usr/bin/env python

from math import *

print ('#include <armfix.h>\n')

#print ('const fix_0_32_t lookup [0x800] = {')
#
#for B in range (0x800):
#	b = B / 0x7ff
#	print ('\t', b, ',')
#
#print ('};\n\n')

print ('const fix_0_32_t lookup_sqrt [0x800] = {')

for B in range (0x800):
	b = sqrt (B / 0x7ff)
	print ('\t', b, ',')

print ('};')

