CFLAGS = -g
LDFLAGS = -g
SOURCES = jedec.c machxo.c main.c
INCLUDES = jedec.h machxo.h

OBJS = jedec.o machxo.o main.o

PROG = prog_machxo

$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROG)

main.o : $(INCLUDES)
jedec.o : jedec.h
machxo.o : machxo.h
