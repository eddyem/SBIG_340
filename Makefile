# run `make DEF=...` to add extra defines
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lm -pthread
SRCS := $(wildcard *.c)
#GCC_GE_4_9_3 := $(shell g++ -dumpversion | gawk '{print $$1>=4.9.3?"1":"0"}')
DEFINES := $(DEF)   -D_XOPEN_SOURCE=1111
#ifeq ($(GCC_GE_4_9_3),1)
    DEFINES += -D_DEFAULT_SOURCE
#else
#	DEFINES += -D_BSD_SOURCE
#endif
#DEFINES += -DEBUG
CFLAGS += -Wall -Wextra -O2 -std=gnu99
CC = gcc

all : sbig340 daemon client

sbig340 : $(SRCS)
	@echo -e "\t\tLD sbig340"
	$(CC)  $(CFLAGS) $(DEFINES) $(LDFLAGS) $(shell pkg-config --libs cfitsio) -ltiff  $(SRCS) -o sbig340

daemon : $(SRCS)
	@echo -e "\t\tLD daemon"
	$(CC) -DDAEMON $(CFLAGS) $(DEFINES) $(LDFLAGS) $(SRCS) -o daemon

client : $(SRCS)
	@echo -e "\t\tLD client"
	$(CC) -DCLIENT $(CFLAGS) $(DEFINES) $(LDFLAGS) $(shell pkg-config --libs cfitsio) -ltiff  $(SRCS) -o client

clean:
	@echo -e "\t\tCLEAN"
	@rm -f sbig340 daemon client

gentags:
	CFLAGS="$(CFLAGS) $(DEFINES)" geany -g $(PROGRAM).c.tags *[hc] 2>/dev/null

.PHONY: gentags clean
