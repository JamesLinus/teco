te :: te_data.o te_exec0.o te_exec1.o te_exec2.o te_main.o te_rdcmd.o \
		te_srch.o te_subs.o te_utils.o te_window.o te_chario.c \
		te_defs.h $(<)_data.o $(<)_exec0.o $(<)_exec1.o \
		$(<)_exec2.o $(<)_main.o $(<)_rdcmd.o $(<)_srch.o \
		$(<)_subs.o $(<)_utils.o $(<)_window.o $(<)_chario.o \
		-ltermcap -lmalloc
