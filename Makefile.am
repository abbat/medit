ACLOCAL_AMFLAGS = -I m4 $(ACLOCAL_FLAGS)

SUBDIRS = po po-gsv api doc moo

EXTRA_DIST =				\
	tools/genenums.py		\
	tools/glade2c.py		\
	tools/xml2h.py

DISTCHECK_CONFIGURE_FLAGS =
if MOO_DEV_MODE
DISTCHECK_CONFIGURE_FLAGS += MAKEFLAGS=-j3
endif
if MOO_STRICT_MODE
DISTCHECK_CONFIGURE_FLAGS += --enable-strict
endif

CLEANFILES =

test:
	$(MAKE) $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) check
	$(MAKE) $(AM_MAKEFLAGS) uninstall
	$(MAKE) $(AM_MAKEFLAGS) check
	$(MAKE) $(AM_MAKEFLAGS) install
	$(MAKE) $(AM_MAKEFLAGS) installcheck
fullcheck:
	$(MAKE) $(AM_MAKEFLAGS) test
	$(MAKE) $(AM_MAKEFLAGS) distcheck

@MOO_PO_SUBDIRS_RULE@
