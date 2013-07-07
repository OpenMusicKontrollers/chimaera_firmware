# LIB_MAPLE_HOME needs to be defined in your environment 

TARGET = F303CB
#TARGET = F303CC

export BOARD ?= $(TARGET)
export MEMORY_TARGET ?= jtag
export USER_MODULES := $(shell pwd)

export DFU_VENDOR := 0x0483
export DFU_PRODUCT := 0xdf11

.DEFAULT_GOAL := sketch

all: sketch

%:
	$(MAKE) -f $(LIB_MAPLE_HOME)/Makefile $@

DfuSeDl:	build/$(TARGET).bin
	dfu-util -a 0 -d $(DFU_VENDOR):$(DFU_PRODUCT) -s 0x08000000:leave -D $<
