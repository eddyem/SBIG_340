# run `make DEF=...` to add extra defines,
# 		conditional compile flags:
# NOLIBRAW=1 - not use libraw & libgd
# NOTIFF     - not use libtiff
# NOCFITSIO  - not use cfitsio
#
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lm -pthread
LDIMG   :=
SRCS    := $(wildcard *.c)
DEFINES := $(DEF) -D_GNU_SOURCE  -D_XOPEN_SOURCE=1111
DEFINES += -DEBUG
CFLAGS += -Wall -Wextra -O2
OBJS := $(SRCS:%.c=%.o)
CC  = gcc
CPP = g++

TARGETS := sbig340_daemon

ifndef NOLIBRAW
	LDIMG += $(shell pkg-config --libs libraw) -lgd
	CFLAGS += $(shell pkg-config --cflags libraw)
	DEBAYER := debayer.o
	DEFINES += -DLIBRAW
endif

ifndef NOTIFF
	LDIMG += -ltiff
	DEFINES += -DLIBTIFF
endif

ifndef NOCFITSIO
	LDIMG += $(shell pkg-config --libs cfitsio)
	DEFINES += -DLIBCFITSIO
endif

all : sbig340_daemon sbig340_standalone sbig340_client

debayer.o : debayer.cpp
	@echo -e "\t\tG++ debayer"
	$(CPP) $(CFLAGS) $(DEFINES) debayer.cpp -c

sbig340_standalone : $(SRCS) $(DEBAYER)
	@echo -e "\t\tBuild standalone"
	$(CC) $(CFLAGS) -std=gnu99 $(DEFINES) $(LDFLAGS) $(LDIMG) $(SRCS) $(DEBAYER) -o $@

sbig340_daemon : $(SRCS)
	@echo -e "\t\tBuild daemon"
	$(CC) -DDAEMON $(CFLAGS) -std=gnu99 $(DEFINES) $(LDFLAGS) $(SRCS) -o $@

sbig340_client : $(SRCS) $(DEBAYER)
	@echo -e "\t\tBuild client"
	$(CC) -DCLIENT $(CFLAGS) -std=gnu99 $(DEFINES) $(LDFLAGS) $(LDIMG) $(SRCS) $(DEBAYER) -o $@

clean:
	@echo -e "\t\tCLEAN"
	@rm -f $(OBJS) debayer.o

xclean: clean
	@echo -e "\t\tRM binaries"
	@rm -f sbig340_standalone sbig340_daemon sbig340_client

gentags:
	CFLAGS="$(CFLAGS) $(DEFINES)" geany -g sbig340.c.tags *.[hc] *.cpp 2>/dev/null

.PHONY: gentags clean xclean
