#!/usr/bin/make -f
include dpf/Makefile.base.mk

all: dgl plugins

dgl:
	$(MAKE) -C dpf/dgl opengl

plugins: dgl
	$(MAKE) all -C plugins/EmberEQ

clean:
	$(MAKE) clean -C dpf/dgl
	$(MAKE) clean -C plugins/EmberEQ
	rm -rf bin build

.PHONY: all dgl plugins clean
