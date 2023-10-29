AM_CFLAGS = -fPIC
AM_CPPFLAGS = -I$(top_srcdir)/src/common

lib_LTLIBRARIES = \
    flux_pwr_manager.la \
    libjob_notification.la 

flux_pwr_manager_la_SOURCES = \
    flux_pwr_manager.c \
    job_data.c \
    node_power_profile.c \
    dynamic_job_map.c \
    job_power_util.c \
    device_power_info.c \
    json_utility.c \
    power_util.c

flux_pwr_manager_la_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    $(FLUX_CORE_CFLAGS) \
    $(VARIORUM_INCLUDE)

flux_pwr_manager_la_LIBADD = \
    $(FLUX_CORE_LIBS) \
    $(VARIORUM_LIB) \
    $(top_builddir)/src/common/libcommon.la \
    $(top_builddir)/src/flux_pwr_manager/power_policies/libpower_policy.la \
    $(top_builddir)/src/flux_pwr_manager/node_manager/libnode_manager.la

flux_pwr_manager_la_LDFLAGS = \
    $(fluxpwr_ldflags) \
    -module

# Definition for job_notification shared library
libjob_notification_la_SOURCES = job_notification.c
# libjob_notification_la_LDFLAGS = -module -shared
libjob_notification_la_LIBADD = $(FLUX_CORE_LIBS) $(VARIORUM_LIB) $(top_builddir)/src/common/libcommon.la
libjob_notification_la_CPPFLAGS = $(AM_CPPFLAGS) $(FLUX_CORE_CFLAGS)
