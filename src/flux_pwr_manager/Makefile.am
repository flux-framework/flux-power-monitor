AM_CFLAGS = -fPIC
AM_CPPFLAGS = -I$(top_srcdir)/src/common
lib_LTLIBRARIES = \
									flux_pwr_manager.la
flux_pwr_manager_la_SOURCES = \
											flux_pwr_manager.c 
flux_pwr_manager_la_CPPFLAGS = \
												$(AM_CPPFLAGS) \
												$(FLUX_CORE_CFLAGS) \
												$(VARIORUM_INCLUDE)
flux_pwr_manager_la_LIBADD = \
											$(FLUX_CORE_LIBS) \
											$(VARIORUM_LIB) \
											$(top_builddir)/src/common/libcommon.la
flux_pwr_manager_la_LDFLAGS = \
																$(fluxpwr_ldflags) \
																	-module
