CXX=g++
CFLAGS=-Wall -Werror -Wextra
CFLAGS+=-Wno-error=write-strings
CFLAGS+=-g -O0
CFLAGS+=-I . -I ../src -I ../src/impl
#CFLAGS+=-DDEBUG
CFLAGS+=-DNDEBUG
CFLAGS+=-DCUT -DREBALANCE
#CFLAGS+=-DEXPORT_PDF
CFLAGS+=
LDFLAGS+=

all:
	$(CXX) $(CFLAGS) tests.cpp $(LDFLAGS)
