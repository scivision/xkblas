CXX=g++
CFLAGS=-Wall -Werror -Wextra
CFLAGS+=-Wno-error=write-strings
CFLAGS+=-g -O3
CFLAGS+=-I . -I ../ -I ../../ -I ../../device -I ../../../include
CFLAGS+=-DNDEBUG
#CFLAGS+=-DEXPORT_PDF
CFLAGS+=
LDFLAGS+=

all:
	$(CXX) $(CFLAGS) tests.cpp $(LDFLAGS)
