/* TECO for Ultrix   Copyright 1986 Matt Fichtenbaum						*/
/*
 * This program and its components belong to GenRad Inc, Concord MA 01742
 * They may be copied if this copyright notice is included
 */

/* te_main.c  main program  1/14/87 */

/*
* This is TECO for Ultrix on a Vax.  It is mostly compatible with DEC TECO
* as explained in the DEC TECO manual.  It was written from a manual for
* TECO-11 version 34, and so adheres most closely to that version.
*
* This program consists of several source files, as follows:
*
* te_main.c (this file)	Main program - initialize, read command line and
*			startup file, handle errors, high-level read and
*			execute command strings.
*
* te_defs.h		Definitions file, to be #included with other files
*
* te_data.c		Global variables
*
* te_rdcmd.c		Read in a command string
*
* te_exec0.c		First-level command execution - numbers, values,
*			assemble expressions
*
* te_exec1.c		Most commands
*
* te_exec2.c		"E" and "F" commands, and file I/O
*
* te_srch.c		routines associated with "search" commands
*
* te_subs.c		higher-level subroutines
*
* te_utils.c		lower-level subroutines
*
* te_chario.c		keyboard (stdin), typeout (stdout), suspend
*
* te_window.c		display window and display special functions
*
* These routines should be compiled and linked to form the TECO executable.
*/
#include "te_defs.h"

main(argc, argv)
int argc;			/* arg count */
char *argv[];		/* array of string pointers */
{
	/* copy command line to Qz */
	save_args(argc, argv, &qreg[36]);

	/* read startup file */
	read_startup();

	/* set tty to CBREAK, no echo, asynch mode */
	setup_tty(TTY_ON);

	/* initialize screen-image buffer */
	window(WIN_INIT);

	/* set terminal screen-size parameters */
	get_term_par();

	/* set up error restart */
	if (err = setjmp(xxx)) {
		/* EOF from standard input - clean up and exit */
		if (err == E_EFI)
			goto quit;
		printf("\015\012?  %s", errors[err-1]);

		/* print unfulfilled search string */
		if (err == E_SRH)
			print_string(SERBUF);

		/* or file string */
		else if ((err == E_FNF) || (err == E_COF) ||
				(err == E_AMB))
			print_string(FILBUF);
		crlf();

		/* stop indirect command execution */
		eisw = 0;

		/* reset ^C trap, read w/o wait, ^O (unused), no echo */
		et_val &= ~(ET_CTRLC | ET_NOWAIT | ET_CTRLO | ET_NOECHO);

		/* if ET has "quit on error" set, exit (phone home) */
		if (et_val & ET_QUIT) {
			/* reset screen, keyboard, output files */
			cleanup();
			exit(1);		/* and exit */
		}
	}

	/* forever: read and execute command strings */

	/* "exit" sets exitflag to -1; ^C to -2; "hangup" to -3 */
	for (exitflag = 1; exitflag >= 0; ) {
		/* display the buffer */
		window(WIN_REFR);

		/* free any storage from failed insert */
		free_blist(insert_p);

		/* return any delayed cells */
		free_blist(dly_freebuff);
		insert_p = dly_freebuff = NULL;

		/* clear "abort on error" */
		et_val &= ~ET_QUIT;
		if (read_cmdstr())
			goto quit;

		/* enable ^C detector */
		exitflag = 0;

		/*
		 * If not in scroll mode, force full redraw
		 * on first ^W or nW
		 */
		if (!WN_scroll)
			window(WIN_REDRAW);
		exec_cmdstr();
	}

	/* ^C detected during execution */
	if (exitflag == -2)
		ERROR(E_XAB);

	/* hangup during execution: save buffer and close files */
	else if (exitflag == -3)
		panic();

	/* exit from program */
quit:
	ev_val = es_val = 0;	/* no last one-line window */
	window(WIN_REFR);	/* last display */
	cleanup();		/* reset screen, terminal, output files */
	return(0);
}

/* reset screen state, keyboard state; remove open output files */

cleanup()
{
	window(WIN_OFF);		/* restore screen */
	setup_tty(TTY_OFF);		/* restore terminal */
	kill_output(&po_file);		/* kill any open primary output file */
	kill_output(&so_file);		/* and secondary file */
}


/* print string for error message */
/* argument is subscript of a qreg qh, prints text from that buffer */

print_string(arg)
int arg;
{
	int i, c;
	struct buffcell *p;

	type_char('"');
	for (p = qreg[arg].f, c = 0, i = 0; i < qreg[arg].z; i++) {
		if (!p->ch[c]) break;
		type_char(p->ch[c]);
		if (++c > CELLSIZE-1) {
			p = p->f;
			c = 0;
		}
	}
	type_char('"');
}

/* copy invocation command line to a text buffer */
save_args(argc, argv, q)
	int argc;
	char *argv[];
	struct qh *q;
{
	char c;
	struct qp ptr;

	/* attach a text buffer */
	make_buffer(q);

	/*
	 * Initialize pointer to output string and output char count
	 * for each argument.
	 */
	ptr.p = q->f;
	ptr.c = q->z = 0;
	for (; argc > 0; argv++, argc--) {
		while ( ((c = *((*argv)++)) != '\0') && (q->z < CELLSIZE-1) ) {
			/* copy char to q-reg */
			ptr.p->ch[ptr.c] = c;
			fwdcx(&ptr);

			/* count characters */
			++q->z;
		}

		/* if not last argument... */
		if (argc > 1) {

			/* space to separate arguments */
			ptr.p->ch[ptr.c] = ' ';
			fwdcx(&ptr);
			++q->z;
		}
	}
}



/* routine to read startup file */

char startup_name[] = "/.tecorc";		/* name of startup file */

read_startup()
{
	char *hp, *getenv();
	char fn[CELLSIZE];		/* filename storage */
	int i;

	/* look for ".tecorc" in current directory first */

	if (!(eisw = fopen(&startup_name[1], "r"))) {

		/* if not found, look in home directory */
		if (hp = getenv("HOME")) {

			/* copy until trailing null */
			for (i = 0; i < CELLSIZE; i++)
				if (!(fn[i] = *(hp++)))
					break;
			for (hp = &startup_name[0]; i < CELLSIZE; i++)
				if (!(fn[i] = *(hp++)))
					break;

			/* set eisw if file found, or not if not */
			eisw = fopen(fn, "r");
		}
	}
}

/* routine to get terminal height and width from termcap */
get_term_par()
{
	static char lbuff[1024];	/* termcap buffer */
	char *pname;			/* pointer to name of terminal */
	extern char *getenv();

	/* read terminal name */
	if (pname = getenv("TERM")) {
		/* get entry */
		tgetent(lbuff, pname);

		/* get #lines and #columns and set params */
		set_term_par(tgetnum("li"), tgetnum("co"));
	}

	/* Let SIGIWINCH-type handler have a shot at it */
	recalc_tsize();
}
