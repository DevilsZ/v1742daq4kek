CC          = g++ -Wall -g -O2 -std=c++11 -pthread
LN	    = g++ -Wall
INCLUDE	    = /root/driver/driver/universeII-Rosewood-v1.5.0/vmelib-4.x
LIBS	    = -L//root/driver/driver/universeII-Rosewood-v1.5.0/vmelib-4.x -lvmelib 
PROGS       = v1742-seq check_v1742 v1742-std decoder

%.o:%.cpp
	$(CC) -I$(INCLUDE) -c $<

all: $(PROGS)

v1742-seq: v1742-seq.o
	$(LN) v1742-seq.o $(LIBS) -o v1742-seq -pthread

v1742-std: v1742-std.o
	$(LN) v1742-std.o $(LIBS) -o v1742-std

check_v1742: check_v1742.o
	$(LN) check_v1742.o $(LIBS) -o check_v1742

decoder: decoder.o
	$(LN) decoder.o $(LIBS) -o decoder

clean:
	rm -f *.o $(PROGS)
