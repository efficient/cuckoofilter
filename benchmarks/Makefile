# Uncomment one of the following to switch between debug and opt mode
OPT = -O3 -DNDEBUG
#OPT = -g -ggdb

CXXFLAGS += -fno-strict-aliasing -Wall -std=c++11 -I. -I../src/ $(OPT) -march=native

LDFLAGS+= -Wall -lpthread -lssl -lcrypto

HEADERS = $(wildcard ../src/*.h) *.h

SRC = ../src/hashutil.cc

.PHONY: all

all: conext-table3.exe conext-figure5.exe bulk-insert-and-query.exe

%.exe: %.cc ${HEADERS} ${SRC} Makefile
	$(CXX) $(CXXFLAGS) $< -o $@ $(SRC) $(LDFLAGS)
