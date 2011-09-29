pyexec_LTLIBRARIES = _dbus_py.la

AM_CPPFLAGS = -I$(top_srcdir)/include $(DBUS_CFLAGS) $(PYTHON_INCLUDES)
AM_LDFLAGS = -module -avoid-version -export-symbols-regex init_dbus_py $(DBUS_LIBS)

_dbus_py_la_SOURCES = module.c
