AM_CFLAGS = -fPIC
AM_CPPFLAGS = -I$(top_srcdir)/src/common

lib_LTLIBRARIES = \
    pwr_mgr.la \
    libjob_notification.la 

pwr_mgr_la_SOURCES = \
    pwr_mgr.c \
		cluster_pwr_mgr.c \
		job_pwr_mgr.c \
		json_utility.c \
		job_hash.c 

pwr_mgr_la_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    $(FLUX_CORE_CFLAGS) \
    $(VARIORUM_INCLUDE)

pwr_mgr_la_LIBADD = \
    $(FLUX_CORE_LIBS) \
    $(VARIORUM_LIB) \
    $(top_builddir)/src/common/libcommon.la \
    $(top_builddir)/src/flux_pwr_manager/power_policies/libpower_policy.la \
    $(top_builddir)/src/flux_pwr_manager/node_manager/libnode_manager.la

pwr_mgr_la_LDFLAGS = \
    $(fluxpwr_ldflags) \
		-lczmq \
    -module

# Definition for job_notification shared library
libjob_notification_la_SOURCES = job_notification.c
# libjob_notification_la_LDFLAGS = -module -shared
libjob_notification_la_LIBADD = $(FLUX_CORE_LIBS) $(VARIORUM_LIB) $(top_builddir)/src/common/libcommon.la
libjob_notification_la_CPPFLAGS = $(AM_CPPFLAGS) $(FLUX_CORE_CFLAGS)
