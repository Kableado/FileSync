# SPDX-License-Identifier: MIT
# Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez

IsMinGW := $(findstring MSYS,$(shell uname -s))$(findstring MINGW,$(shell uname -s))
ifneq (,$(IsMinGW))
	RES_APP  := filesync.exe
	BUILDDIR := build-$(shell gcc -dumpmachine)
else
	RES_APP  := filesync
	BUILDDIR := build-$(shell gcc -dumpmachine)
endif
VERBOSE_BUILD=false

CC    := gcc
RM    := rm -f
ECHO  := echo
MKDIR := mkdir

HEADS := \
		src/util.h \
		src/crc.h \
		src/fileutil.h \
		src/parameteroperation.h \
		src/filenode.h \
		src/actionfilenode.h \
		src/actionfilenodesync.h \
		src/actionfilenodecopy.h

OBJS_BASE :=  \
		$(BUILDDIR)/util.o \
		$(BUILDDIR)/crc.o \
		$(BUILDDIR)/fileutil.o \
		$(BUILDDIR)/parameteroperation.o \
		$(BUILDDIR)/filenode.o \
		$(BUILDDIR)/actionfilenode.o \
		$(BUILDDIR)/actionfilenodesync.o \
		$(BUILDDIR)/actionfilenodecopy.o

OBJS_APP := \
		$(OBJS_BASE) \
		$(BUILDDIR)/main.o

CFLAGS := -g
LIBS   := -lm


ifeq ($(VERBOSE_BUILD),true)
	DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
	DO_CXX=$(CXX) $(CFLAGS) -o $@ -c $<
else
	DO_CC=@$(ECHO) "CC: $@" ;\
		$(CC) $(CFLAGS) -o $@ -c $<
	DO_CXX=@$(ECHO) "CXX: $@" ;\
		$(CXX) $(CFLAGS) -o $@ -c $<
endif


all: $(RES_APP)


clean:
	$(RM) $(RES_APP)
	$(RM) $(OBJS_APP)

$(BUILDDIR):
	@-$(MKDIR) $(BUILDDIR)



$(BUILDDIR)/util.o: src/util.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/crc.o: src/crc.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/fileutil.o: src/fileutil.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/parameteroperation.o: src/parameteroperation.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/filenode.o: src/filenode.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/actionfilenode.o: src/actionfilenode.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/actionfilenodesync.o: src/actionfilenodesync.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/actionfilenodecopy.o: src/actionfilenodecopy.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/main.o: src/main.c $(HEADS)
	$(DO_CC)


$(RES_APP): $(BUILDDIR) $(OBJS_APP)
	@$(ECHO) "LINK: $@"
	@$(CC) $(OBJS_APP) \
		-o $(RES_APP) $(LIBS)

