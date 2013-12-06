/* TECO for Ultrix   Copyright 1986 Matt Fichtenbaum */
/* This program and its components belong to GenRad Inc, Concord MA 01742 */
/* They may be copied if this copyright notice is included */

/* exec0.c   execute command string   11/31/86 */
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include "defs.h"

void
exec_cmdstr(void)
{
    int digit_sw;
    struct tm *timeptr;
    char *timestring, *asctime();
    time_t t;

    exitflag = 0;		/* set flag to "executing" */
    cmdstr.p = cbuf.f;	/* cmdstr to start of command string */
    cmdstr.z = cbuf.z;

    /* clear char ptr and iteration flag */
    cmdstr.flag = cmdstr.dot = cmdstr.c = 0;
    msp = &cmdstr;	/* initialize macro stack pointer */
    esp = &estack[0];	/* initialize expression stack pointer */
    qsp = &qstack[-1];	/* initialize q-reg stack pointer */
    atflag = colonflag = 0;	/* clear flags */
    esp->flag2 =
            esp->flag1 = 0;	/* initialize expression reader */
    esp->op = OP_START;
    trace_sw = 0;	/* start out with trace off */
    digit_sw = 0;	/* and no digits read */

    /* until end of command string */
    while (!exitflag) {
        /* interpret next char as corresp. control char */
        if (getcmdc0(trace_sw) == '^') {
            cmdc = getcmdc(trace_sw) & 0x1f;
        }

        /* process number */
        /* this works lousy for hex but so does TECO-11 */
        if (isdigit(cmdc)) {
            /* invalid digit */
            if (cmdc - '0' >= ctrl_r) {
                ERROR(E_ILN);
            }

            /* first digit */
            if (!(digit_sw++)) {
                esp->val1 = cmdc - '0';
            } else {
                /* other digits */
                esp->val1 = esp->val1 * ctrl_r + cmdc - '0';
            }
            esp->flag1++;		/* indicate a value read in */
            continue;
        }

        /* not a digit: dispatch on character */
        digit_sw = 0;
        switch (mapch_l[cmdc & 0xFF]) {

        /* characters ignored */
        case CR:
        case LF:
        case VT:
        case FF:
        case ' ':
            break;

        /*
         * ESC: one absorbs argument,
         * two terminate current level
         */
        case ESC:
            /* if next char is an ESC */
            if (peekcmdc(ESC)) {
                /* pop stack; if empty, terminate */
                if (msp <= &mstack[0]) {
                    exitflag = 1;
                } else {
                    --msp;
                }
            } else {
                /* else consume argument */
                esp->flag1 = 0;
                esp->op = OP_START;
            }
            break;

        /* skip comments */
        case '!':
            while (getcmdc(trace_sw) != '!') {
                ;
            }
            esp->flag1 = esp->flag2 = 0;
            break;

        /* modifiers */
        case '@':
            atflag++;
            break;

        case ':':
            /* is it "::" ? */
            if (peekcmdc(':')) {
                /*
                 * yes, skip 2nd :
                 * and set flag to show 2
                 */
                getcmdc(trace_sw);
                colonflag = 2;
            } else {
                /* otherwise just 1 colon */
                colonflag = 1;
            }
            break;

        /* trace control */
        case '?':
            trace_sw = !(trace_sw);
            break;

        /* values */
        case '.':
            esp->val1 = dot;
            esp->flag1 = 1;
            break;

        case 'z':
            esp->val1 = z;
            esp->flag1 = 1;
            break;

        case 'b':
            esp->val1 = 0;
            esp->flag1 = 1;
            break;
        case 'h':
            esp->val1 = z;
            esp->val2 = 0;
            esp->flag2 = esp->flag1 = 1;
            esp->op = OP_START;
            break;

        /* -length of last insert, etc. */
        case CTL('S'):
            esp->val1 = ctrl_s;
            esp->flag1 = 1;
            break;

        case CTL('Y'):		/* .-^S, . */
            esp->val2 = dot + ctrl_s;
            esp->val1 = dot;
            esp->flag1 = esp->flag2 = 1;
            esp->op = OP_START;
            break;

        case '(':
            if (++esp > &estack[ESTACKSIZE-1]) {
                ERROR(E_PDO);
            }
            esp->flag2 = esp->flag1 = 0;
            esp->op = OP_START;
            break;

        case CTL('E'):			/* form feed flag */
            esp->val1 = ctrl_e;
            esp->flag1 = 1;
            break;

        case CTL('N'):			/* eof flag */
            esp->val1 = infile->eofsw;
            esp->flag1 = 1;
            break;

        case CTL('^'):			/* value of next char */
            esp->val1 = getcmdc(trace_sw);
            esp->flag1 = 1;
            break;

        /* date, time */
        case CTL('B'):
        case CTL('H'):
            (void)time(&t);
            timeptr = localtime(&t);
            esp->val1 =
                (cmdc == CTL('B')) ?
                    (timeptr->tm_year * 512
                            + timeptr->tm_mon * 32
                            + timeptr->tm_mday)
                    :
                    (timeptr->tm_hour * 1800
                            + timeptr->tm_min * 30
                            + timeptr->tm_sec/2);
            esp->flag1 = 1;

            /* make a time buffer */
            make_buffer(&timbuf);
            timestring = asctime(timeptr);

            /* copy character string */
            for (timbuf.z = 0; timbuf.z < 24; timbuf.z++) {
                timbuf.f->ch[timbuf.z] = *(timestring + timbuf.z);
            }
            break;

        /* number of characters to matching ( ) { } [ ] */
        case CTL('P'):
            do_ctlp();
            break;

        /*
         * none of the above: incorporate the
         * last value into the expression
         */
        default:
            /* if a value entered */
            if (esp->flag1) {
                switch (esp->op) {
                case OP_START:
                    break;

                case OP_ADD:
                    esp->val1 += esp->exp;
                    esp->op = OP_START;
                    break;

                case OP_SUB:
                    esp->val1 = esp->exp - esp->val1;
                    esp->op = OP_START;
                    break;

                case OP_MULT:
                    esp->val1 *= esp->exp;
                    esp->op = OP_START;
                    break;

                case OP_DIV:
                    esp->val1 = (esp->val1) ?
                            (esp->exp / esp->val1) :
                            0;
                    esp->op = OP_START;
                    break;

                case OP_AND:
                    esp->val1 &= esp->exp;
                    esp->op = OP_START;
                    break;

                case OP_OR:
                    esp->val1 |= esp->exp;
                    esp->op = OP_START;
                    break;
                }
            }	/* end of "if a new value" */

            exec_cmds1();	/* go do the command */
        }	/* end of non-digit command dispatch */
    }		/* end of "while" command loop */
}		/* end of exec_cmdstr */
