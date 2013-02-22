# LIB_MAPLE_HOME needs to be defined in your environment 

export BOARD ?= maple_mini
export MEMORY_TARGET ?= flash
export USER_MODULES := $(shell pwd)

.DEFAULT_GOAL := sketch

all: sketch

%:
	$(MAKE) -f $(LIB_MAPLE_HOME)/Makefile $@