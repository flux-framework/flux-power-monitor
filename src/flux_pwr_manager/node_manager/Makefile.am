AM_CFLAGS = -fPIC @PTHREAD_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/src/common -I$(top_srcdir)/src/flux_pwr_manager $(FFTW_LIBS)

noinst_LTLIBRARIES = libnode_manager.la

libnode_manager_la_SOURCES = \
    node_data.h \
    node_manager.c \
		node_util.c \
    power_monitor.c\
		node_job_info.c \
		fft_predictor.c

libnode_manager_la_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    $(FLUX_CORE_CFLAGS) \
    $(VARIORUM_INCLUDE)

libnode_manager_la_LIBADD = \
    $(FLUX_CORE_LIBS) \
    $(VARIORUM_LIB) \
    @PTHREAD_LIBS@ \
		$(FFTW_LIBS)


