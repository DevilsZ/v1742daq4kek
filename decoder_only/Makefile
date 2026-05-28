# Compiler
CXX         = g++
CXXFLAGS    = -Wall -g -O2 -std=c++11 -pthread -Iinc

# Linker
LD          = g++

# Program names
PROGS       = dat2root rootanalyzer

# ROOT settings
ROOTCFLAGS  := $(shell root-config --cflags)
ROOTLIBS    := $(shell root-config --libs)

# Object files
OBJS_COMMON = inc/v1742Event.o

OBJS_DAT2ROOT = dat2root.o $(OBJS_COMMON)
OBJS_ROOTANALYZER = rootanalyzer.o $(OBJS_COMMON)

# Default target
all: $(PROGS)

# Compile rule
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(ROOTCFLAGS) -c $< -o $@

# Compile v1742Event separately
inc/v1742Event.o: inc/v1742Event.cpp inc/v1742Event.hpp inc/headers.h
	$(CXX) $(CXXFLAGS) $(ROOTCFLAGS) -c inc/v1742Event.cpp -o inc/v1742Event.o

# Link programs
dat2root: $(OBJS_DAT2ROOT)
	$(LD) $(OBJS_DAT2ROOT) $(ROOTLIBS) -o $@

rootanalyzer: $(OBJS_ROOTANALYZER)
	$(LD) $(OBJS_ROOTANALYZER) $(ROOTLIBS) -o $@

# Cleanup
clean:
	rm -f *.o inc/*.o $(PROGS)
