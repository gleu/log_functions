MODULE_big = log_functions
OBJS = log_functions.o

ifdef USE_PGXS
$(warning You can compile this module with PGXS only if you have 9.2+ PostgreSQL headers.)
$(warning Make sure you compile for 9.2+ PostgreSQL servers.)
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/log_functions
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

override CFLAGS += -DINCLUDE_PACKAGE_SUPPORT=0  -I$(top_builddir)/src/pl/plpgsql/src -I$(top_builddir)/src/backend

