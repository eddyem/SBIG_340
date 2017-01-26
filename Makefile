PROGRAM = sbig340
LDFLAGS =
#-fdata-sections -ffunction-sections -Wl,--gc-sections
SRCS = $(wildcard *.c)
DEFINES = $(DEF) -D_XOPEN_SOURCE=1111 -DEBUG
OBJDIR = mk
CFLAGS = -Wall -Werror -Wextra -Wno-trampolines -std=gnu99
OBJS = $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.o))
DEPS = $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.d))
CC = gcc

all : $(PROGRAM)

$(PROGRAM) : $(OBJS)
	@echo -e "\t\tLD $(PROGRAM)"
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(PROGRAM)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJS): $(OBJDIR) $(DEPS)

$(OBJDIR)/%.d: %.c $(OBJDIR)
	$(CC) -MM -MG $< | sed -e 's,^\([^:]*\)\.o[ ]*:,$(@D)/\1.o $(@D)/\1.d:,' >$@

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(OBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) -c $(CFLAGS) $(DEFINES) -o $@ $<

clean:
	@echo -e "\t\tCLEAN"
	@rm -f $(OBJS)
	@rm -f $(DEPS)
	@rmdir $(OBJDIR) 2>/dev/null || true

xclean: clean
	@rm -f $(PROGRAM)

gentags:
	CFLAGS="$(CFLAGS) $(DEFINES)" geany -g $(PROGRAM).c.tags *[hc] 2>/dev/null

.PHONY: gentags clean xclean

