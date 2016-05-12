# makefile for zos

all: libkqueue.dll libkqueue.a

ifndef INSTALL_LOCATION 
INSTALL_LOCATION=$(HOME)/local/swift
endif

install: all
	@bash -c "[[ -d $(INSTALL_LOCATION) ]] || (echo INSTALL_LOCATION $(INSTALL_LOCATION)  not found; false)"
	@bash -c "[[ -w $(INSTALL_LOCATION) ]] || (echo INSTALL_LOCATION $(INSTALL_LOCATION)  not writ-able ; false)"
	mkdir -p $(INSTALL_LOCATION)/include/
	mkdir -p $(INSTALL_LOCATION)/lib
	cp -r ../include/*   $(INSTALL_LOCATION)/include/
	cp libkqueue.x $(INSTALL_LOCATION)/lib
	cp libkqueue.dll $(INSTALL_LOCATION)/lib
	cp libkqueue.a $(INSTALL_LOCATION)/lib


ifndef SRC_DIR
SRC_DIR=..
endif

ifdef DEBUG
MORE_FLAGS=-DDEBUG=1
else
MORE_FLAGS=
endif

ME:=$(firstword $(MAKEFILE_LIST))

AR=ar -ruv

CC=/c390/archive/zosdev/latest/xlC -F:clang++
LINK=/c390/archive/zosdev/latest/xlC -F:clang++ -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,DLL,LIST,LP64 -W l,XPLINK
LINKEXE=/c390/archive/zosdev/latest/xlC -F:clang++ -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,LIST,LP64 -W l,XPLINK

CC=xlc
LINK=xlC -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,DLL,LIST,LP64 -W l,XPLINK
LINKEXE=xlC -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,LIST,LP64 -W l,XPLINK

CC=/home/jsaad/oss/local/bin/xlc
LINK=$(CC) -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,DLL,LIST,LP64 -W l,XPLINK
LINKEXE=$(CC) -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,LIST,LP64 -W l,XPLINK

CC=/c390/archive/zosdev/latest/xlc
LINK=/c390/archive/zosdev/latest/xlc -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,DLL,LIST,LP64 -W l,XPLINK
LINKEXE=/c390/archive/zosdev/latest/xlc -qlist -V -W l,CALL,REUS=RENT,MAP,XREF,LIST,LP64 -W l,XPLINK

CFLAGS_DLL=-q64 \
-qlist -qsource -qasm \
-qnosearch -qxplink -qnocsect \
-DHAVE_CONFIG_H -I$(SRC_DIR) -I$(SRC_DIR)/src/common -I$(SRC_DIR)/include \
-D_XOPEN_SOURCE=600 \
-D_UNIX03_WITHDRAWN \
-Dinline= \
-g -qdll $(MORE_FLAGS) -qexportall -c -M 

CFLAGS_EXE=-q64 \
-qlist -qsource -qasm \
-qnosearch -qxplink -qnocsect \
-DHAVE_CONFIG_H -I$(SRC_DIR) -I$(SRC_DIR)/src/common -I$(SRC_DIR)/include \
-D_XOPEN_SOURCE=600 \
-D_UNIX03_WITHDRAWN \
-Dinline= \
-g -qdll $(MORE_FLAGS) -c -M 

ASM=as -m GOFF

CFLAGS=$(CFLAGS_DLL)

CSRC_DLL= \
src/common/filter.c \
src/common/knote.c \
src/common/map.c \
src/common/kevent.c \
src/common/kqueue.c \
src/zos/platform.c \
src/zos/read.c \
src/zos/write.c \
src/zos/user.c \
src/zos/vnode.c \
src/zos/signal.c \
src/zos/timer.c \
src/zos/proc.c \
src/zos/posix-kevent.c \
src/zos/tls.c

CSRC_TEST= \
test/test-vnode.c \
test/test-user.c \
test/test-timer.c \
test/test-test.c \
test/test-signal.c \
test/test-read.c \
test/test-write.c \
test/test-proc.c \
test/test-main.c \
test/test-kevent.c

DLLOBJS=$(notdir $(patsubst %.c,%.o,$(CSRC_DLL)))

TESTOBJS=$(notdir $(patsubst %.c,%.o,$(CSRC_TEST)))

ASMOBJS=ceeuopt.o

ALLOBJS=$(DLLOBJS) $(TESTOBJS) $(ASMOBJS)

-include $(patsubst %.o,%.u,$(DLLOBJS))
-include $(patsubst %.o,%.u,$(TESTOBJS))

vpath %.h $(SRC_DIR)/ $(SRC_DIR)/src/common/ $(SRC_DIR)/src/zos/ $(SRC_DIR)/test/ $(SRC_DIR)/zosbuild/
vpath %.c $(SRC_DIR)/ $(SRC_DIR)/src/common/ $(SRC_DIR)/src/zos/ $(SRC_DIR)/test/ 

LE_OPTIONS=\
_CEEUOPT  CSECT \n\
_CEEUOPT  AMODE ANY \n\
_CEEUOPT  RMODE ANY \n\
_         CEEXOPT TEST=(ALL) \n\
_         DC    C'LE RUNTIME OPTIONS' \n\
_         DC    C'Bind with application' \n\
_         END

ceeuopt.o: $(ME)
	@echo "$(LE_OPTIONS)" | /bin/sed -e "s'^ *_''"  |  /bin/awk '{printf "%-80s\n",$$0}' >  $(patsubst %.o,%.asm,$@)
	$(ASM) -a $(patsubst %.o,%.asm,$@) > $(patsubst %.o,%.lst,$@)

%.o: %.c ../config.h $(ME)
	echo "compiling in " $(PWD)
	$(CC) -c $(CFLAGS) $< >$(notdir $(patsubst %.c,%.lst,$<))

%.o: %.asm $(ME)
	echo "assembling in " $(PWD)
	$(ASM) $<

libkqueue.dll libkqueue.x libkqueue.a: $(DLLOBJS) $(CSRC_DLL) $(ME)
	$(LINK) -o libkqueue.dll $(DLLOBJS) > libkqueue.map && dbgld libkqueue.dll
	$(AR) libkqueue.a $(DLLOBJS) >> libkqueue.map

kqtest: $(TESTOBJS) libkqueue.x libkqueue.dll $(ASMOBJS) $(ME)
	$(LINKEXE) -o kqtest $(TESTOBJS) $(ASMOBJS) libkqueue.x > kqtest.map
	dbgld -c ./kqtest

runtest: kqtest libkqueue.dll libkqueue.x $(ME)
	LIBPATH=$(LIBPATH):. ./kqtest

clean:
	-rm $(ALLOBJS) 2>/dev/null
	-rm $(patsubst %.o,%.u,$(ALLOBJS)) 2>/dev/null
	-rm $(patsubst %.o,%.dbg,$(ALLOBJS)) 2>/dev/null
	-rm $(patsubst %.o,%.lst,$(ALLOBJS)) 2>/dev/null
	-rm kqtest kqtest.map kqtest.mdbg  libkqueue.dll libkqueue.map libkqueue.x  2>/dev/null
	-rm CEEDUMP.* 2>/dev/null

$(SRC_DIR)/config.h: $(SRC_DIR)/zosbuild/zosconfig.h
	cp $(SRC_DIR)/zosbuild/zosconfig.h $(SRC_DIR)/config.h
