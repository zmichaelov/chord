CC = gcc
CFLAGS = -O2
LDFLAGS = -lcrypto
DEBUGFLAG = -ggdb
OBJS = chord.o csapp.o


all: chord
all: CFLAGS += $(DEBUGFLAG)

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

chord.o: chord.c
	$(CC) $(CFLAGS) -c chord.c


chord: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o chord

clean:
	rm -f *~ csapp.o chord chord.o
