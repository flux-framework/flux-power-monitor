AM_CFLAGS = -fPIC
AM_CPPFLAGS = -I$(top_srcdir)/src/common
lib_LTLIBRARIES = \
									flux_pwr_monitor.la
flux_pwr_monitor_la_SOURCES = \
											root_node_level_info.h \
											root_node_level_info.c \
											flux_pwr_monitor.c 
flux_pwr_monitor_la_CPPFLAGS = \
												$(AM_CPPFLAGS) \
												$(FLUX_CORE_CFLAGS) \
												$(VARIORUM_INCLUDE)
flux_pwr_monitor_la_LIBADD = \
											$(FLUX_CORE_LIBS) \
											$(VARIORUM_LIB) \
											$(top_builddir)/src/common/libcommon.la \
											-lvariorum
flux_pwr_monitor_la_LDFLAGS = \
																$(fluxpwr_ldflags) \
																	-module
