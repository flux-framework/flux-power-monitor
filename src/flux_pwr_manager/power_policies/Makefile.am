AM_CFLAGS = -fPIC
AM_CPPFLAGS = -I$(top_srcdir)/src/common -I$(top_srcdir)/src/flux_pwr_manager
noinst_LTLIBRARIES=libpower_policy.la
libpower_policy_la_SOURCES = \
														 power_policy_manager.h\
														 uniform_power_policy.c \
														 uniform_power_strategy.c \
														 dynamic_power_policy.c \
														 dynamic_power_strategy.c
libpower_policy_la_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    $(FLUX_CORE_CFLAGS) \
    $(VARIORUM_INCLUDE)

libpower_policy_la_LIBADD = \
    $(FLUX_CORE_LIBS) \
    $(VARIORUM_LIB) 


