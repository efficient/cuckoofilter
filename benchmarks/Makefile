# Uncomment one of the following to switch between debug and opt mode
OPT = -O3 -DNDEBUG
#OPT = -g -ggdb

CXXFLAGS += -fno-strict-aliasing -Wall -std=c++11 -I. -I../src/ $(OPT) -march=core-avx2

LDFLAGS+= -Wall -lpthread -lssl -lcrypto

HEADERS = $(wildcard ../src/*.h) *.h

SRC = ../src/hashutil.cc

.PHONY: all

BINS = conext-table3.exe conext-figure5.exe bulk-insert-and-query.exe

all: $(BINS)

clean:
	/bin/rm -f $(BINS)

%.exe: %.cc ${HEADERS} ${SRC} Makefile
	$(CXX) $(CXXFLAGS) $< -o $@ $(SRC) $(LDFLAGS)
