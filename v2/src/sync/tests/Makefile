CXX=g++
CFLAGS=-Wall -Werror -Wextra
CFLAGS+=-Wno-error=write-strings
CFLAGS+=-g -O0
CFLAGS+=-I . -I ../ -I ../../ -I ../../device -I ../../../include
#CFLAGS+=-DDEBUG
#CFLAGS+=-DNDEBUG
CFLAGS+=-DCUT -DREBALANCE
CFLAGS+=-DEXPORT_PDF
CFLAGS+=
LDFLAGS+=

all:
	$(CXX) $(CFLAGS) tests.cpp $(LDFLAGS)
