# Compiler
CXX         = g++
CXXFLAGS    = -Wall -g -O2 -std=c++11 -pthread

# Linker
LD          = g++

# Program name
PROGS       = dat2root

# ROOT settings
ROOTCFLAGS  := $(shell root-config --cflags)
ROOTLIBS    := $(shell root-config --libs)

# Compile
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(ROOTCFLAGS) -c $<

# Default target
all: $(PROGS)

# Link
dat2root: dat2root.o
	$(LD) dat2root.o $(ROOTLIBS) -o $@

# Cleanup
clean:
	rm -f *.o $(PROGS)
