# Note: LIB_MAPLE_HOME needs to be defined in your environment 

# set number of sensors: 16, 32, 48, 64, 80, 96, 112, 128, 144, 160
export SENSORS ?= 96

# set firmware version
export VERSION_MAJOR ?= 0
export VERSION_MINOR ?= 2
export VERSION_PATCH ?= 0

# set revision of board design: 3, 4
export REVISION ?= 4

# set STM32F303Cx chip version: F303CB, F303CC
export BOARD := F303CB
export MEMORY_TARGET := jtag
export USER_MODULES := $(shell pwd)

# set DfuSe USB vendor and product IDs
export USB_VENDOR := 0x0483
export USB_PRODUCT := 0xdf11

RELEASE := 0x0001
VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)-$(REVISION)
BIN := build/$(BOARD).bin
DFU := build/chimaera_S$(SENSORS)-$(VERSION).dfu

.PHONY: dfu reset update download release
.DEFAULT_GOAL := sketch

all: sketch

%:
	$(MAKE) -f $(LIB_MAPLE_HOME)/Makefile $@

$(BIN): sketch

dfu: $(DFU)

$(DFU): $(BIN)
	dfuse_pack \
		-o $@ \
		-f 0x0001 \
		-v $(USB_VENDOR) \
		-p $(USB_PRODUCT) \
			-n 'Chimaera S'$(SENSORS)' '$(VERSION)'. Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch). Released under zlib/libpng License. By Open Music Kontrollers (http://open-music-kontrollers.ch/chimaera/).' \
			-m 0x08000000 -i $< \
			-a 0

reset:
	oscsend osc.udp://255.255.255.255:4444 /reset/flash i $(shell echo $$[RANDOM])
	sleep 1

update:	$(IMAGE) reset
	dfu-util -a 0 -s :leave -D $<

download:	$(BIN) reset
	dfu-util -a 0 -d $(USB_VENDOR):$(USB_PRODUCT) -s 0x08000000:leave -D $<

release:
	./releasor
