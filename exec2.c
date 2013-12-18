/* TECO for Ultrix   Copyright 1986 Matt Fichtenbaum */
/* This program and its components belong to GenRad Inc, Concord MA 01742 */
/* They may be copied if this copyright notice is included */

/* exec2.c   process "E" and "F" commands   2/26/87 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include "defs.h"

struct qh oldcstring;			/* hold command string during ei */

/* file stuff for input/output files */

/* input files */
struct infiledata pi_file = { NULL, -1 };
struct infiledata si_file = { NULL, -1 };

/* pointer to currently active input file structure */
struct infiledata *infile = &pi_file;

/* output files */
struct outfiledata po_file, so_file;

/* pointer to currently active output file structure */
struct outfiledata *outfile = &po_file;

/* indirect command file pointer */
FILE *eisw;

static int read_filename(int flag, char func);
static int do_eq1(char *);
static void write_stream(FILE *file,
    struct qp *lbuf, int nchars, int crlf_sw);
static int read_stream(FILE *file,
    int *ff_found,
    struct qp *lbuff, int *nchars, int endsw, int crlf_sw, int ff_sw);
static int do_glob(struct qh *lbuff);
static void do_en_next(void);
static void find_endcond(char arg);

/* process E commands */
void
do_e(void)
{
    int c;					/* temps */
    int old_var;
    FILE *t_eisw;

    /* read next character and dispatch */
    switch (mapch_l[getcmdc(trace_sw) & 0xFF]) {	

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
        if ((et_val ^ old_var) & ET_TRUNC) {
            window(WIN_REDRAW);
        }
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
        if ((ez_val ^ old_var) & EZ_TAB4) {
                window(WIN_REDRAW);
        }
        break;

    /* E_ search */

    case '_':
        do_nsearch('e');
        break;

    /* file I/O commands */

    case 'a':				/* set secondary output */
        outfile = &so_file;
        break;

    case 'b':			/* open read/write with backup */
        /* read the name */
        if (!read_filename(1, 'r')) {
            ERROR(E_NFI);
        }

        /* close previously-open file */
        if (infile->fd) {
                fclose(infile->fd);
        }
        if (!(infile->fd = fopen(fbuf.f->ch, "r"))) {
            if (!colonflag) {
                ERROR(E_FNF);
            }
        } else {
            struct stat st;

            /* output file already open */
            if (outfile->fd) {
                ERROR(E_OFO);
            }

            /* get modes from original file now */
            if (fstat(fileno(infile->fd), &st) < 0) {
                st.st_mode = 0600;
            }

            /* save file string */
            for (ll = 0; ll < CELLSIZE; ll++) {
                if ((outfile->t_name[ll] =
                        outfile->f_name[ll] =
                        fbuf.f->ch[ll]) == '\0') {
                    break;
                }
            }
            outfile->name_size = ll;
            outfile->t_name[ll++] = '.';
            outfile->t_name[ll++] = 't';
            outfile->t_name[ll++] = 'm';
            outfile->t_name[ll++] = 'p';
            outfile->t_name[ll] = '\0';
            if (!(outfile->fd = fopen(outfile->t_name, "w"))) {
                if (!colonflag) {
                    ERROR(E_COF);
                } else {
                    fclose(infile->fd);
                    infile->fd = NULL;
                }
            } else {
                chmod(outfile->t_name, st.st_mode & 0777);
                outfile->bak = 1;	/* set backup mode */
            }
        }
        infile->eofsw = -1 - (esp->val1 = (infile->fd) ? -1 : 0);
        esp->flag1 = colonflag;
        colonflag = 0;
        break;

    case 'x':				/* finish edit and exit */
        exitflag = -1;

        /* VVV --- fall through to "EC" --- VVV */

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
            while ((c = getc(infile->fd)) != EOF) {
                putc(c, outfile->fd);
            }
        }

        /* VVV --- fall through to "EF" --- VVV */

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
            /* remove it */
            if (!(outfile->bak & 8)) {
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
                eisw = NULL;

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
            if (!colonflag) {
                ERROR(E_FNF);
            }

        /* if this "ei" came from the command string */
        } else if (!eisw) {
            /* save current command string */
            oldcstring.f = cbuf.f;
            oldcstring.z = cbuf.z;

            /* and make it inaccessible to "rdcmd" */
            cbuf.f = NULL;
        }

        /* if a command file had been open, close it */
        if (eisw) {
            fclose(eisw);
        }
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
        if (!read_filename(0, 'r')) {
            infile = &pi_file;
        } else {
            /* close previously-open file */
            if (infile->fd) {
                    fclose(infile->fd);
            }
            if (!(infile->fd = fopen(fbuf.f->ch, "r"))) {
                if (!colonflag) {
                    ERROR(E_FNF);
                }
            }
        }
        infile->eofsw = -1 - (esp->val1 = (infile->fd) ? -1 : 0);
        esp->flag1 = colonflag;
        colonflag = 0;
        esp->op = OP_START;
        break;

    case 'w':	/* specify output file, or switch to primary output */
        if(!read_filename(0, 'w')) {
                outfile = &po_file;
        } else {
            /* output file already open */
            if (outfile->fd) {
                ERROR(E_OFO);
            }

            /* save file string */
            for (ll = 0; ll < CELLSIZE; ll++) {
                if ((outfile->t_name[ll] =
                        outfile->f_name[ll] =
                        fbuf.f->ch[ll]) == '\0') {
                    break;
                }
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
            if (!(outfile->fd = fopen(outfile->t_name, "w"))) {
                if (!colonflag) {
                    ERROR(E_COF);
                }
            }

            /* save "temp suffix" status */
            outfile->bak = ez_val & EZ_NOTMPFIL;

            /* Return status for colon */
            if ( (esp->flag1 = colonflag) ) {
                esp->val1 = (outfile->fd ? -1 : 0);
            }
        }
        break;

    case 'y':			/* EY is Y without protection */
        if (esp->flag1) {
            ERROR(E_NYA);
        }
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
        /* do this as a separate routine */
        esp->val1 = do_eq();
        esp->flag1 = colonflag;
        colonflag = 0;
        esp->op = OP_START;
        break;

    default:
        ERROR(E_IEC);
    }
}

/*
 * routine to execute a system command
 * this is done by forking off another process
 * to execute a shell via 'execl'
 * routine returns -1 if success, 0 if error in fork
 */
int
do_eq(void)
{
    int t;
    int status;
    char *pname;				/* pointer to name of shell */
    extern char *getenv();

    build_string(&sysbuf);

    /* command must fit within one cell */
    if (sysbuf.z > CELLSIZE-2) {
        ERROR(E_STL);
    }

    /* store terminating null */
    sysbuf.f->ch[sysbuf.z] = '\0';

    /* read shell name */
    if (!(pname = getenv("SHELL"))) {
        ERROR(E_SYS);
    }

    /* if not m,nEQ command */
    if (!esp->flag1) {
        /* restore full screen */
        if (win_data[7]) {
            window(WIN_SUSP);
        }

        /* force characters out */
        crlf();

        /* restore terminal to normal mode */
        setup_tty(TTY_SUSP);

        /* fork a new process */
        t = fork();

        /* if this is the child */
        if (t == 0) {
            /* call the named Unix routine */
            execl(pname, pname, "-c", &sysbuf.f->ch[0], NULL);

            /* normally shouldn't get here */
            printf("Error in 'execl'\n");
            exit(1);
        }

        /* if parent, wait for child to finish */
        while (wait(&status) != t) {
            ;
        }

        /* keep failure indication from child */
        if (status & 0xFF) {
            t = -1;
        }

        /* restore terminal to teco mode */
        setup_tty(TTY_RESUME);

        /* if window was enabled */
        if (win_data[7]) {
            /* set reverse video */
            vt(VT_SETSPEC1);

            /* require CR before continuing */
            fputs("Type RETURN to continue", stdout);

            /* reverse video off */
            vt(VT_CLRSPEC);

            while (gettty() != LF) {
                ;
            }

            /* back to beginning of line */
            putchar(CR);

            /* and erase the message */
            vt(VT_EEOL);

            /* re-enable window */
            window(WIN_RESUME);

            /* and make it redraw afterwards */
            window(WIN_REDRAW);
        }
    } else {
        /* m,nEQ command */
        t = do_eq1(pname);
    }

    /* return failure if fork failed or proc status nonzero */
    return( (t == -1) ? 0 : -1);
}

/*
 * Execute m,nEQtext$ command.  Run "text" as a Unix command that
 * receives its std input from chars m through n of teco's buffer.
 * Output from the command is placed in Q#.
 *
 * arg is pointer to shell name
 */
static int
do_eq1(char *shell)
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
    if (pipe(pipe_out)) {
        ERROR(E_SYS);
    }

    /* disable split screen */
    if (win_data[7]) {
        window(WIN_SUSP);
    }

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
        while (wait(&status) != ff) {
            ;
        }
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

    /* won't read from "output" pipe */
    close(pipe_out[0]);

    /* make the "std in" pipe for the process, quit if can't */
    if (pipe(pipe_in)) {
        exit(1);
    }

    /* fork to two processes (total 3), exit if error */
    if ((ff = fork()) == -1) {
        exit(1);
    }

    /* parent - will send chars */
    if (ff) {
        close(pipe_in[0]);	/* won't read from this pipe */

        /* open pipe for writing; exit if open fails */

        if ((xx_in = fdopen(pipe_in[1], "w")) == 0) {
            exit(1);
        }

        /* write to stream, CRLF becomes LF */
        write_stream(xx_in, &aa, ll, 0);
        fclose(xx_in);

        /* wait for child */
        while (wait(&status) != ff) {
            ;
        }

        /* exit with child's status */
        exit(status & 0xFF);

    }

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
    execl(shell, shell, "-c", &sysbuf.f->ch[0], NULL);
    fputs("execl failed\n", stderr);
    exit(1);
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

int
do_en(void)
{
    int t;

    /* if a file string is specified */
    if (build_string(&fbuf)) {

        /* upper limit on string length */
        if (fbuf.z > CELLSIZE-2) {
            ERROR(E_STL);
        }

        /* terminating null */
        fbuf.f->ch[fbuf.z] = '\0';

        /* glob the result */
        t = do_glob(&en_buf);

        /* set the buffer pointer to the beginning of the buffer */
        en_ptr.p = en_buf.f;
        en_ptr.dot = en_ptr.c = 0;

    } else {

        /* if no string, get next filename */
        do_en_next();

        /* t zero if no more filespecs */
        t = (fbuf.z) ? -1 : 0;

        /* if no colon, end of spec is an error */
        if (!t && !colonflag) {
            ERROR(E_NFI);
        }
    }
    return (t);
}

/*
 * Routine to expand the string in the filespec buffer
 * argument is the address of a qh that gets the expanded string
 * argument->v gets set to the number of file specs found
 */
static int
do_glob(struct qh *lbuff)
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
    sprintf(glob_cmd, "ls -1 ");
    for (t = 0; t < fbuf.z +1; t++) {
        glob_cmd[t+6] = fbuf.f->ch[t];
    }

    /* make a pipe */
    if (pipe(glob_pipe)) {
        ERROR(E_SYS);
    }

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

            /*
             * count newline chars that separate file specs;
             *  make it a null char to terminate each filename.
             */
            if (c == '\n') {
                ++lbuff->v;
                c = '\0';
            }

            /* store them in buffer */
            glob_ptr.p->ch[glob_ptr.c] = c;
            fwdcx(&glob_ptr);
        }

        /* through with stream */
        fclose(xx_out);

        /* save character count */
        lbuff->z = glob_ptr.dot;

        /* wait for child to finish */
        while (wait(&status) != t) {
                ;
        }
        setup_tty(TTY_RESUME);

        /* return success unless child exited nonzero */
        return((status & 0xFF) ? 0 : -1);
    }

    /* this is the child */

    /* child won't read */
    close(glob_pipe[0]);

    /* substitute pipe for standard out */
    dup2(glob_pipe[1], fileno(stdout));

    /* don't need that anymore */
    close(glob_pipe[1]);

    /* execute the "glob" */
    execl("/bin/sh", "sh", "-c", glob_cmd, NULL);
    fputs("execl failed\n", stderr);
    exit(1);
}

/* routine to get next file spec from "EN" list into filespec buffer */
static void
do_en_next(void)
{
    char c;

    /* initialize the filespec buffer */
    make_buffer(&fbuf);
    fbuf.z = 0;

    /* stop at end of string */
    while (en_ptr.dot < en_buf.z) {
        c = en_ptr.p->ch[en_ptr.c];
        fwdc(&en_ptr);

        /* null is terminator between file specs */
        if (!c) {
            break;
        }

        /* store char */
        fbuf.f->ch[fbuf.z++] = c;

        /* limit on filespec size */
        if (fbuf.z >= CELLSIZE-1) {
            ERROR(E_STL);
        }
    }

    /* filespec ends with NULL */
    fbuf.f->ch[fbuf.z] = '\0';
}

/*
 * routine to read a file name
 * reads it into fbuf text area
 * returns nonzero if a name, 0 if none
 * flag nonzero => empty name clears filespec buffer
 * func is 'r' for ER or EB cmds, 'i' for EI, 'w' for EW
 */
static int
read_filename(int flag, char func)
{
    int i, t, expand;
    char c;
    struct qh temp_buff;		/* temp buffer for globbing filespec */

    /* if no name spec'd */
    if (!(t = build_string(&fbuf))) {
        /* if no name spec'd, set length to 0 */
        if (flag) {
            fbuf.z = 0;
        }
        return(0);
    }

    if (fbuf.z > CELLSIZE-2) {
        ERROR(E_STL);
    }

    /* store terminating NULL */
    fbuf.f->ch[fbuf.z] = '\0';

    /* check for characters to be expanded by the shell */

    for (i = 0; i < fbuf.z; i++) {
        if ((c = fbuf.f->ch[i]) == '*' || c == '?' ||
                c == '[' || c == 0173) {
            break;
        }
    }

    /* one of these was found, or first char is ~ */
    if ( (expand = (i < fbuf.z)) || fbuf.f->ch[0] == '~') {

        /* make a temp buffer to glob filename into */
        temp_buff.f = NULL;
        make_buffer(&temp_buff);

        /* expand the file name */
        do_glob(&temp_buff);

        /* no match */
        if (temp_buff.z == 0) {
            /* return the storage */
            free_blist(temp_buff.f);

            /* "can't open" or "file not found" */
            ERROR(func == 'w' ? E_COF : E_FNF);
        } else if (temp_buff.v == 0) {
            /* if exactly one file name */

            /* return the old file spec */
            free_blist(fbuf.f);

            /* put the temp buffer there instead */
            fbuf.f = temp_buff.f;
            fbuf.z = temp_buff.z;
            if (fbuf.z > CELLSIZE-2)
                    ERROR(E_STL);
            fbuf.f->ch[fbuf.z] = '\0';

            /*
             * if this is EW and 'twas from a
             * wildcard expansion
             */
            if (func == 'w' && expand) {

                /*
                 * "file XXXX already exists:
                 * overwrite? [yn]"
                 */
                vt(VT_SETSPEC1);
                fputs("File ", stdout);
                fputs(fbuf.f->ch, stdout);
                fputs(" already exists: overwrite? [ny] ", stdout);
                vt(VT_CLRSPEC);

                /* read user's response */
                c = gettty();
                putchar(CR);

                /* clean up the screen */
                vt(VT_EEOL);

                /* abort here */
                if (c != 'y') {
                    ERROR(E_COF);
                }
            }
        } else {
            /* multiple file specs */

            /* if multiple file specs here aren't allowed */
            if (func != 'r' || !(ez_val & EZ_MULT)) {

                /* return the storage */
                free_blist(temp_buff.f);
                ERROR(E_AMB);
            }

            /* substitute the expansion for the "EN" list */
            free_blist(en_buf.f);

            /* and initialize the "EN" list pointer */
            en_ptr.p = en_buf.f = temp_buff.f;
            en_buf.z = temp_buff.z;
            en_ptr.dot = en_ptr.c = 0;

            /* get the first file */
            do_en_next();
        }
    }
    return(t);
}



/* fetch or set variable */
void
set_var(int *arg)
{
    /* if an argument, then set the variable */
    if (esp->flag1) {
        /* if two arguments, then <clr>, <set> */
        if (esp->flag2) {
            *arg = (*arg & ~esp->val2) | esp->val1;
        } else {
            /* one arg is new value */
            *arg = esp->val1;
        }
        esp->flag2 = esp->flag1 = 0;	/* consume argument */
    } else {
        /* otherwise fetch the variable's value */
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
int
read_file(struct qp *lbuff, int *nchars, int endsw)
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
static int
read_stream(FILE *file, int *ff_found,
    struct qp *lbuff, int *nchars, int endsw, int crlf_sw, int ff_sw)
{
    int chr, crflag, c;
    struct buffcell *p;

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
                (endsw > 0 && --endsw == 0))) {
            break;
        }
    }
    (*lbuff).p = p;			/* update pointer */
    (*lbuff).c = c;

    /* set flag to indicate whether FF found */
    if (ff_found) {
        *ff_found = (chr == FF) ? -1 : 0;
    }

    /* and return "eof found" value */
    return( (chr == EOF) ? -1 : 0);
}

/*
 * routine to write text buffer out to selected output file
 * arguments are qp to start of text, number of characters,
 * and an "append FF" switch
 */
void
write_file(struct qp *lbuff, int nchars, int ffsw)
{
    if (!outfile->fd && (nchars)) {
        ERROR(E_NFO);
    } else {
        write_stream(outfile->fd, lbuff, nchars, ez_val & EZ_CRLF);
    }
    if (outfile->fd && ffsw) {
        putc(FF, outfile->fd);
    }
}


/*
 * routine to write text buffer to I/O stream.  Used by
 * write_file, above, and by "eq" write to pipe to other
 * Unix processes.  Arguments buff, nchars as above; file
 * is stream pointer, crlf_sw zero converts CRLF to LF
 */
static void
write_stream(FILE *file, struct qp *lbuf, int nchars, int crlf_sw)
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

/*
 * Routine to kill output file: argument is pointer to an output file structure */
void
kill_output(struct outfiledata *outptr)
{
    if (outptr->fd) {
        fclose(outptr->fd);
        unlink(outptr->t_name);
        outptr->fd = NULL;
    }
}

/*
 * "panic" routine called when "hangup" signal occurs
 * this routine saves the text buffer and closes the output files
 */

/* name of file created to save buffer */
char panic_name[] = "TECO_SAVED.tmp";

void
panic(void)
{
    /* if buffer nonempty and no file open, make one */
    if (!outfile->fd && z) {
        outfile->fd = fopen(panic_name, "w");
    }

    /* set a pointer to start of text buffer */
    set_pointer(0, &aa);

    /* and write out buffer unless "open" failed */
    if (outfile->fd && z) {
        write_file(&aa, z, 0);
    }

    /* close any open output files */
    if (po_file.fd) {
        fclose(po_file.fd);
        po_file.fd = NULL;
    }

    if (so_file.fd) {
        fclose(so_file.fd);
        so_file.fd = NULL;
    }
}

/* do "F" commands */
void
do_f(void)
{
    struct buffcell *delete_p;

    /* read next character and dispatch */
    switch (mapch_l[getcmdc(trace_sw) & 0xFF]) {

    /* back to beginning of current iteration */
    case '<':
        /* if in iteration */
        if (cptr.flag & F_ITER) {
            cptr.p = cptr.il->p;	/* restore */
            cptr.c = cptr.il->c;
            cptr.dot = cptr.il->dot;

        /* else, restart current macro */
        } else {
            for (cptr.dot = cptr.c = 0; cptr.p->b->b != NULL;
                    cptr.p = cptr.p->b) {
                ;
            }
        }
        break;

    /* to end of current iteration */
    case '>':
        find_enditer();	/* find it */

        /* if exit, exit appropriately */
        if ( ( ((esp->flag1) ? esp->val1 : srch_result) >= 0) ?
                (~colonflag) : colonflag) {
            pop_iteration(0);
        }
        break;

    /* to end of conditional */
    case '\'':
    case '|':					/* to "else," or end */
        find_endcond(cmdc);
        break;

    /* "F" search commands */

    /* bounded search, alternative args */
    case 'b':
        do_fb();
        break;

    /* bounded search, alternative args, then "FR" */
    case 'c':
        if (do_fb()) {
            goto do_fr;
        }

        /* otherwise skip insert string */
        while (getcmdc(trace_sw) != term_char) {
            ;
        }
        break;

    /* do "N" and then "FR" */
    case 'n':
        if (do_nsearch('n')) {
            goto do_fr;
        }

        /* otherwise skip insert string */
        while (getcmdc(trace_sw) != term_char) {
            ;
        }
        break;

    /* do "_" and then "FR" */
    case '_':
        if (do_nsearch('_')) {
            goto do_fr;
        }

        /* otherwise skip insert string */
        while (getcmdc(trace_sw) != term_char) {
            ;
        }
        break;

    /* search and replace: search, then do "FR" */
    case 's':
        /* read search string and terminator */
        build_string(&sbuf);

        /* if search passed, do "FR" */
        if (end_search(  do_search( setup_search() )  )) {
            goto do_fr;
        }

        /* otherwise skip insert string */
        while (getcmdc(trace_sw) != term_char) {
            ;
        }
        break;

    /* replace last insert, search, etc. */
    case 'r':
        /* shouldn't have argument */
        if (esp->flag1) {
            ERROR(E_NFR);
        }

        /* set terminator */
        term_char = (atflag) ? getcmdc(trace_sw) : ESC;
        atflag = 0;

do_fr:		/* entry from FN, F_, and FC */

        /* save a pointer to the current spot */
        set_pointer(dot, &cc);

        /* back dot up over the string */
        dot += ctrl_s;

	/*
	 * Record replace as delete+insert.
	 * Here, we note the part being removed.
	 */
	undo_del(dot, -ctrl_s);

        /* code from "insert1": convert dot to a qp */
        set_pointer(dot, &aa);

        /* save beginning of original cell */
        delete_p = aa.p;

        /* update earliest char loc touched */
        if (dot < buff_mod) {
            buff_mod = dot;
        }

        /* get a new cell */
        insert_p = bb.p = get_bcell();
        bb.c = 0;

        /* save char position of dot in cell */
        ins_count = aa.c;
        aa.c = 0;

        /* copy cell up to dot */
        movenchars(&aa, &bb, ins_count);

        /* insert */
        moveuntil(&cptr, &bb, term_char, &ins_count,
                cptr.z-cptr.dot, trace_sw);

        /* advance command-string pointer */
        cptr.dot += ins_count;

        /* skip terminator */
        getcmdc(trace_sw);

        /* subtract delete length from buffer count */
        z += ctrl_s;

        /* put the new cell where the old one was */
        delete_p->b->f = insert_p;

        /* code borrowed from "insert2" */
        insert_p->b = delete_p->b;
        insert_p = NULL;

        /* if replacement text was same length, we can save time */
        if (bb.c == cc.c) {
            /* copy rest of cell */
            for (; bb.c < CELLSIZE; bb.c++) {
                bb.p->ch[bb.c] = cc.p->ch[bb.c];
            }

            /*
             * Replace orig cell's place in chain with
             * end of new list
             */
            bb.p->f = cc.p->f;
            if (bb.p->f) {
                bb.p->f->b = bb.p;
            }

            /* terminate the part snipped out */
            cc.p->f = NULL;
            free_blist(delete_p);	/* return the old part */

        } else {
            /*
             * Different positions: shift the remainder
             * of the buffer
             */

            /* splice rest of buffer to end */
            bb.p->f = delete_p;
            delete_p->b = bb.p;


            /*
             * Squeeze buffer, return unused cells, end buffer
             */
            movenchars(&cc, &bb, z-dot);
            free_blist(bb.p->f);
            bb.p->f = NULL;
        }

	/* Second part of undo: note amount inserted */
	undo_insert(dot, ins_count);

        /* add # of chars inserted */
        z += ins_count;
        dot += ins_count;
        ctrl_s = -ins_count;		/* save string length */
        esp->flag1 = esp->flag2 = 0;	/* and consume arguments */
        esp->op = OP_START;
        break;

    case 'u':		/* Control undo */

	/* Unexpected modifiers */
	if (esp->flag2 || colonflag) {
            ERROR(E_UND);
	}

	/* fu, -1fu -- roll back one level */
	if ((!esp->flag1) || (esp->val1 == -1)) {
	    roll_back();

	/* 1fu -- roll forward one previously undone level */
	} else if (esp->val1 == 1) {
	    roll_forward();

	/* Unknown */
	} else {
            ERROR(E_UND);
	}
	break;

    default:
        ERROR(E_IFC);
    }
}

/*
 * routines for macro iteration
 * pop iteration: if arg nonzero, exit unconditionally
 * else check exit conditions and exit or reiterate
 */
void
pop_iteration(int arg)
{
    /* if reiteration */
    if (!arg && (!cptr.il->dflag || (--(cptr.il->count) > 0)) ) {
        cptr.p = cptr.il->p;		/* restore */
        cptr.c = cptr.il->c;
        cptr.dot = cptr.il->dot;
    } else {
        /* if not last thing on stack, back up */
        if (cptr.il->b) {
            cptr.il = cptr.il->b;
        } else {
            /* else clear "iteration" flag */
            cptr.flag &= ~F_ITER;
        }
    }
}


/* find end of iteration - read over arbitrary <> and one > */
void
find_enditer(void)
{
    int icnt;

    /* scan for matching > */
    for (icnt = 1; icnt > 0;) {
        /* scan for next < or > */
        while ((skipto(0) != '<') && (skipc != '>')) {
            ;
        }

        /* and keep track of macro level */
        if (skipc == '<') {
            ++icnt;
        } else {
            --icnt;
        }
    }
}

/* find end of conditional */
static void
find_endcond(char arg)
{
    int icnt;

    for (icnt = 1; icnt > 0;) {
        while ((skipto(0) != '"') && (skipc != '\'') && (skipc != '|')) {
            ;
        }
        if (skipc == '"') {
            ++icnt;
        } else if (skipc == '\'') {
            --icnt;
        } else if ((icnt == 1) && (arg == '|')) {
            break;
        }
    }
}
