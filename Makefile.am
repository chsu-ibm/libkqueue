
# FIXME - Only supports Linux for now

lib_LTLIBRARIES = libkqueue.la
kqincludedir = $(includedir)/kqueue/sys
kqinclude_HEADERS = include/sys/event.h
dist_man_MANS = kqueue.2

libkqueue_la_CFLAGS = -I./src/common -I./include -g -D_XOPEN_SOURCE=600 -D__thread= -Dinline=

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
       src/common/alloc.h \
       src/common/debug.h \
       src/common/private.h \
       src/common/queue.h \
       src/common/tree.h

libkqueue_la_LIBADD = 

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libkqueue.pc

check_PROGRAMS = kqtest

TESTS = kqtest

kqtest_SOURCES = \
       test/main.c \
       test/kevent.c \
       test/test.c \
       test/proc.c \
       test/read.c \
       test/signal.c \
       test/timer.c \
       test/vnode.c \
       test/user.c \
       test/common.h

kqtest_CFLAGS = -g -I./include -I./test -D_XOPEN_SOURCE=600

kqtest_LDADD = libkqueue.la


