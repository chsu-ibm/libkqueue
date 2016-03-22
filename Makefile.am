
# FIXME - Only supports Linux for now

lib_LTLIBRARIES = libkqueue.la
kqincludedir = $(includedir)/kqueue/sys
kqinclude_HEADERS = include/sys/event.h
dist_man_MANS = kqueue.2

libkqueue_la_CFLAGS = -I./src/common -I./include -D_XOPEN_SOURCE=600 -D__thread= -Dinline= -g

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

libkqueue_la_LIBADD = 

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

kqtest_CFLAGS = -I./include -I./test -D_XOPEN_SOURCE=600 -g

kqtest_LDADD = libkqueue.la


