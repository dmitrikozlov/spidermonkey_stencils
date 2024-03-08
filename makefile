SMONKEY=../mozilla-unified/obj-x86_64-pc-linux-gnu/dist
SMONKEYLIB=mozjs-124a1

#SMONKEY=../firefox-115.8.0/obj-debug-x86_64-pc-linux-gnu/dist
#SMONKEYLIB=mozjs-115

CC=g++
CFLAGS=-std=c++17 -D DEBUG -g -I $(SMONKEY)/include
#ODIR=obj
OBJS=boilerplate.o stencils.o

all: stc

stc: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -l$(SMONKEYLIB) -L $(SMONKEY)/bin -o stc

boilerplate.o: boilerplate.cpp boilerplate.h
	$(CC) $(CFLAGS) -c boilerplate.cpp

stencils.o: stencils.cpp
	$(CC) $(CFLAGS) -c stencils.cpp

.PHONY: clean

test: stc
	LD_LIBRARY_PATH=$(SMONKEY)/bin ./stc

gdb: stc
	LD_LIBRARY_PATH=$(SMONKEY)/bin gdb ./stc

clean:
	rm *.o stc

