# LIB_MAPLE_HOME needs to be defined in your environment 

export BOARD ?= 48F3
export MEMORY_TARGET ?= jtag
export USER_MODULES := $(shell pwd)

export DFU_VENDOR := 0x0483
export DFU_PRODUCT := 0xdf11

.DEFAULT_GOAL := sketch

all: sketch

%:
	$(MAKE) -f $(LIB_MAPLE_HOME)/Makefile $@

flash:	build/48F1.bin
	python2 $(LIB_MAPLE_HOME)/support/stm32loader.py -p /dev/ttyUSB0 -evw $<

DfuSeDl:	build/48F3.bin
	dfu-util -a 0 -d $(DFU_VENDOR):$(DFU_PRODUCT) -s 0x08000000:leave -D $<
