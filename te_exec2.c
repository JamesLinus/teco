/* TECO for Ultrix   Copyright 1986 Matt Fichtenbaum						*/
/* This program and its components belong to GenRad Inc, Concord MA 01742	*/
/* They may be copied if this copyright notice is included					*/

/* te_exec2.c   process "E" and "F" commands   2/26/87 */
#include "te_defs.h"

struct qh oldcstring;						/* hold command string during ei */

/* file stuff for input/output files */

struct infiledata pi_file = { 
	NULL, -1 };	/* input files */
struct infiledata si_file = { 
	NULL, -1 };
struct infiledata *infile = &pi_file;		/* pointer to currently active input file structure */
struct outfiledata po_file, so_file;		/* output files */
struct outfiledata *outfile = &po_file;		/* pointer to currently active output file structure */
FILE *eisw;									/* indirect command file pointer */

/* process E commands */

do_e()
{
	int c;							/* temps */
	int old_var;
	FILE *t_eisw;

	/* read next character and dispatch */
	switch (mapch_l[getcmdc(trace_sw)]) {	

	/* numeric values */
	case 'd':				/* ED */
		set_var(&ed_val);
		break;

	case 's':				/* ES */
		set_var(&es_val);
		break;

	case 't':				/* ET */
		old_var = et_val;
		set_var(&et_val);

		/* force read_only bits */
		et_val = (et_val & 0100651) | 001006;

		/* redraw if ET & 256 changes */
		if ((et_val ^ old_var) & ET_TRUNC) window(WIN_REDRAW);
		break;

	case 'u':				/* EU */
		set_var(&eu_val);
		break;

	case 'v':				/* EV */
		set_var(&ev_val);
		break;

	case 'z':				/* EZ */
		old_var = ez_val;
		set_var(&ez_val);
		tabmask = (ez_val & EZ_TAB4) ? 3 : 7;	/* set tab mask */

		/* force window redisplay if EZ_TAB4 changes */
		if ((ez_val ^ old_var) & EZ_TAB4)
			window(WIN_REDRAW);
		break;

	/* E_ search */

	case '_':
		do_nsearch('e');
		break;

	/* file I/O commands */

	case 'a':				/* set secondary output */
		outfile = &so_file;
		break;

	case 'b':				/* open read/write with backup */
		/* read the name */
		if (!read_filename(1, 'r'))
			ERROR(E_NFI);

		/* close previously-open file */
		if (infile->fd)
			fclose(infile->fd);
		if (!(infile->fd = fopen(fbuf.f->ch, "r"))) {
			if (!colonflag) ERROR(E_FNF);
		} else {
			/* output file already open */
			if (outfile->fd)
				ERROR(E_OFO);

			/* save file string */
			for (ll = 0; ll < CELLSIZE; ll++)
				if ((outfile->t_name[ll] =
					outfile->f_name[ll] =
					fbuf.f->ch[ll]) == '\0')
						break;
			outfile->name_size = ll;
			outfile->t_name[ll++] = '.';
			outfile->t_name[ll++] = 't';
			outfile->t_name[ll++] = 'm';
			outfile->t_name[ll++] = 'p';
			outfile->t_name[ll] = '\0';
			if (!(outfile->fd = fopen(outfile->t_name, "w"))) {
				if (!colonflag)
					ERROR(E_COF);
				else {
					fclose(infile->fd);
					infile->fd = NULL;
				}
			} else
				outfile->bak = 1;	/* set backup mode */
		}
		infile->eofsw = -1 - (esp->val1 = (infile->fd) ? -1 : 0);
		esp->flag1 = colonflag;
		colonflag = 0;
		break;

	case 'x':				/* finish edit and exit */
		exitflag = -1;

		/* --- fall through to "EC" --- */

	/* finish edit */
	case 'c':
		/* set a pointer to start of text buffer */
		set_pointer(0, &aa);

		/* write the current buffer */
		write_file(&aa, z, ctrl_e);

		/* empty the buffer */
		dot = z = 0;

		/* force window redraw */
		window(WIN_REDRAW);

		/* if any input remaining, copy it to output */
		if ((outfile->fd) && (infile->fd) && !(infile->eofsw)) {
			while ((c = getc(infile->fd)) != EOF)
				putc(c, outfile->fd);
		}

		/* --- fall through to "EF" --- */
	case 'f':			/* close output file */

		/* only if one is open */
		if (outfile->fd) {
			fclose(outfile->fd);

			/* if this is "backup" mode */
			if (outfile->bak & 1) {
				outfile->f_name[outfile->name_size] = '.';
				outfile->f_name[outfile->name_size+1] = 'b';
				outfile->f_name[outfile->name_size+2] = 'a';
				outfile->f_name[outfile->name_size+3] = 'k';
				outfile->f_name[outfile->name_size+4] = '\0';
				outfile->t_name[outfile->name_size] = '\0';

				/* rename orig file */
				rename(outfile->t_name, outfile->f_name);
			}

			/* if output file had ".tmp" extension */
			if (!(outfile->bak & 8)) {								/* remove it */
				outfile->t_name[outfile->name_size] = '.';
				outfile->f_name[outfile->name_size] = '\0';

				/* rename output */
				rename(outfile->t_name, outfile->f_name);
			}
		}
		outfile->fd = NULL;	/* mark "no output file open" */
		break;

	case 'i':			/* indirect file execution */
		/* if no filename specified, reset command input */
		if (!read_filename(1, 'i')) {
			/*
			 * If ending a file execute, restore
			 * the previous "old command string"
			 */
			if (eisw) {
				/* return the file descriptor */
				fclose(eisw);

				/*
				 * return the command string used by
				 * the file (after execution done)
				 */
				dly_free_blist(cbuf.f);
				cbuf.f = oldcstring.f;
				cbuf.z = oldcstring.z;
			}
			t_eisw = 0;
		} else if (!(t_eisw = fopen(fbuf.f->ch, "r"))) {
			if (!colonflag) ERROR(E_FNF);

		/* if this "ei" came from the command string */
		} else if (!eisw) {
			/* save current command string */
			oldcstring.f = cbuf.f;
			oldcstring.z = cbuf.z;

			/* and make it inaccessible to "rdcmd" */
			cbuf.f = NULL;
		}

		/* if a command file had been open, close it */
		if (eisw)
			fclose(eisw);
		esp->val1 = (eisw = t_eisw) ? -1 : 0;
		esp->flag1 = colonflag;
		colonflag = 0;
		esp->op = OP_START;
		break;
	case 'k':			/* kill output file */
		kill_output(outfile);
		break;

	case 'p':			/* switch to secondary input */
		infile = &si_file;
		break;

	/* specify input file, or switch to primary input */
	case 'r':
		/* no name, switch to primary input */
		if (!read_filename(0, 'r'))
			infile = &pi_file;
		else {
			/* close previously-open file */
			if (infile->fd)
				fclose(infile->fd);
			if (!(infile->fd = fopen(fbuf.f->ch, "r"))) {
				if (!colonflag)
					ERROR(E_FNF);
			}
		}
		infile->eofsw = -1 - (esp->val1 = (infile->fd) ? -1 : 0);
		esp->flag1 = colonflag;
		colonflag = 0;
		esp->op = OP_START;
		break;

	case 'w':	/* specify output file, or switch to primary output */
		if(!read_filename(0, 'w'))
			outfile = &po_file;
		else {
			/* output file already open */
			if (outfile->fd)
				ERROR(E_OFO);

			/* save file string */
			for (ll = 0; ll < CELLSIZE; ll++) {
				if ((outfile->t_name[ll] =
					outfile->f_name[ll] =
					fbuf.f->ch[ll]) == '\0')
						break;
			}
			outfile->name_size = ll;

			/* if not using literal output name */
			if (!(ez_val & EZ_NOTMPFIL)) {
				/* use provisional suffix ".tmp" */
				outfile->t_name[ll++] = '.';
				outfile->t_name[ll++] = 't';
				outfile->t_name[ll++] = 'm';
				outfile->t_name[ll++] = 'p';
				outfile->t_name[ll] = '\0';
			}
			if (!(outfile->fd = fopen(outfile->t_name, "w")))
				if (!colonflag)
					ERROR(E_COF);

			/* save "temp suffix" status */
			outfile->bak = ez_val & EZ_NOTMPFIL;

			/* Return status for colon */
			if (esp->flag1 = colonflag)
				esp->val1 = (outfile->fd ? -1 : 0);
		}
		break;

	case 'y':				/* EY is Y without protection */
		if (esp->flag1)
			ERROR(E_NYA);
		dot = z = 0;			/* clear buffer */
		set_pointer(0, &aa);
		read_file(&aa, &z, (ed_val & ED_EXPMEM ? -1 : 0) );
		esp->flag1 = colonflag;
		colonflag = 0;
		esp->op = OP_START;
		break;
	case 'n':				/* wildcard filespec */
		esp->val1 = do_en();
		esp->flag1 = colonflag;
		colonflag = 0;
		esp->op = OP_START;
		break;

	case 'q':				/* system command */
		esp->val1 = do_eq();			/* do this as a separate routine */
		esp->flag1 = colonflag;
		colonflag = 0;
		esp->op = OP_START;
		break;

	default:
		ERROR(E_IEC);
	}
}
/* routine to execute a system command			*/
/* this is done by forking off another process	*/
/* to execute a shell via 'execl'				*/
/* routine returns -1 if success, 0 if error in fork */

int do_eq()
{
	int t;
	int status;
	char *pname;				/* pointer to name of shell */
	extern char *getenv();

	build_string(&sysbuf);
	if (sysbuf.z > CELLSIZE-2) ERROR(E_STL);	/* command must fit within one cell */
	sysbuf.f->ch[sysbuf.z] = '\0';				/* store terminating null */
	if (!(pname = getenv("SHELL"))) ERROR(E_SYS);	/* read shell name */

	if (!esp->flag1)			/* if not m,nEQ command */
	{
		if (win_data[7]) window(WIN_SUSP);			/* restore full screen */
		crlf();										/* force characters out */
		setup_tty(TTY_SUSP);						/* restore terminal to normal mode */

		t = vfork();							/* fork a new process */
		if (t == 0)								/* if this is the child */
		{
			execl(pname, pname, "-c", &sysbuf.f->ch[0], 0);		/* call the named Unix routine */
			printf("Error in 'execl'\n");		/* normally shouldn't get here */
			exit(1);
		}

		while (wait(&status) != t);				/* if parent, wait for child to finish */
		if (status & 0xFF) t = -1;			/* keep failure indication from child */

		setup_tty(TTY_RESUME);						/* restore terminal to teco mode */
		if (win_data[7])					/* if window was enabled */
		{
			vt(VT_SETSPEC1);				/* set reverse video */
			fputs("Type RETURN to continue", stdout);		/* require CR before continuing */
			vt(VT_CLRSPEC);					/* reverse video off */
			while (gettty() != LF);
			putchar(CR);					/* back to beginning of line */
			vt(VT_EEOL);					/* and erase the message */
			window(WIN_RESUME);				/* re-enable window */
			window(WIN_REDRAW);				/* and make it redraw afterwards */
		}
	}

	else t = do_eq1(pname);					/* m,nEQ command */

	return( (t == -1) ? 0 : -1);			/* return failure if fork failed or proc status nonzero */
}
/* Execute m,nEQtext$ command.  Run "text" as a Unix command that	*/
/* receives its std input from chars m through n of teco's buffer.	*/
/* Output from the command is placed in Q#.							*/

do_eq1(shell)
char *shell;			/* arg is pointer to shell name */
{
	int ff, pipe_in[2],
		pipe_out[2];		/* fork result and two pipes */
	FILE *xx_in, *xx_out;		/* std in and out for called process */
	FILE *fdopen();
	int status;

	/* set aa to start of text, ll to number of chars */
	ll = line_args(1, &aa);

	/* set pointer at end of text */
	dot += ll;

	/* set ^S to - # of chars */
	ctrl_s = -ll;

	/* make input pipe; failure if can't */
	if (pipe(pipe_out))
		ERROR(E_SYS);

	/* disable split screen */
	if (win_data[7])
		window(WIN_SUSP);

	/* put console back to original state */
	setup_tty(TTY_SUSP);

	/* fork first child: if error, quit */
	if ((ff = fork()) == -1) {
		close(pipe_out[0]);
		close(pipe_out[1]);
		setup_tty(TTY_RESUME);
		if (win_data[7]) {
			window(WIN_RESUME);
			window(WIN_REDRAW);
		}
		ERROR(E_SYS);
	}

	/* if this is the parent, the task is to read into q# */
	if (ff) {
		make_buffer(&timbuf);	/* initialize the q# header */
		bb.p = timbuf.f;	/* init bb to point to q# */
		timbuf.z = bb.c = 0;

		/* parent won't write to this pipe */
		close(pipe_out[1]);

		/* open the "std out" pipe for reading */
		if ((xx_out = fdopen(pipe_out[0], "r")) == 0) {
			close(pipe_out[0]);	/* if can't open output pipe */
			setup_tty(TTY_RESUME);
			if (win_data[7]) {
				window(WIN_RESUME);
				window(WIN_REDRAW);
			}
			ERROR(E_SYS);		/* "open" failure */
		}

		/* read from pipe to q# */
		read_stream(xx_out, 0, &bb, &timbuf.z, 0, 0, 1);
		close(pipe_out[0]);

		/* wait for children to finish */
		while (wait(&status) != ff)
			;
		setup_tty(TTY_RESUME);
		if (win_data[7]) {
			window(WIN_RESUME);
			window(WIN_REDRAW);
		}
		return((status & 0xFF) ? -1 : 0);
	}

	/*
	 * This is the child.  It in turn forks into two processes, of which
	 * the "parent" (original child) writes the specified part of the
	 * buffer to the pipe, and the new child (grandchild to the original
	 * teco) execl's the Unix command.
	 */

	else {
		/* won't read from "output" pipe */
		close(pipe_out[0]);

		/* make the "std in" pipe for the process, quit if can't */
		if (pipe(pipe_in))
			exit(1);

		/* fork to two processes (total 3), exit if error */
		if ((ff = fork()) == -1)
			exit(1);

		/* parent - will send chars */
		if (ff) {
			close(pipe_in[0]);	/* won't read from this pipe */

			/* open pipe for writing; exit if open fails */

			if ((xx_in = fdopen(pipe_in[1], "w")) == 0)
				exit(1);

			/* write to stream, CRLF becomes LF */
			write_stream(xx_in, &aa, ll, 0);
			fclose(xx_in);

			/* wait for child */
			while (wait(&status) != ff)
				;

			/* exit with child's status */
			exit(status & 0xFF);
		} else {
			/* this process is the grandchild */

			/* close "input" for writing */
			close(pipe_in[1]);

			/* substitute pipe_in for stdin */
			dup2(pipe_in[0], fileno(stdin));

			/* and pipe_out for stdout	*/
			dup2(pipe_out[1], fileno(stdout));

			/* close original descriptors */
			close(pipe_in[0]);
			close(pipe_out[1]);

			/* execute specified routine */
			execl(shell, shell, "-c", &sysbuf.f->ch[0], 0);
			fputs("execl failed\n", stderr);
			exit(1);
		}
		/*NOTREACHED*/
	}
}

/*
 * Routines to handle EN wild-card file specification
 * ENfilespec$ defines file class, leaving 'filespec'
 * in filespec buffer and reading expanded list of
 * files into local buffer.  EN$ gets next filespec
 * into filespec buffer.
 */

struct qh en_buf;		/* header for storage for file list */
struct qp en_ptr;		/* pointer to load/read file list */
static char glob_cmd0[] = {
	'g', 'l', 'o', 'b', ' '
};

do_en(){
	int t;

	if (build_string(&fbuf))					/* if a file string is specified */
	{
		if (fbuf.z > CELLSIZE-2) ERROR(E_STL);		/* upper limit on string length */
		fbuf.f->ch[fbuf.z] = '\0';				/* terminating null */
		t = do_glob(&en_buf);					/* glob the result */
		en_ptr.p = en_buf.f;					/* set the buffer pointer to the beginning of the buffer */
		en_ptr.dot = en_ptr.c = 0;
	}
	else										/* if no string, get next filename */
	{
		do_en_next();
		t = (fbuf.z) ? -1 : 0;					/* t zero if no more filespecs */
		if (!t && !colonflag) ERROR(E_NFI);		/* if no colon, end of spec is an error */
	}
	return (t);
}

/*
 * Routine to expand the string in the filespec buffer
 * argument is the address of a qh that gets the expanded string
 * argument->v gets set to the number of file specs found
 */
do_glob(lbuff)
	struct qh *lbuff;
{
	char glob_cmd[CELLSIZE+5];	/* "glob filespec" command string */
	int t;
	int c;
	int glob_pipe[2];		/* pipe to shell for filenames */
	struct qp glob_ptr;		/* pointer for result buffer */
	FILE *xx_out;			/* stream for chars from pipe */
	FILE *fdopen();
	int status;

	/* initialize expanded file buffer */
	make_buffer(lbuff);

	/* initialize pointer to buffer */
	glob_ptr.p = lbuff->f;
	glob_ptr.c = glob_ptr.dot = lbuff->z = lbuff->v = 0;

	/* set up "glob filespec" command */
	for (t = 0; t < 5; t++)
		glob_cmd[t] = glob_cmd0[t];
	for (t = 0; t < fbuf.z +1; t++)
		glob_cmd[t+5] = fbuf.f->ch[t];

	/* make a pipe */
	if (pipe(glob_pipe))
		ERROR(E_SYS);

	/* put console back to normal */
	setup_tty(TTY_SUSP);

	/* spawn a child... if failure */
	if ((t = fork()) == -1) {
		close(glob_pipe[0]);		/* undo the pipe */
		close(glob_pipe[1]);
		setup_tty(TTY_RESUME);
		ERROR(E_SYS);			/* and exit with failure */
	}

	/* if this is the parent */
	if (t) {
		close(glob_pipe[1]);		/* parent won't write */

		/* open pipe for read */
		if ((xx_out = fdopen(glob_pipe[0], "r")) == 0) {

			/* failure to open */
			close(glob_pipe[0]);
			setup_tty(TTY_RESUME);
			ERROR(E_SYS);
		}

		/* read characters from pipe */
		while ((c = getc(xx_out)) != EOF) {

			/* count null chars that separate file specs */
			if (c == '\0')
				++lbuff->v;

			/* store them in buffer */
			glob_ptr.p->ch[glob_ptr.c] = c;
			fwdcx(&glob_ptr);
		}

		/* through with stream */
		fclose(xx_out);

		/* save character count */
		lbuff->z = glob_ptr.dot;

		/* wait for child to finish */
		while (wait(&status) != t)
			;
		setup_tty(TTY_RESUME);

		/* return success unless child exited nonzero */
		return((status & 0xFF) ? 0 : -1);
	} else {
		/* this is the child */

		/* child won't read */
		close(glob_pipe[0]);

		/* substitute pipe for standard out */
		dup2(glob_pipe[1], fileno(stdout));

		/* don't need that anymore */
		close(glob_pipe[1]);

		/* execute the "glob" */
		execl("/bin/csh", "csh", "-fc", glob_cmd, 0);
		fputs("execl failed\n", stderr);
		exit(1);
		/*NOTREACHED*/
	}
}

/* routine to get next file spec from "EN" list into filespec buffer */

do_en_next()
{
	char c;

	make_buffer(&fbuf);							/* initialize the filespec buffer */
	fbuf.z = 0;

	while(en_ptr.dot < en_buf.z)				/* stop at end of string */
	{
		c = en_ptr.p->ch[en_ptr.c];
		fwdc(&en_ptr);
		if (!c) break;							/* null is terminator between file specs */
		fbuf.f->ch[fbuf.z++] = c;				/* store char */
		if (fbuf.z >= CELLSIZE-1) ERROR(E_STL);		/* limit on filespec size */
	}

	fbuf.f->ch[fbuf.z] = '\0';					/* filespec ends with NULL */
}

/* routine to read a file name				*/
/* reads it into fbuf text area				*/
/* returns nonzero if a name, 0 if none		*/
/* flag nonzero => empty name clears filespec buffer */
/* func is 'r' for ER or EB cmds, 'i' for EI, 'w' for EW */

int read_filename(flag, func)
int flag;
char func;
{
	int i, t, expand;
	char c;
	struct qh temp_buff;						/* temp buffer for globbing filespec */

	if (!(t = build_string(&fbuf)))				/* if no name spec'd */
	{
		if (flag) fbuf.z = 0;					/* if no name spec'd, set length to 0 */
	}
	else
	{
		if (fbuf.z > CELLSIZE-2) ERROR(E_STL);
		fbuf.f->ch[fbuf.z] = '\0';				/* store terminating NULL */

		/* check for characters to be expanded by the shell */

		for (i = 0; i < fbuf.z; i++)
			if ((c = fbuf.f->ch[i]) == '*' || c == '?' || c == '[' || c == 0173) break;
		if ( (expand = (i < fbuf.z)) || fbuf.f->ch[0] == '~')	/* one of these was found, or first char is ~ */
		{
			temp_buff.f = NULL;					/* make a temp buffer to glob filename into */
			make_buffer(temp_buff);
			do_glob(&temp_buff);				/* expand the file name */
			if (temp_buff.z == 0)				/* no match */
			{
				free_blist(temp_buff.f);		/* return the storage */
				ERROR(func == 'w' ? E_COF : E_FNF);	/* "can't open" or "file not found" */
			}
			else if (temp_buff.v == 0)			/* if exactly one file name */
			{
				free_blist(fbuf.f);				/* return the old file spec */
				fbuf.f = temp_buff.f;			/* put the temp buffer there instead */
				fbuf.z = temp_buff.z;
				if (fbuf.z > CELLSIZE-2) ERROR(E_STL);
				fbuf.f->ch[fbuf.z] = '\0';

				if (func == 'w' && expand)		/* if this is EW and 'twas from a wildcard expansion */
				{
					vt(VT_SETSPEC1);			/* "file XXXX already exists: overwrite? [yn]" */
					fputs("File ", stdout);
					fputs(fbuf.f->ch, stdout);
					fputs(" already exists: overwrite? [ny] ", stdout);
					vt(VT_CLRSPEC);
					c = gettty();				/* read user's response */
					putchar(CR);
					vt(VT_EEOL);				/* clean up the screen */
					if (c != 'y') ERROR(E_COF);	/* abort here */
				}
			}
			
			    else								/* multiple file specs */
			{
				if (func != 'r' || !(ez_val & EZ_MULT))				/* if multiple file specs here aren't allowed */
				{
					free_blist(temp_buff.f);			/* return the storage */
					ERROR(E_AMB);
				}
				free_blist(en_buf.f);					/* substitute the expansion for the "EN" list */
				en_ptr.p = en_buf.f = temp_buff.f;		/* and initialize the "EN" list pointer */
				en_buf.z = temp_buff.z;
				en_ptr.dot = en_ptr.c = 0;
				do_en_next();					/* get the first file */
			}
		}
	}
	return(t);
}



/* fetch or set variable */

set_var(arg)
int *arg;			/* argument is pointer to variable */
{
	if (esp->flag1)		/* if an argument, then set the variable */
	{
		if (esp->flag2)					/* if two arguments, then <clr>, <set> */
			*arg = (*arg & ~esp->val2) | esp->val1;
		else *arg = esp->val1;			/* one arg is new value */
		esp->flag2 = esp->flag1 = 0;	/* consume argument */
	}
	else				/* otherwise fetch the variable's value */
	{
		esp->val1 = *arg;
		esp->flag1 = 1;
	}
}



/*
 * Read from selected input file stream into specified buffer
 * terminate on end-of-file or form feed
 * if endsw > 0 terminates after that many lines
 * if endsw < 0 stops if z > BUFF_LIMIT
 * returns -1 if read EOF, 0 otherwise
 */
read_file(lbuff, nchars, endsw)
	struct qp *lbuff;
	int *nchars, endsw;
{
	/* return if no input file open */
	if (!infile->fd) {
		infile->eofsw = -1, ctrl_e = 0;
	} else {
		infile->eofsw = read_stream(infile->fd, &ctrl_e, lbuff,
			nchars, endsw, ez_val & EZ_CRLF,
			ez_val & EZ_READFF);
	}
	return(esp->val1 = infile->eofsw);
}

/*
 * Read from an I/O stream into specified buffer
 * this is used by read_file and by "eq" pipe from other Unix processes
 * args buff, nchars, endsw as above; file is stream pointer, ff_found is
 * address of a switch to set if read ended with a FF, crlf_sw is lf->crlf
 * conversion, ff_sw indicates whether to stop on a form feed.
 */
read_stream(file, ff_found, lbuff, nchars, endsw, crlf_sw, ff_sw)
	FILE *file;
	struct qp *lbuff;
	int *ff_found, *nchars, endsw, crlf_sw, ff_sw;
{
	int chr;
	int crflag;
	register struct buffcell *p;
	register int c;

	/* copy pointer locally */
	p = (*lbuff).p;
	c = (*lbuff).c;

	/* "last char wasn't CR" */
	crflag = 0;
	while (((chr = getc(file)) != EOF) && ((chr != FF) || ff_sw)) {

		/* automatic cr before lf */
		if ((chr == LF) && !crflag && !crlf_sw) {
			p->ch[c] = CR;		/* store a cr */
			++(*nchars);		/* increment buffer count */

			/* next cell? */
			if (++c > CELLSIZE-1) {
				/* need a new cell? */
				if (!p->f) {
					p->f = get_bcell();
					p->f->b = p;
				}
				p = p->f;
				c = 0;
			}
		}
		p->ch[c] = chr;			/* store char */
		++(*nchars);			/* increment character count */

		/* next cell? */
		if (++c > CELLSIZE-1) {

			/* need a new cell? */
			if (!p->f) {
				p->f = get_bcell();
				p->f->b = p;
			}
			p = p->f;
			c = 0;
		}
		crflag = (chr == CR);	/* flag set if last char was CR */

		/* term after N lines */
		if ((chr == LF) && ((endsw < 0 && z > BUFF_LIMIT) ||
				(endsw > 0 && --endsw == 0)))
			break;
	}
	(*lbuff).p = p;			/* update pointer */
	(*lbuff).c = c;

	/* set flag to indicate whether FF found */
	if (ff_found)
		*ff_found = (chr == FF) ? -1 : 0;

	/* and return "eof found" value */
	return( (chr == EOF) ? -1 : 0);
}
/* routine to write text buffer out to selected output file	*/
/* arguments are qp to start of text, number of characters,	*/
/* and an "append FF" switch								*/

write_file(lbuff, nchars, ffsw)
struct qp *lbuff;
int nchars, ffsw;
{
	if (!outfile->fd && (nchars)) ERROR(E_NFO);
	else write_stream(outfile->fd, lbuff, nchars, ez_val & EZ_CRLF);
	if (outfile->fd && ffsw) putc(FF, outfile->fd);
}


/*
 * routine to write text buffer to I/O stream.  Used by
 * write_file, above, and by "eq" write to pipe to other
 * Unix processes.  Arguments buff, nchars as above; file
 * is stream pointer, crlf_sw zero converts CRLF to LF
 */
write_stream(file, lbuf, nchars, crlf_sw)
	FILE *file;
	struct qp *lbuf;
	int nchars, crlf_sw;
{
	char c;
	int crflag;

	crflag = 0;
	for (; nchars > 0; nchars--) {

		/* set flag if a c.r. */
		if ((c = (*lbuf).p->ch[(*lbuf).c]) == CR) {
			crflag = 1;
		} else {
			/*
			 * If c.r. not before lf, or if not in
			 * "no cr" mode, output the c.r.
			 */
			if ((crflag) && ((c != LF) || crlf_sw)) {
				putc(CR, file);
			}
			crflag = 0;
			putc(c, file);
		}
		if (++(*lbuf).c > CELLSIZE-1) {
			(*lbuf).p = (*lbuf).p->f;
			(*lbuf).c = 0;
		}
	}
}


/* routine to kill output file: argument is pointer to an output file structure */

kill_output(outptr)
struct outfiledata *outptr;
{
	if (outptr->fd)
	{
		fclose(outptr->fd);
		unlink(outptr->t_name);
		outptr->fd = NULL;
	}
}
/* "panic" routine called when "hangup" signal occurs */
/* this routine saves the text buffer and closes the output files */

char panic_name[] = "TECO_SAVED.tmp";			/* name of file created to save buffer */

panic()
{
	if (!outfile->fd && z) outfile->fd = fopen(panic_name, "w");	/* if buffer nonempty and no file open, make one */

	set_pointer(0, &aa);								/* set a pointer to start of text buffer */
	if (outfile->fd && z) write_file(&aa, z, 0);		/* and write out buffer unless "open" failed */

	if (po_file.fd) fclose(po_file.fd), po_file.fd = NULL;		/* close any open output files */
	if (so_file.fd) fclose(so_file.fd), so_file.fd = NULL;
}
/* do "F" commands */

do_f()
{
	struct buffcell *delete_p;

	switch (mapch_l[getcmdc(trace_sw)])		 /* read next character and dispatch */
	{
	case '<':			/* back to beginning of current iteration */
		if (cptr.flag & F_ITER)		/* if in iteration */
		{
			cptr.p = cptr.il->p;	/* restore */
			cptr.c = cptr.il->c;
			cptr.dot = cptr.il->dot;
		}
		else for (cptr.dot = cptr.c = 0; cptr.p->b->b != NULL; cptr.p = cptr.p->b);	/* else, restart current macro */
		break;

	case '>':			/* to end of current iteration */
		find_enditer();	/* find it */
		if ( ( ((esp->flag1) ? esp->val1 : srch_result) >= 0) ? (~colonflag) : colonflag)	/* if exit */
			pop_iteration(0);	/* and exit if appropriate */
		break;

	case '\'':					/* to end of conditional */
	case '|':					/* to "else," or end */
		find_endcond(cmdc);
		break;

		/* "F" search commands */

	case 'b':						/* bounded search, alternative args */
		do_fb();
		break;

	case 'c':						/* bounded search, alternative args, then "FR" */
		if (do_fb()) goto do_fr;
		while (getcmdc(trace_sw) != term_char);		/* otherwise skip insert string */
		break;

	case 'n':						/* do "N" and then "FR" */
		if (do_nsearch('n')) goto do_fr;
		while (getcmdc(trace_sw) != term_char);		/* otherwise skip insert string */
		break;

	case '_':						/* do "_" and then "FR" */
		if (do_nsearch('_')) goto do_fr;
		while (getcmdc(trace_sw) != term_char);		/* otherwise skip insert string */
		break;

	case 's':						/* search and replace: search, then do "FR" */
		build_string(&sbuf);		/* read search string and terminator */
		if (end_search(  do_search( setup_search() )  )) goto do_fr;	/* if search passed, do "FR" */
		while (getcmdc(trace_sw) != term_char);		/* otherwise skip insert string */
		break;
		case 'r':						/* replace last insert, search, etc. */
		if (esp->flag1) ERROR(E_NFR);	/* shouldn't have argument */
		term_char = (atflag) ? getcmdc(trace_sw) : ESC;		/* set terminator */
		atflag = 0;
do_fr:					/* entry from FN, F_, and FC */
		set_pointer(dot, &cc);		/* save a pointer to the current spot */
		dot += ctrl_s;				/* back dot up over the string */
		set_pointer(dot, &aa);		/* code from "insert1": convert dot to a qp */
		delete_p = aa.p;			/* save beginning of original cell */
		if (dot < buff_mod) buff_mod = dot;		/* update earliest char loc touched */
		insert_p = bb.p = get_bcell();			/* get a new cell */
		bb.c = 0;
		ins_count = aa.c;		/* save char position of dot in cell */
		aa.c = 0;

		movenchars(&aa, &bb, ins_count);	/* copy cell up to dot */
		moveuntil(&cptr, &bb, term_char, &ins_count, cptr.z-cptr.dot, trace_sw);	/* insert */
		cptr.dot += ins_count;		/* advance command-string pointer */
		getcmdc(trace_sw);			/* skip terminator */

		z += ctrl_s;				/* subtract delete length from buffer count */
		delete_p->b->f = insert_p;	/* put the new cell where the old one was */
		insert_p->b = delete_p->b;	/* code borrowed from "insert2" */
		insert_p = NULL;

		if (bb.c == cc.c)			/* if replacement text was same length, we can save time */
		{
			for (; bb.c < CELLSIZE; bb.c++) bb.p->ch[bb.c] = cc.p->ch[bb.c];	/* copy rest of cell */
			bb.p->f = cc.p->f;		/* replace orig cell's place in chain with end of new list */
			if (bb.p->f) bb.p->f->b = bb.p;
			cc.p->f = NULL;			/* terminate the part snipped out */
			free_blist(delete_p);	/* return the old part */
		}

		else						/* different positions: shift the remainder of the buffer */
		{
			bb.p->f = delete_p;		/* splice rest of buffer to end */
			delete_p->b = bb.p;
			movenchars(&cc, &bb, z-dot);	/* squeeze buffer */
			free_blist(bb.p->f);		/* return unused cells */
			bb.p->f = NULL;				/* and end the buffer */
		}

		z += ins_count;				/* add # of chars inserted */
		dot += ins_count;
		ctrl_s = -ins_count;		/* save string length */
		esp->flag1 = esp->flag2 = 0;	/* and consume arguments */
		esp->op = OP_START;
		break;

	default:
		ERROR(E_IFC);
	}
}
/* routines for macro iteration */
/* pop iteration: if arg nonzero, exit unconditionally */
/* else check exit conditions and exit or reiterate */

pop_iteration(arg)
int arg;
{
	if (!arg && (!cptr.il->dflag || (--(cptr.il->count) > 0)) )		/* if reiteration */
	{
		cptr.p = cptr.il->p;		/* restore */
		cptr.c = cptr.il->c;
		cptr.dot = cptr.il->dot;
	}
	else
	{
		if (cptr.il->b) cptr.il = cptr.il->b;	/* if not last thing on stack, back up */
		else cptr.flag &= ~F_ITER;				/* else clear "iteration" flag */
	}
}


/* find end of iteration - read over arbitrary <> and one > */

find_enditer()
{
	register int icnt;

	for (icnt = 1; icnt > 0;)		/* scan for matching > */
	{
		while ((skipto(0) != '<') && (skipc != '>'));	/* scan for next < or > */
		if (skipc == '<') ++icnt;		/* and keep track of macro level */
		else --icnt;
	}
}



/* find end of conditional */
char find_endcond(arg)
char arg;
{
	register int icnt;

	for (icnt = 1; icnt > 0;)
	{
		while ((skipto(0) != '"') && (skipc != '\'') && (skipc != '|'));
		if (skipc == '"') ++icnt;
		else if (skipc == '\'') -- icnt;
		else if ((icnt == 1) && (arg == '|')) break;
	}
}

