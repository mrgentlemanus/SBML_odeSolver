# -----------------------------------------------------------------
# $Revision: 1.1 $
# $Date: 2005/10/27 13:37:27 $
# -----------------------------------------------------------------
# Programmer(s): Radu Serban and Aaron Collier @ LLNL
# -----------------------------------------------------------------
# Copyright (c) 2002, The Regents of the University of California.
# Produced at the Lawrence Livermore National Laboratory.
# All rights reserved.
# For details, see sundials/cvodes/LICENSE.
# -----------------------------------------------------------------
# Makefile for CVODES module
#
# cvodes/source/Makefile.  Generated from Makefile.in by configure.
# -----------------------------------------------------------------

SHELL = /bin/bash

srcdir       = .
builddir     = .
abs_builddir = /cygdrive/h/SFORGE_CVS/SBML_odeSolver/Win32/WinCVODE/sundials/cvodes/source
top_builddir = ../../
prefix       = /cygdrive/h/SFORGE_CVS/SBML_odeSolver/Win32/WinCVODE/sundials
exec_prefix  = ${prefix}
includedir   = ${prefix}/include
libdir       = ${exec_prefix}/lib

INSTALL        = /usr/bin/install -c
INSTALL_LIB    = ${INSTALL}
INSTALL_HEADER = ${INSTALL} -m 644

LIBTOOL      = $(SHELL) $(top_builddir)/libtool
LIBTOOL_DEPS = config/ltmain.sh

CPP      = /usr/bin/cc -E
CPPFLAGS = 
CC       = /usr/bin/cc
CFLAGS   = -g -O2
LDFLAGS  = 
LIBS     = -lm 

SHARED_INC_DIR = $(srcdir)/../../shared/include
CVODES_INC_DIR = $(srcdir)/../include

CVODES_LIB       = libsundials_cvodes.la
CVODES_LIB_FILES = cvodes.lo cvodesio.lo cvodea.lo cvband.lo cvdense.lo cvdiag.lo cvspgmr.lo cvbandpre.lo cvbbdpre.lo

OBJECT_FILES = cvodes.o cvodesio.o cvodea.o cvband.o cvdense.o cvdiag.o cvspgmr.o cvbandpre.o cvbbdpre.o

all: update $(CVODES_LIB_FILES)

update:
	${INSTALL_HEADER} ${top_builddir}/config/sundials_config.h ${builddir}

install: update $(CVODES_LIB_FILES)
	$(LIBTOOL) --mode=link $(CC) $(CFLAGS) -o $(CVODES_LIB) $(CVODES_LIB_FILES) -rpath $(libdir) $(LDFLAGS) $(LIBS) -avoid-version
	$(LIBTOOL) --mode=install $(INSTALL_LIB) $(CVODES_LIB) $(libdir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvodes.h $(includedir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvodea.h $(includedir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvdense.h $(includedir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvband.h $(includedir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvdiag.h $(includedir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvspgmr.h $(includedir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvbandpre.h $(includedir)
	$(INSTALL_HEADER) $(CVODES_INC_DIR)/cvbbdpre.h $(includedir)

clean:
	$(LIBTOOL) --mode=clean rm -f $(CVODES_LIB)
	rm -f $(CVODES_LIB_FILES)
	rm -f $(OBJECT_FILES)
	rm -f sundials_config.h

uninstall: clean
	$(LIBTOOL) --mode=uninstall rm -f $(libdir)/$(CVODES_LIB)
	rm -f $(includedir)/cvodes.h
	rm -f $(includedir)/cvodea.h
	rm -f $(includedir)/cvdense.h
	rm -f $(includedir)/cvband.h
	rm -f $(includedir)/cvdiag.h
	rm -f $(includedir)/cvspgmr.h
	rm -f $(includedir)/cvbandpre.h
	rm -f $(includedir)/cvbbdpre.h

cvodes.lo: $(srcdir)/cvodes.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvodes.c
cvodesio.lo: $(srcdir)/cvodesio.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvodesio.c
cvodea.lo: $(srcdir)/cvodea.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvodea.c
cvdense.lo: $(srcdir)/cvdense.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvdense.c
cvband.lo: $(srcdir)/cvband.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvband.c
cvdiag.lo: $(srcdir)/cvdiag.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvdiag.c
cvspgmr.lo: $(srcdir)/cvspgmr.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvspgmr.c
cvbandpre.lo: $(srcdir)/cvbandpre.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvbandpre.c
cvbbdpre.lo: $(srcdir)/cvbbdpre.c
	$(LIBTOOL) --mode=compile $(CC) $(CPPFLAGS) -I. -I$(SHARED_INC_DIR) -I$(CVODES_INC_DIR) $(CFLAGS) -c $(srcdir)/cvbbdpre.c

libtool: $(top_builddir)/$(LIBTOOL_DEPS)
	@cd $(top_builddir) ;                 \
	 $(SHELL) ./config.status --recheck ; \
	 cd $(abs_builddir)
