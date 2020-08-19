#
# $Id: Makefile 1111 2009-03-04 07:57:14Z os $
#

OPTIONS  = -D_GNU_SOURCE

CC       = cc
CXX      = c++
CFLAGS   = $(OPTIONS) -g -O2 -Wall -fPIC
CXXFLAGS = $(CFLAGS)
LDFLAGS  = -rdynamic
LIBS     = -ldl
OBJ      = main.o parser.o misc.o
PLUGINS  = input-mmap.so input-gzip.so output-xml.so output-ign.so
SOFLAGS  = -shared

all:	aoppa $(PLUGINS)

aoppa: $(OBJ)
	$(CXX) $(LDFLAGS) -o aoppa $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ) aoppa $(PLUGINS) core

## plugins
input-mmap.so: input-mmap.cc
	$(CXX) $(SOFLAGS) $(CXXFLAGS) -o $@ $?

input-gzip.so: input-gzip.cc
	$(CXX) $(SOFLAGS) $(CXXFLAGS) -o $@ $? -lz

output-xml.so: output-xml.cc
	$(CXX) $(SOFLAGS) $(CXXFLAGS) -o $@ $?

output-ign.so: output-ign.cc
	$(CXX) $(SOFLAGS) $(CXXFLAGS) -o $@ $?
