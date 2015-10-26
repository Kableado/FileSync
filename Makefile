
ifneq (,$(findstring MINGW,$(shell uname -s)))
	RES_APP = filesync.exe
	BUILDDIR = build-mingw
else
	RES_APP = filesync
	BUILDDIR = build-linux
endif


CC    = gcc
RM    = rm -f
ECHO  = echo
MKDIR = mkdir

HEADS = \
		src/util.h \
		src/crc.h \
		src/fileutil.h \
		src/filenode.h

OBJS_BASE =  \
		$(BUILDDIR)/util.o \
		$(BUILDDIR)/crc.o \
		$(BUILDDIR)/fileutil.o \
		$(BUILDDIR)/filenode.o \
		$(BUILDDIR)/filenodecmp.o

OBJS_APP = \
		$(OBJS_BASE) \
		$(BUILDDIR)/main.o

CFLAGS   = -g
LIBS     = -lm


DO_CC=@$(ECHO) "CC: $@" ;\
	$(CC) $(CFLAGS) -o $@ -c $<

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

$(BUILDDIR)/filenode.o: src/filenode.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/filenodecmp.o: src/filenodecmp.c $(HEADS)
	$(DO_CC)


$(BUILDDIR)/main.o: src/main.c $(HEADS)
	$(DO_CC)


$(RES_APP): $(BUILDDIR) $(OBJS_APP)
	@$(ECHO) "LINK: $@"
	@$(CC) $(OBJS_APP) \
		-o $(RES_APP) $(LIBS)


