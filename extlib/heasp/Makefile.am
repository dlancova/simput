AM_CFLAGS =-I@top_srcdir@/extlib/heainit
AM_CFLAGS+=-I@top_srcdir@/extlib/heaio
AM_CFLAGS+=-I@top_srcdir@/extlib/heautils
AM_CFLAGS+=-I@top_srcdir@/extlib/cfitsio

########### LIBRARIES #################

# Libraries that are installed in the 'lib' directory.
lib_LTLIBRARIES=libhdsp.la

# Sources:
libhdsp_la_SOURCES=WriteRMF.c WritePHA.c WriteARF.c SPutils.c		\
                  ReturnChannel.c MergeARFRMF.c TransposeRMF.c SPio.c	\
                  RMF.c ReadRMF.c ReadARF.c ReadPHA.c
libhdsp_la_LDFLAGS = -version-info 1:1:0
#libhdsp_la_LIBADD=-L../heautils/.libs/ -lhdutils
libhdsp_la_LIBADD=../heautils/libhdutils.la

############ HEADERS #################

# Define the headers that will be installed in $(includedir):
include_HEADERS=heaspio.h heasp.h
