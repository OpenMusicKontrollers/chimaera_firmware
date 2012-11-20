#!/bin/bash

export LIB_MAPLE_HOME=$HOME/src/libmaple
export BOARD=maple_mini

cmc/cmc_dist.py > cmc/cmc_dist.c
cmc/cmc_lookup.py > cmc/cmc_lookup.c

SCRIPT=`readlink -f $0`
USER_MODULES=`dirname $SCRIPT` make -f $LIB_MAPLE_HOME/Makefile $@
