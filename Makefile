
UNAME := $(shell uname -a)
CC = g++
#ifeq ($(firstword $(filter Linux,$(UNAME))),Linux)
#  CC = g++
#endif
#ifeq ($(firstword $(filter Darwin,$(UNAME))),Darwin)
#  CC = g++-4.7
#endif

# Uncomment one of the following to switch between debug and opt mode
OPT = -std=c++0x -O3 -DNDEBUG
#OPT = -std=c++0x -g -ggdb

CFLAGS += -Wall -c -I. -I./include -I/usr/include/ -I./filter/ $(OPT)

LDFLAGS+= -Wall -lpthread -lssl -lcrypto

LIBOBJECTS = \
	./filter/hashutil.o \
	./filter/printutil.o \

HEADERS = $(wildcard filter/*.h)

BENCH = bench_filter


PROGRAMS = $(BENCH)

all: $(PROGRAMS)

clean:
	rm -f $(PROGRAMS) */*.o

bench_filter: bench/bench_filter.o $(LIBOBJECTS) 
	$(CC) bench/bench_filter.o $(LIBOBJECTS) $(LDFLAGS) -o $@


%.o: %.cc ${HEADERS} Makefile
	$(CC) $(CFLAGS) $< -o $@

# TODO(gabor): dependencies for .o files
# TODO(gabor): Build library
