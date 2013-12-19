# Note: LIB_MAPLE_HOME needs to be defined in your environment 

# set number of sensors: 16, 32, 48, 64, 80, 96, 112, 128, 144, 160
export SENSORS := 96 # 6 sensor units

# set revision of board design: 3, 4
export REVISION := 3

# set STM32F303Cx chip version: F303CB, F303CC
export BOARD := F303CB
export MEMORY_TARGET := jtag
export USER_MODULES := $(shell pwd)

# set DfuSe USB vendor and product IDs
export DFU_VENDOR := 0x0483
export DFU_PRODUCT := 0xdf11

.DEFAULT_GOAL := sketch

all: sketch

%:
	$(MAKE) -f $(LIB_MAPLE_HOME)/Makefile $@

download:	build/$(BOARD).bin
	oscsend osc.udp://255.255.255.255:4444 /chimaera/reset/flash i 13
	sleep 1
	dfu-util -a 0 -d $(DFU_VENDOR):$(DFU_PRODUCT) -s 0x08000000:leave -D $<
