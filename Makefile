# LIB_MAPLE_HOME needs to be defined in your environment 

export BOARD ?= 48F1
export MEMORY_TARGET ?= jtag
export USER_MODULES := $(shell pwd)

.DEFAULT_GOAL := sketch

all: sketch

%:
	$(MAKE) -f $(LIB_MAPLE_HOME)/Makefile $@

flash:	build/48F1.bin
	python2 $(LIB_MAPLE_HOME)/support/stm32loader.py -p /dev/ttyUSB0 -evw $<
