# run `make DEF=...` to add extra defines
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lm -pthread
SRCS := $(wildcard *.c)
DEFINES := $(DEF) -D_XOPEN_SOURCE=1111
DEFINES += -DEBUG
#OBJDIR := mk
CFLAGS += -O2 -Wall -Werror -Wextra -Wno-trampolines -std=gnu99
#OBJS := $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.o))
#DEPS := $(OBJS:.o=.d)
CC = gcc

all : sbig340 daemon client
# $(OBJDIR)

sbig340 : $(SRCS)
	@echo -e "\t\tLD sbig340"
	$(CC)  $(CFLAGS) $(DEFINES) $(LDFLAGS) $(shell pkg-config --libs cfitsio) -ltiff  $(SRCS) -o sbig340

daemon : $(SRCS)
	@echo -e "\t\tLD daemon"
	$(CC) -DDAEMON $(CFLAGS) $(DEFINES) $(LDFLAGS) $(SRCS) -o daemon

client : $(SRCS)
	@echo -e "\t\tLD client"
	$(CC) -DCLIENT $(CFLAGS) $(DEFINES) $(LDFLAGS) $(shell pkg-config --libs cfitsio) -ltiff  $(SRCS) -o client

#$(OBJDIR):
#	mkdir $(OBJDIR)

#ifneq ($(MAKECMDGOALS),clean)
#-include $(DEPS)
#endif

#$(OBJDIR)/%.o: %.c
#	@echo -e "\t\tCC $<"
#	$(CC) -MD -c $(LDFLAGS) $(CFLAGS) $(DEFINES) -o $@ $<

#clean:
#	@echo -e "\t\tCLEAN"
#	@rm -f $(OBJS) $(DEPS)
#	@rmdir $(OBJDIR) 2>/dev/null || true

#xclean: clean
#	@rm -f $(PROGRAM)

gentags:
	CFLAGS="$(CFLAGS) $(DEFINES)" geany -g $(PROGRAM).c.tags *[hc] 2>/dev/null

.PHONY: gentags
#clean xclean
