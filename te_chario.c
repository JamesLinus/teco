/* TECO for Ultrix   Copyright 1986 Matt Fichtenbaum						*/
/* This program and its components belong to GenRad Inc, Concord MA 01742 */
/* They may be copied if this copyright notice is included */

/* te_chario.c   character I/O routines   10/9/86 */
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <termios.h>
#include <errno.h>
#include "te_defs.h"

#include <fcntl.h>
#ifndef DEBUG
extern void int_handler();
#ifdef SIGTSTP
extern void stp_handler();
#endif
extern void hup_handler();
#define SIGINTMASK 2
#endif
int lf_sw;	/* nonzero: make up a LF following an entered CR */

/* original and new tty flags */
struct termios tty_orig, tty_new, tc_noint;

/* character which back-deletes a character in interactive mode */
char delchar = DEL;

int inp_noterm;		/* nonzero if standard input is not a terminal */
int out_noterm;		/* nonzero if standard output is not a terminal */

static char backpush;	/* Emulate BSD push-back function */

static sigset_t intrmask;	/* Mask w. interrupts disabled */
#ifdef SIGTSTP
static sigset_t suspmask;	/* Mask w. ^Z/^Y disabled */
#endif

static void qio_char(char c);

/*
 * sig()
 *      Dispatch signal handling
 */
static void
sig(int signum, void (*handler)(int))
{
    struct sigaction sa;

    bzero(&sa, sizeof(sa));
    sa.sa_handler = stp_handler;
    sigaction(signum, &sa, 0);
}

/*
 * set tty (stdin) mode.  TECO mode is CBREAK, no ECHO, sep CR & LF
 * operation; normal mode is none of the above.  TTY_OFF and TTY_ON do this
 * absolutely; TTY_SUSP and TTY_RESUME use saved signal status.
 */
void
setup_tty(int arg)
{
    int ioerr;
    struct termios tmpbuf;
    static int done_init = 0;

    /*
     * Establish signal masks for our major modes
     */
    if (!done_init) {
        sigemptyset(&intrmask);
        sigaddset(&intrmask, SIGINT);
#ifdef SIGTSTP
        sigemptyset(&suspmask);
        sigaddset(&intrmask, SIGTSTP);
#endif
        done_init = 1;
    }

    /* initial processing: set tty mode */

    if (arg == TTY_ON) {
        /* get std input characteristics */
        ioerr = tcgetattr(fileno(stdin), &tty_orig);

        /* nonzero if input not a terminal */
        inp_noterm = (ioerr && (errno == ENOTTY));
        if (!inp_noterm) {
            delchar = tty_orig.c_cc[VERASE];
        }

        /* get std output characteristics */
        ioerr = tcgetattr(fileno(stdout), &tmpbuf);

        /* nonzero if output not a terminal */
        out_noterm = (ioerr && (errno == ENOTTY));

        /* make a copy of tty control structure */
        tty_new = tty_orig;

        /* turn on teco modes */
        tty_new.c_iflag &= ~(INLCR|IGNCR|ICRNL|IXON|IXOFF);
        tty_new.c_lflag &= ~(ICANON|ECHO);
        tty_new.c_lflag |= (ISIG);
        tty_new.c_oflag &= ~(ONLCR);
        tty_new.c_cc[VMIN] = 1;
        tty_new.c_cc[VTIME] = 0;

        /* disable the interrupt char in this one */
        tc_noint = tty_new;
        tc_noint.c_cc[VINTR] = -1;
    }

    if ((arg == TTY_ON) || (arg == TTY_RESUME)) {
        /* Set up TTY for TECO */
        tcsetattr(fileno(stdin), TCSAFLUSH, &tty_new);
#ifndef DEBUG
        /* Handle signals */
#ifdef SIGTSTP
        sig(SIGTSTP, stp_handler);
#endif
        sig(SIGINT, int_handler);
        sig(SIGHUP, hup_handler);
#endif /* DEBUG */
    } else {
        /* Restore to original state */
        tcsetattr(fileno(stdin), TCSAFLUSH, &tty_orig);
#ifndef DEBUG
#ifdef SIGTSTP
        sig(SIGTSTP, SIG_DFL);
#endif
        sig(SIGINT, SIG_DFL);
        sig(SIGHUP, SIG_DFL);
#endif
    }
}

/* routines to handle keyboard input */

/*
* routine to get a character without waiting, used by ^T when ET & 64 is
* set if lf_sw is nonzero, return the LF; else use the FNDELAY fcntl to
* inquire of the input
*/
int
gettty_nowait(void)
{
    char c;
    int err, cnt;

again:
    if (lf_sw) {
        lf_sw = 0;
        return(LF);		/* LF to be sent: return it */
    }

    /* If have pushback, return it */
    if (backpush) {
        c = backpush;
        backpush = '\0';
        return(c);
    }

    /* Clear EOF if set */
    if (feof(stdin)) {
        clearerr(stdin);
    }

    /* set to "no delay" mode */
    tty_new.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSAFLUSH, &tty_new);

    /* read character, or -1, skip nulls */
    do {
        cnt = read(fileno(stdin), &c, sizeof(c));
    } while ((cnt > 0) && (c == '\0'));
    err = errno;

    /* reset to normal mode */
    tty_new.c_cc[VMIN] = 1;
    tcsetattr(fileno(stdin), TCSAFLUSH, &tty_new);

    /* If interrupted, try again */
    if ((cnt < 0) && (err == EINTR)) {
        goto again;
    }

    /* No data--return -1 */
    if (cnt == 0) {
        return(-1);
    }

    /* CR: set switch to make up a LF */
    if (c == CR) {
        ++lf_sw;
    }
    return(c & 0xFF);
}

/* normal routine to get a character */

int in_read = 0; /* flag for "read busy" (used by interrupt handler) */

int
gettty(void)
{
    int c;

again:
    /* if switch set, make up a line feed */
    if (lf_sw) {
        lf_sw = 0;
        return(LF);
    }

    /* If have pushback, return it */
    if (backpush) {
        c = backpush;
        backpush = '\0';
        return(c);
    }

    ++in_read;			/* set "read busy" switch */

    /* get character; skip nulls */
    while(!(c = getchar())) {
        ;
    }

    /* Interrupted--try again */
    if ((c == -1) && (errno == EINTR)) {
        goto again;
    }

    c &= 0x7F;
    in_read = 0;			/* clear switch */
    if (c == CR) {
        ++lf_sw;		/* CR: set switch to make up a LF */
    }
    if (c == EOF) {
        ERROR(E_EFI);		/* end-of-file from standard input */
    }
    return(c);
}

#ifndef DEBUG

/* routine to handle interrupt signal */

void
int_handler()
{
    /* if executing commands */
    if (exitflag <= 0) {

        /* if "trap ^C" set, clear it and ignore */
        if (et_val & ET_CTRLC) {
            et_val &= ~ET_CTRLC;
        } else {
            /* else set flag to stop execution */
            exitflag = -2;
        }
    }

    /* if interrupt happened in "getchar" pass a ^C to input */
    if (in_read) {
        /* clear "read" switch */
        in_read = 0;

        /* disable interrupt char */
        tcsetattr(fileno(stdin), TCSAFLUSH, &tc_noint);

        /* send a ^C to input stream */
        qio_char(CTL('C'));

        /* reenable interrupt char */
        tcsetattr(fileno(stdin), TCSAFLUSH, &tty_new);
    }
}
#endif

/*
 * routine to disable (1), enable (0) ^C interrupt, used to block
 * interrupts during display update
 */
sigset_t oldmask;	/* storage for previous signal mask */

void
block_inter(int func)
{
#ifndef DEBUG
    /* if arg nonzero, block interrupt */
    if (func) {
        sigprocmask(SIG_BLOCK, &intrmask, &oldmask);
    } else {
        /* otherwise restore old signal mask */
        sigprocmask(SIG_SETMASK, &oldmask, (sigset_t *)0);
    }
#endif
}

#if defined(SIGTSTP)
/* routine to handle "stop" signal (^Y) */
void
stp_handler()
{
    window(WIN_SUSP);		/* restore screen */
    setup_tty(TTY_SUSP);		/* put tty back to normal */
    sig(SIGTSTP, SIG_DFL); 	/* put default action back */
    sigprocmask(SIG_UNBLOCK, &suspmask, 0);	/* unblock "suspend" signal */
    kill(0, SIGTSTP);		/* suspend this process */

    /* ----- process gets suspended here ----- */

    /* restore local handling of "stop" signal */
    sig(SIGTSTP, stp_handler);
    setup_tty(TTY_RESUME);			/* restore tty */
    buff_mod = 0;				/* set whole screen modified */

    /* redraw window */
    if (win_data[7]) {
        window(WIN_RESUME);		/* re-enable window */
        window(WIN_REDRAW);		/* force complete redraw */
        window(WIN_REFR);		/* and refresh */
    }
    qio_char('\0');				/* wake up the input */

    /* if not executing, prompt again and echo command string so far */
    if (exitflag) {
        retype_cmdstr('*');
    }
}
#endif

/* simulate a character's having been typed on the keyboard */
static void
qio_char(char c)
{
    backpush = c;
}

/* routine to handle "hangup" signal */
#ifndef DEBUG

void
hup_handler()
{
    /* if executing, set flag to terminate */
    if (!exitflag) {
        exitflag = -3;
    } else {
        /* dump buffer and close output files */
        panic();
        exit(1);
    }
}
#endif



/* type a crlf */
void
crlf(void)
{
    type_char(CR);
    type_char(LF);
}



/* routine to type one character */
void
type_char(char ch)
{
    int c = ((int)ch) & 0xFF;

    /* spacing char beyond end of line */
    if ((char_count >= WN_width) && (c != CR) && !(spec_chars[c] & A_L)) {

        /* truncate output to line width */
        if (et_val & ET_TRUNC) {
            return;
        } else {
            /*
             * otherwise do automatic new line (note
             * recursive call to type_char)
             */
            crlf();
        }
    }

    /* control char? */
    if ((c & 0140) == 0) {
        switch (c & 0177) {
        case CR:
            putchar(c);
            char_count = 0;
            break;

        case LF:
            putchar(c);
#ifdef NOTDEF
            scroll_dly(); /* filler chars in case VT-100 scrolls */
#endif
            break;

        case ESC:
            if ((et_val & ET_IMAGE) && !exitflag) {
                putchar(c);
            } else {
                putchar('$');
                char_count++;
            }
            break;

        case TAB:
            if ((et_val & ET_IMAGE) && !exitflag) {
                putchar(c);
            } else {
                for (type_char(' '); (char_count & tabmask) != 0;
                        type_char(' ')) {
                    ;
                }
            }
            break;

        default:
            if ((et_val & ET_IMAGE) && !exitflag) {
                putchar(c);
            } else {
                putchar('^');
                putchar(c + 'A'-1);
                char_count += 2;
            }
            break;
        }
    } else {
        putchar(c);
        char_count++;
    }
}
