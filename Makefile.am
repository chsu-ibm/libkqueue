
# FIXME - Only supports Linux for now

kqincludedir = $(includedir)/kqueue/sys

if INSTALL
    lib_LTLIBRARIES = libkqueue.la
    kqinclude_HEADERS = include/sys/event.h
    dist_man_MANS = kqueue.2
else
    noinst_LTLIBRARIES = libkqueue.la
endif

libkqueue_la_CFLAGS = -I$(top_srcdir)/src/common -I$(top_srcdir)/include -Wall -Wextra -Wno-missing-field-initializers -Werror -g -O2 -std=c99 -D_XOPEN_SOURCE=600 -fvisibility=hidden -D__thread= -Dinline= 

libkqueue_la_SOURCES = \
       src/common/filter.c \
       src/common/knote.c \
       src/common/map.c \
       src/common/kevent.c \
       src/common/kqueue.c \
       src/posix/platform.c \
       src/posix/platform.h \
       src/posix/read.c \
       src/posix/write.c \
       src/posix/user.c \
       src/posix/vnode.c \
       src/posix/signal.c \
       src/posix/timer.c \
       src/posix/posix-kevent.c \
       src/common/alloc.h \
       src/common/debug.h \
       src/common/private.h \
       src/common/queue.h \
       src/common/tree.h

if !BIONIC_LIBC
libkqueue_la_LIBADD = -lpthread -lrt
endif

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libkqueue.pc

check_PROGRAMS = kqtest

TESTS = kqtest

kqtest_SOURCES = \
       test/test-main.c \
       test/test-kevent.c \
       test/test-test.c \
       test/test-proc.c \
       test/test-read.c \
       test/test-signal.c \
       test/test-timer.c \
       test/test-vnode.c \
       test/test-user.c \
       test/common.h

kqtest_CFLAGS = -g -O0 -Wall -Werror -I$(top_srcdir)/include -I$(top_srcdir)/test -I$(builddir) -D_XOPEN_SOURCE=600 

if BIONIC_LIBC
kqtest_LDADD = libkqueue.la
else
kqtest_LDADD = -lpthread -lrt libkqueue.la
endif


