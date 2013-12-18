/*
 * TECO for Ultrix   Copyright 1986 Matt Fichtenbaum
 * This program and its components belong to GenRad Inc, Concord MA 01742
 * They may be copied if this copyright notice is included
 */

/* rdcmd.c  read in the command string  12/20/85 */
#include "defs.h"

int ccount;	/* count of chars read in */

static int find_lasteol(void);

int
read_cmdstr(void)
{
    int c;				/* temporary character */
    int i;				/* temporary */

    rev_undo(1);
    goto prompt;

    /* prompt again: new line */
restart:

    /* if input not from a file */
    if (!eisw && !inp_noterm) {
        crlf();
    }

    /* issue prompt */
prompt:
    if (!eisw && !inp_noterm) {
        type_char('*');
    }
    ccount = 0;
    lastc = ' ';

    /* loop to read command string chars */
    for (;;) {

        /* if terminal input */
        if (!eisw && !inp_noterm) {
            /* process rubout */
            if ((c = gettty()) == delchar) {
                /* if at beginning, ignore */
                if (!ccount) {
                    goto restart;
                }

                /* decrement char count */
                --ccount;

                /* back up the command-string pointer */
                backc(&cmdstr);

                /* look at the character just deleted */
                /* control chars: set c to char erased */
                if (((c = cmdstr.p->ch[cmdstr.c]) < ' ') &&
                        (c != ESC)) {

                    /* line up */
                    if (c == LF) {
                            vt(VT_LINEUP);

                    } else if ((c == CR) || (c == TAB)) {

                        /* back up to previous line */
                        i = find_lasteol();

                        /* erase current line */
                        type_char(CR);
                        vt(VT_EEOL);

                        /*
                         * If this was line with
                         * prompt, retype the prompt
                         */
                        if (i == ccount) {
                            type_char('*');
                        }

                        /*
                         * Retype line: stop before
                         * deleted position
                         */
                        for (; (t_qp.p != cmdstr.p) ||
                                (t_qp.c != cmdstr.c);
                                fwdc(&t_qp)) {
                            type_char(t_qp.p-> ch[t_qp.c]);
                        }
                    } else {
                        /* erase ordinary ctrl chars */
                        vt(VT_BS2);
                        char_count -= 2;
                    }
                } else {
                    /* erase printing chars */
                    vt(VT_BS1);
                    char_count--;
                }

                /* disable dangerous last chars */
                lastc = ' ';
                continue;
            } else if (c == CTL('U')) {
                /* process "erase current line" */

                /* erase line */
                type_char(CR);
                vt(VT_EEOL);

                /* back up to last eol: if beginning, restart */
                if ((ccount -= find_lasteol()) <= 0) {
                    goto prompt;
                }

                /* put command pointer back to this point */
                cmdstr.p = t_qp.p;
                cmdstr.c = t_qp.c;
                lastc = ' ';

                /* and read it again */
                continue;
            } else {
                /* not a rubout or ^U */

                /* if at beginning of line */
                if (!ccount) {
                    /* save old command string */
                    if (c == '*') {
                        /* echo character */
                        type_char('*');

                        /* read reg spec and echo */
                        type_char(c = gettty());
                        i = getqspec(0, c);

                        /*
                         * Return its previous
                         * contents
                         */
                        free_blist(qreg[i].f);

                        /*
                         * Put the old command string
                         * in its place
                         */
                        qreg[i].f = cbuf.f;
                        qreg[i].f->b = (struct buffcell *) &qreg[i];
                        qreg[i].z = cbuf.z;

                        /* no old command string */
                        cbuf.f = NULL;

                        /* no previous error */
                        terr = 0;
                        goto restart;

                    } else if ((c == '?') && (terr)) {
                        /*
                         * Echo previous command
                         * string up to error
                         */

                        /* echo ? */
                        type_char('?');

                        /* find beginning */
                        for (aa.p = cptr.p; aa.p->b->b != NULL;
                                aa.p = aa.p->b) {
                            ;
                        }


                        for (aa.c = 0; (aa.p != cptr.p) || (aa.c < cptr.c);
                                fwdc(&aa)) {
                            type_char(aa.p->ch[aa.c]);
                        }

                        /* a final ? */
                        type_char('?');

                        /* reset error switch */
                        terr = 0;
                        goto restart;

                    } else if ((c == LF) || (c == CTL('H'))) {

                        /* line feed, backspace */

                        /*
                         * Pointer up or down
                         * one line
                         */
                        dot += lines( (c == LF) ?  1 : -1);

                        /* display one line */
                        window(WIN_LINE);
                        goto restart;

                    } else {
                        /*
                         * First real command on
                         * a line
                         */

                        /*
                         * Start a command string if
                         * need be
                         */
                        make_buffer(&cbuf);

                        /*
                         * Set cmdstr to point to
                         * start of command string
                         */
                        cmdstr.p = cbuf.f;
                        cmdstr.c = 0;

                        /*
                         * No chars in command string
                         * now
                         */
                        cbuf.z = 0;

                        /* clear last error flag */
                        terr = 0;
                    }
                }	/* end of "if first char on line" */

                /* check ^G-something */

                if (lastc == CTL('G')) {
                    if (c == CTL('G')) {

                        /*
                         * Save count for possible
                         * "save in q-reg"
                         */
                        cbuf.z = ccount;
                        goto restart;
                    }
                    if ((c == '*') || (c == ' ')) {
                        /*
                         * Remove the previous ^G
                         * from buffer
                         */
                        backc(&cmdstr);
                        --ccount;
                        crlf();

                        /*
                         * Retype appropriate part of
                         * command string
                         */
                        retype_cmdstr(c);
                        lastc = ' ';
                        continue;
                    }
                }
            }
        } else {
            /*
             * If input from indirect file or
             * redirected std input
             */

            /* first command? */
            if (!ccount) {

                /* start a command string if need be */
                if (!cbuf.f) {
                    cbuf.f = get_bcell();
                    cbuf.f->b = (struct buffcell *) &cbuf;
                }

                /* point cmdstr to start of command string */
                cmdstr.p = cbuf.f;
                cbuf.z = cmdstr.c = 0;
            }

            c = (eisw) ? getc(eisw) : gettty() ;	/* get char */

            /* if this is end of the indirect command file */
            if (eisw && (c == EOF)) {

                /* close the input file */
                fclose(eisw);

                /* reset the switch */
                eisw = 0;
                lastc = ' ';

                /* and go read more chars */
                continue;
            } else {
                if ((c == LF) && (lastc != CR) &&
                                !(ez_val & EZ_CRLF)) {

                    /* LF: store implied CR first */

                    cmdstr.p->ch[cmdstr.c] = CR;
                    ++ccount;
                    fwdcx(&cmdstr);
                }
            }
        }		/* end of "if redirected std in or eisw" */

        /* store character in command string */

        /* store the character */
        cmdstr.p->ch[cmdstr.c] = c;

        /* keep count of chars */
        ++ccount;

        /* echo the character */
        if (!eisw && !inp_noterm) {
            type_char(c);
        }

        /* next char pos'n; extend command string if nec */
        fwdcx(&cmdstr);

        /* stop on 2nd ESC */
        if ((c == ESC) && (lastc == ESC)) {
            break;
        }

        /* immediate exit */
        if ((c == CTL('C')) && (lastc == CTL('C'))) {
            return(-1);
        }

        /* keep track of last char */
        lastc = c;
    }			/* end of read-char loop */

    /* indicate number of chars in command string */
    cbuf.z = ccount;

    /* final new-line */
    if (!eisw && !inp_noterm) {
        crlf();
    }
    return(0);
}

/*
 * back up to find most recent CR or LF in entered command string
 * return number of chars backed up
 */
static int
find_lasteol(void)
{
    int i;

    /* look for beg. of line */
    for (i = 0, t_qp.p = cmdstr.p, t_qp.c = cmdstr.c;
            (backc(&t_qp)) ; i++) {
        if ((t_qp.p->ch[t_qp.c] == CR) || (t_qp.p->ch[t_qp.c] == LF)) {
            fwdc(&t_qp);	/* stop short of previous eol */
            break;
        }
    }
    char_count = 0;				/* reset tab count */
    return(i);
}

/*
 * retype command string: entirely (arg = '*') or most recent line (arg = ' ')
 */
void
retype_cmdstr(char c)
{
    int i;

    /* if input is really from terminal */
    if (!inp_noterm) {

        /* look for beginning of this line */
        if (c == ' ') {

            /* to last eol, and count char's backed up */
            i = ccount - find_lasteol();
        } else {
            /* retype whole command string */
            t_qp.p = cbuf.f;
            i = t_qp.c = 0;
        }

        /* if from beginning, retype prompt */
        if (!i) {
            type_char('*');
        }

        /* type command string from starting point */
        for (; i < ccount; i++) {
            type_char(t_qp.p->ch[t_qp.c]);
            fwdc(&t_qp);
        }
    }
}
