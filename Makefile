MYRPC_DEB_DIR    := $(CURDIR)/build/deb
MYSYSLOG_DEB_DIR := $(CURDIR)/../libmysyslog/build/deb
REPO_DIR ?= $(CURDIR)/../myRPC-repo

.PHONY: all clean deb repo

all:
	$(MAKE) -C source/libmysyslog
	$(MAKE) -C source/myRPC-client
	$(MAKE) -C source/myRPC-server

clean:
	$(MAKE) -C source/libmysyslog clean
	$(MAKE) -C source/myRPC-client clean
	$(MAKE) -C source/myRPC-server clean
	rm -rf build-deb
	rm -rf build
	rm -rf deb
	rm -rf repo

deb:
	$(MAKE) -C source/libmysyslog deb
	$(MAKE) -C source/myRPC-server deb
	$(MAKE) -C source/myRPC-client deb

repo:
	mkdir -p repo
	cp deb/*.deb repo/
	dpkg-scanpackages repo /dev/null | gzip -9c > repo/Packages.gz
