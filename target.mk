[].SUFFIXES:

ifndef PLATFORM
PLATFORM := unix
export PLATFORM
endif

OBJDIR := _$(PLATFORM)

MAKETARGET = \
	$(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile SRCDIR=$(CURDIR)/src $(MAKECMDGOALS)

.PHONY: $(OBJDIR)
$(OBJDIR):
	+@[ -d $@ ] || mkdir -p $@
	+@$(MAKETARGET)

Makefile : ;
%.mk :: ;

% :: $(OBJDIR) ;

.PHONY: clean
clean:
	rm -rf $(OBJDIR)
