CC=gcc -traditional
OBJS= te_chario.o te_data.o te_exec0.o te_exec1.o \
	te_exec2.o te_main.o te_rdcmd.o te_srch.o te_subs.o \
	te_utils.o te_window.o
CFLAGS= -O
LIBS= -ltermcap

te: $(OBJS)
	$(CC) $(CFLAGS) -o te $(OBJS) $(LIBS)

clean:
	rm -f core *.o tags

clobber: clean
	rm -f te teco
