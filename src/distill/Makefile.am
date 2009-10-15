# $Id$

bin_PROGRAMS = svnsup-distill

noinst_HEADERS = \
	distill.h

svnsup_distill_SOURCES = \
	auth.c \
	distill.c \
	editor.c \
	error.c \
	log.c \
	ra.c \
	txdelta.c

svnsup_distill_CPPFLAGS = \
	-I$(top_builddir)/src/libsvnsup \
	${APR_1_CFLAGS}

svnsup_distill_LDADD = \
	$(top_builddir)/src/libsvnsup/libsvnsup.a \
	${APR_1_LDADD} \
	${SVN_DELTA_1_LIBS} \
	${SVN_CLIENT_1_LIBS} \
	${SVN_RA_1_LIBS}
