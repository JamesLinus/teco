# CC=clang
CC=gcc
OBJS=chario.o data.o exec0.o exec1.o \
	exec2.o main.o rdcmd.o srch.o subs.o \
	utils.o window.o undo.o
CFLAGS=-DDEBUG -g -Wall -Werror
LIBS=-ltermcap

te: $(OBJS)
	rm -f te
	$(CC) $(CFLAGS) -o te $(OBJS) $(LIBS)

clean:
	rm -f core *.o tags

clobber: clean
	rm -f te teco lib/tecorc
