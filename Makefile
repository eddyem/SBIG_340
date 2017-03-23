# run `make DEF=...` to add extra defines
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lm -pthread
LDIMG   := $(shell pkg-config --libs libraw) -lgd $(shell pkg-config --libs cfitsio) -ltiff
SRCS := $(wildcard *.c)
DEFINES := $(DEF) -D_GNU_SOURCE  -D_XOPEN_SOURCE=1111
#DEFINES += -DEBUG
CFLAGS += -Wall -Wextra -O2
CFLAGS += $(shell pkg-config --cflags libraw)
OBJS := $(SRCS:%.c=%.o)
CC  = gcc
CPP = g++

all : sbig340_standalone sbig340_daemon sbig340_client

debayer.o : debayer.cpp
	@echo -e "\t\tG++ debayer"
	$(CPP) $(CFLAGS) $(DEFINES) debayer.cpp -c

sbig340_standalone : $(SRCS) debayer.o
	@echo -e "\t\tBuild standalone"
	$(CC) $(CFLAGS) -std=gnu99 $(DEFINES) $(LDFLAGS) $(LDIMG) $(SRCS) debayer.o -o $@

sbig340_daemon : $(SRCS)
	@echo -e "\t\tBuild daemon"
	$(CC) -DDAEMON $(CFLAGS) -std=gnu99 $(DEFINES) $(LDFLAGS) $(SRCS) -o $@

sbig340_client : $(SRCS) debayer.o
	@echo -e "\t\tBuild client"
	$(CC) -DCLIENT $(CFLAGS) -std=gnu99 $(DEFINES) $(LDFLAGS) $(LDIMG) $(SRCS) debayer.o -o $@

clean:
	@echo -e "\t\tCLEAN"
	@rm -f $(OBJS) debayer.o

xclean: clean
	@echo -e "\t\tRM binaries"
	@rm -f sbig340_standalone sbig340_daemon sbig340_client

gentags:
	CFLAGS="$(CFLAGS) $(DEFINES)" geany -g sbig340.c.tags *.[hc] *.cpp 2>/dev/null

.PHONY: gentags clean xclean
