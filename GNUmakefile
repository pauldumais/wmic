###########################################################################
#
# This program is part of Zenoss Core, an open source monitoring platform.
# Copyright (C) 2008-2010, Zenoss Inc.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2, or (at your
# option) any later version, as published by the Free Software Foundation.
#
# For complete information please visit: http://www.zenoss.com/oss/
#
###########################################################################
CPP=gcc -E -ffreestanding

all: install

install: pywmi-installed 
build: pywmi-build 

ifeq ($(PYTHON),)
PYTHON=python
endif

PYLIBDIR=	$(ZENHOME)/lib/python
ZENBIN=		$(ZENHOME)/bin
PYINCLUDE=	$(shell $(PYTHON) pyinclude.py)
VERSION:=$(shell cd pysamba ; python -c 'import version as v; print v.VERSION')

LIBSUFFIX=so.0
ifeq ($(shell uname), Darwin)
LIBSUFFIX=dylib
endif

pywmi-build:
	cd Samba/source ;				\
	./autogen.sh ;					\
	CPPFLAGS="-I$(PYINCLUDE)"			\
	./configure --without-readline --enable-debug ;	\
	$(MAKE) proto bin/wmic bin/winexe libraries ; 		\
	touch $@

pywmi-installed: $(DESTDIR)$(PYLIBDIR) $(DESTDIR)$(ZENBIN) pywmi-build
	cd Samba/source ; \
	cp bin/winexe $(DESTDIR)$(ZENBIN) ; \
	cp bin/wmic $(DESTDIR)$(ZENBIN) ; \
	cp bin/shared/*async_wmi_lib.$(LIBSUFFIX)* $(DESTDIR)$(PYLIBDIR)
	rm -rf $(DESTDIR)$(PYLIBDIR)/pysamba
	cp -r pysamba $(DESTDIR)$(PYLIBDIR)

$(DESTDIR)$(ZENBIN) $(DESTDIR)$(PYLIBDIR):
	mkdir -p $@

clean:
	-cd Samba/source && make distclean
	rm -f pysamba/version.pyc
	rm -f Samba/source/pywmi-build
	rm -f Samba/source/include/config_tmp.h.in
	rm -f Samba/source/winexe/libwincmd.dylib.0.80
	rm -f Samba/source/wmi/libwmiclient.dylib.1.3.14
	rm -f Samba/source/build/smb_build/config.pm
	rm -f Samba/source/heimdal/lib/gssapi/gkrb5_err.c
	rm -f Samba/source/heimdal/lib/gssapi/gkrb5_err.h
	rm -f Samba/source/configure
	rm -f Samba/source/extra_cflags.txt
	rm -f Samba/source/version.h 
	rm -f Samba/source/bin/shared/* 
	rm -f Samba/source/bin/static/* 
	rm -f Samba/source/bin/pkgconfig/* 
	rm -f Samba/source/heimdal/lib/des/hcrypto

tarball:
	-svn rm -m 'cleanup' http://dev.zenoss.org/svn/tags/wmi-$(VERSION)
	svn cp -m "tagging wmi-$(VERSION)" http://dev.zenoss.org/svn/trunk/wmi http://dev.zenoss.org/svn/tags/wmi-$(VERSION)
	svn export http://dev.zenoss.org/svn/tags/wmi-$(VERSION)
	tar -cjf ../wmi-$(VERSION).tar.bz2 wmi-$(VERSION)
	rm -rf wmi-$(VERSION)

.PHONY: tarball clean pywmi-installed pywmi-build
