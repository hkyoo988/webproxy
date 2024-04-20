CC = gcc
CFLAGS = -O2 -Wall -I .

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: echoclient

# tiny: tiny.c csapp.o
# 	$(CC) $(CFLAGS) -o tiny tiny.c csapp.o $(LIB)
# echoserveri.o: echoserveri.c
# 	$(CC) $(CFLAGS) -c echoserveri.c
csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

echoclient.o: echoclient.c csapp.o
	$(CC) $(CFLAGS) -c echoclient.c

echoclient: echoclient.o csapp.o
	$(CC) $(CFLAGS) -o tiny echoclient.c csapp.o $(LIB)


# cgi:
# 	(cd cgi-bin; make)

clean:
	rm -f *.o tiny *~
# (cd cgi-bin; make clean)

