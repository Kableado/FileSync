
CC=gcc
RM=rm -f
ECHO=echo
MKDIR=mkdir

HEADS = util.h crc.h fileutil.h filenode.h
OBJS_BASE =  \
		$(BUILDDIR)/util.o \
		$(BUILDDIR)/crc.o \
		$(BUILDDIR)/fileutil.o \
		$(BUILDDIR)/filenode.o \
		$(BUILDDIR)/filenodecmp.o
OBJS_APP = \
		$(OBJS_BASE) \
		$(BUILDDIR)/main.o

RES_APP=filesync

CFLAGS = -g
LIBS =  -lm
BUILDDIR = build



DO_CC=@$(ECHO) "CC: $@" ;\
	$(CC) $(CFLAGS) -o $@ -c $<

all: $(RES_APP)


clean:
	$(RM) $(OBJS_BASE)
	$(RM) $(OBJS_APP)

$(BUILDDIR):
	@-$(MKDIR) $(BUILDDIR)



$(BUILDDIR)/util.o: util.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/crc.o: crc.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/fileutil.o: fileutil.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/filenode.o: filenode.c $(HEADS)
	$(DO_CC)

$(BUILDDIR)/filenodecmp.o: filenodecmp.c $(HEADS)
	$(DO_CC)


$(BUILDDIR)/main.o: main.c $(HEADS)
	$(DO_CC)


$(RES_APP): $(BUILDDIR) $(OBJS_APP)
	@$(ECHO) "LINK: $@"
	@$(CC) $(OBJS_APP) \
		-o $(RES_APP) $(LIBS)


