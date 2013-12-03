/* TECO for Ultrix   Copyright 1986 Matt Fichtenbaum */
/* This program and its components belong to GenRad Inc, Concord MA 01742 */
/* They may be copied if this copyright notice is included */

/* te_exec1.c   continue executing commands   1/8/87 */
#include "te_defs.h"

prnum(val, base)
	int val, base;
{
    switch (base) {
    case 8: printf("%o", val); break;
    case 16: printf("%x", val); break;
    default: printf("%d", val); break;
    }
}

exec_cmds1()
{
    char command;	/* command character */
    int cond;		/* conditional in progress */

    /* operators */
    switch (command = mapch_l[cmdc]) {

    case '+':
        esp->exp = (esp->flag1) ? esp->val1 : 0;
        esp->flag1 = 0;
        esp->op = OP_ADD;
        break;

    case '-':
        esp->exp = (esp->flag1) ? esp->val1 : 0;
        esp->flag1 = 0;
        esp->op = OP_SUB;
        break;

    case '*':
        esp->exp = (esp->flag1) ? esp->val1 : 0;
        esp->flag1 = 0;
        esp->op = OP_MULT;
        break;

    case '/':
        esp->exp = (esp->flag1) ? esp->val1 : 0;
        esp->flag1 = 0;
        esp->op = OP_DIV;
        break;

    case '&':
        esp->exp = (esp->flag1) ? esp->val1 : 0;
        esp->flag1 = 0;
        esp->op = OP_AND;
        break;

    case '#':
        esp->exp = (esp->flag1) ? esp->val1 : 0;
        esp->flag1 = 0;
        esp->op = OP_OR;
        break;

    case ')':
        if ((!esp->flag1) || (esp <= &estack[0])) {
            ERROR(E_NAP);
        }
        --esp;

        /* carry value from inside () */
        esp->val1 = (esp+1)->val1;
        esp->flag1 = 1;
        break;

    case ',':
        if (!esp->flag1) {
            ERROR(E_NAC);
        } else {
            esp->val2 = esp->val1;
        }
        esp->flag2 = 1;
        esp->flag1 = 0;
        break;

    case CTL('_'):
        if (!esp->flag1) {
            ERROR(E_NAB);
        } else {
            esp->val1 = ~esp->val1;
        }
        break;

    /* radix control */
    case CTL('D'):
        ctrl_r = 10;
        esp->flag1 = 0;
        esp->op = OP_START;
        break;

    case CTL('O'):
        ctrl_r = 8;
        esp->flag1 = 0;
        esp->op = OP_START;
        break;

    case CTL('R'):
        /* fetch it */
        if (!esp->flag1) {
            esp->val1 = ctrl_r;
            esp->flag1 = 1;
        } else {
            if ((esp->val1 != 8) && (esp->val1 != 10) &&
                    (esp->val1 != 16)) {
                ERROR(E_IRA);
            }
            ctrl_r = esp->val1;
            esp->flag1 = 0;
            esp->op = OP_START;
        }
        break;

    /* other commands */

    /* 1 ^C stops macro execution, 2 exit */
    case CTL('C'):
        /* 2 ^C: stop execution and exit */
        if (peekcmdc(CTL('C'))) {
            exitflag = -1;

        /* 1 ^C: in command string: stop execution */
        } else if (msp <= &mstack[0]) {
                exitflag = 1;
        } else {
                /* in a macro - pop it */
                --msp;
        }
        break;

    case CTL('X'):			/* search mode flag */
        set_var(&ctrl_x);
        break;

    case 'e':
        do_e();
        break;

    case 'f':
        do_f();
        break;

    /* macro call, iteration, conditional */
    case 'm':				/* macro call */
        /* read the macro name */
        mm = getqspec(0, getcmdc(trace_sw));

        /* check room for another level */
        if (msp > &mstack[MSTACKSIZE-1]) {
            ERROR(E_PDO);
        }

        /* push stack */
        ++msp;

        /* to stack entry, put q-reg text start */
        cptr.p = qreg[mm].f;

        /* initial char position, iteration flag */
        cptr.flag = cptr.c = cptr.dot = 0;

        /* number of chars in macro */
        cptr.z = qreg[mm].z;
        break;

    case '<':				/* begin iteration */
        /* if this is not to be executed */
        if ((esp->flag1) && (esp->val1 <= 0)) {
            /* just skip the intervening stuff */
            find_enditer();
        } else {
            /* does this macro have an iteration list? */
            if (!cptr.il) {
                /* no, make one for it */
                cptr.il = (struct is *) get_dcell();

                /* with NULL reverse pointer */
                cptr.il->b = NULL;

            /* is there an iteration in process? */
            } else if (cptr.flag & F_ITER) {
                /* yes, if it has no forward pointer */
                if (!cptr.il->f) {
                    /*
                     * append a cell to the
                     * iteration list
                     */
                    cptr.il->f = (struct is *) get_dcell();

                    /* and link it in */
                    cptr.il->f->b = cptr.il;
                }

                /*
                 * and advance the iteration list
                 * pointer to it
                 */
                cptr.il = cptr.il->f;
            }

            /*
             * Set iteration flag and save start of iteration
             */
            cptr.flag |= F_ITER;
            cptr.il->p = cptr.p;
            cptr.il->c = cptr.c;
            cptr.il->dot = cptr.dot;

            /* if there is an argument, set the "def iter" flag */
            if (cptr.il->dflag = esp->flag1) {
                /* save the count and consume arg */
                cptr.il->count = esp->val1;
                esp->flag1 = 0;
            }
        }
        break;

    case '>':				/* end iteration */
        /* error if > not in iteration */
        if (!(cptr.flag & F_ITER)) {
            ERROR(E_BNI);
        }

        /* decrement count and pop conditionally */
        pop_iteration(0);
        esp->flag1 = esp->flag2 = 0;	/* consume arguments */
        esp->op = OP_START;
        break;

    case ';':				/* semicolon iteration exit */
        /* error if ; not in iteration */
        if (!(cptr.flag &F_ITER)) {
            ERROR(E_SNI);
        }

        /* if exit */
        if ( (((esp->flag1) ? esp->val1 : srch_result) >= 0) ?
                (!colonflag) : (colonflag)) {

            /*
             * Get to end of iteration and pop
             * unconditionally
             */
            find_enditer();
            pop_iteration(1);
        }

        /* consume arg and colon */
        esp->flag1 = colonflag = 0;
        esp->op = OP_START;
        break;

    /* conditionals */

    case '"':
        /* must be an argument */
        if (!esp->flag1) {
            ERROR(E_NAQ);
        }
        esp->flag1 = 0;			/* consume argument */
        esp->op = OP_START;
        switch (mapch_l[getcmdc(trace_sw)]) {
        case 'a':
            cond = isalpha(esp->val1);
            break;

        case 'c':
            cond = isalpha(esp->val1) ||
                    (esp->val1 == '$') || (esp->val1 == '.');
            break;

        case 'd':
            cond = isdigit(esp->val1);
            break;

        case 'e':
        case 'f':
        case 'u':
        case '=':
            cond = !(esp->val1);
            break;

        case 'g':
        case '>':
            cond = (esp->val1 > 0);
            break;

        case 'l':
        case 's':
        case 't':
        case '<':
            cond = (esp->val1 < 0);
            break;

        case 'n':
            cond = esp->val1;
            break;

        case 'r':
            cond = isalnum(esp->val1);
            break;

        case 'v':
            cond = islower(esp->val1);
            break;

        case 'w':
            cond = isupper(esp->val1);
            break;

        default:
            ERROR(E_IQC);
        }

        /* if this conditional isn't satisfied */
        if (!cond) {
            /* read to matching | or ' */
            for (ll = 1; ll > 0;) {
                /* Skip chars */
                while ((skipto(0) != '"') &&
                        (skipc != '|') &&
                        (skipc != '\'')) {
                    ;
                }

                /* Start another level */
                if (skipc == '"') {
                    ++ll;

                /* End a level */
                } else if (skipc == '\'') {
                    --ll;

                /*  if on this level, start executing */
                } else if (ll == 1) {
                    break;
                }
            }
        }
        break;

    case '\'':				/* end of conditional */
        esp->flag1 = esp->flag2 = 0;
        break;				/* ignore it if executing */

    /* "else" clause */
    case '|':
        /* skip to matching ' */
        for (ll = 1; ll > 0;) {
            /* Skip chars */
            while ((skipto(0) != '"') &&
                    (skipc != '\'')) {
                ;
            }

            /* start another level */
            if (skipc == '"') {
                ++ll;

            /* End a level */
            } else {
                --ll;
            }
        }
        break;

    /* q-register numeric operations */

    case 'u':
        /* error if no arg */
        if (!esp->flag1) {
            ERROR(E_NAU);
        } else {
            qreg[getqspec(0, getcmdc(trace_sw))].v = esp->val1;
        }

        /* command's "value" is 2nd arg */
        esp->flag1 = esp->flag2;
        esp->val1 = esp->val2;
        esp->flag2 = 0;			/* clear 2nd arg */
        esp->op = OP_START;
        break;

    /* Qn is numeric val, :Qn is # of chars, mQn is mth char */
    case 'q':
        /* read register name */
        mm = getqspec((colonflag || esp->flag1), getcmdc(trace_sw));
        if (!(esp->flag1)) {
            esp->val1 = (colonflag) ? qreg[mm].z : qreg[mm].v;
            esp->flag1 = 1;
        } else {
            /* esp->flag1 is already set */

            /* char subscript within range? */
            if ((esp->val1 >= 0) && (esp->val1 < qreg[mm].z)) {
                for (ll = 0, aa.p = qreg[mm].f;
                        ll < (esp->val1 / CELLSIZE); ll++) {
                    aa.p = aa.p->f;
                }
                esp->val1 =
                 (int) aa.p->ch[esp->val1 % CELLSIZE];
            } else {
                    /* char position out of range */
                    esp->val1 = -1;
            }
            esp->op = OP_START;		/* consume argument */
        }
        colonflag = 0;
        break;

    case '%':
        /* add to q reg */
        esp->val1 = (qreg[getqspec(0, getcmdc(trace_sw))].v +=
            get_value(1));
        esp->flag1 = 1;
        break;

    /* move pointer */

    case 'c':
        if (((tdot = dot + get_value(1)) < 0) || (tdot > z)) {
            ERROR(E_POP);	/* add arg to dot, default 1 */
        } else {
            dot = tdot;
        }
        esp->flag2 = 0;
        break;

    case 'r':
        if (((tdot = dot - get_value(1)) < 0) || (tdot > z)) {
            ERROR(E_POP);	/* add arg to dot, default 1 */
        } else {
            dot = tdot;
        }
        esp->flag2 = 0;
        break;

    case 'j':
        if (((tdot = get_value(0)) < 0) || (tdot > z)) {
            ERROR(E_POP);	/* add arg to dot, default 1 */
        } else {
            dot = tdot;
        }
        esp->flag2 = 0;
        break;

    case 'l':
        dot += lines(get_value(1));
        break;

    /* number of chars until nth line feed */
    case CTL('Q'):
        esp->val1 = lines(get_value(1));
        esp->flag1 = 1;
        break;

    /* print numeric value */
    case '=':
        /* error if no arg */
        if (!esp->flag1) {
                ERROR(E_NAE);
        } else {
            int base;

            /* at least one more '=' */
            if (peekcmdc('=')) {
                /* read past it */
                getcmdc(trace_sw);

                /* another? */
                if (peekcmdc('=')) {
                    /* yes, read it too */
                    getcmdc(trace_sw);
                    base = 16;
                } else {
                    base = 8;
                }
            } else {
                base = 10;
            }

            if (esp->flag2) {
                prnum(esp->val2, base);
                printf(",");
            }
            prnum(esp->val1, base);
            if (!colonflag) {
                crlf();
            }
            esp->flag1 = esp->flag2 =  colonflag = 0;
            esp->op = OP_START;

            /*
             * If not in scroll mode, force full redraw
             * on next refresh
             */
            if (!WN_scroll) {
                window(WIN_REDRAW);
            }
        }
        break;

    /* insert text */
    case TAB:				/* insert tab, then text */
        if (ez_val & EZ_NOTABI) {
            break;		/* tab disabled */
        }
        if (esp->flag1) {
            ERROR(E_IIA);	/* can't have arg */
        }

    case 'i':				/* insert text at pointer */
        /* set terminator */
        term_char = (atflag) ? getcmdc(trace_sw) : ESC;

        /* if a nI$ command */
        if (esp->flag1) {
            if (!peekcmdc(term_char)) {
                    ERROR(E_IIA);	/* next char must be term */
            }
            insert1();		/* first part of insert */

            /* insert character */
            bb.p->ch[bb.c] = esp->val1 & 0177;

            /* advance pointer and extend buffer if necessary */
            fwdcx(&bb);
            ins_count = 1;		/* save string length */
            esp->op = OP_START;	/* consume argument */

        } else {
            /* not a nI command: insert text */

            /* initial insert operations */
            insert1();

            /* TAB insert puts in a tab first */
            if (command == TAB) {
                bb.p->ch[bb.c] = TAB;	/* insert a tab */

                /* advance pointer and maybe extend buffer */
                fwdcx(&bb);
            }

            /* copy cmd str -> buffer */
            moveuntil(&cptr, &bb, term_char, &ins_count,
                    cptr.z - cptr.dot, trace_sw);

            /* add 1 if a tab inserted */
            if (command == TAB) {
                ++ins_count;
            }

            /* indicate advance over that many chars */
            cptr.dot += ins_count;
        }
        insert2(ins_count);		/* finish insert */
        getcmdc(trace_sw);		/* flush terminating char */

        /* clear args */
        colonflag = atflag = esp->flag1 = esp->flag2 = 0;
        break;

    /* type text from text buffer */

    case 't':
        /* while there are chars to type */
        for (ll = line_args(0, &aa); ll > 0; ll--) {
            type_char(aa.p->ch[aa.c]);
            fwdc(&aa);
        }

        /* if not in scroll mode, force full redraw on next refresh */
        if (!WN_scroll) {
            window(WIN_REDRAW);
        }
        break;

    case 'v':
        /* arg must be positive */
        if ((ll = get_value(1)) > 0) {
            /*
             * Find start & number of chars
             */
            mm = lines(1 - ll);
            nn = lines(ll) - mm;

            /*
             * Point to start of text, display
             */
            set_pointer(dot + mm, &aa);
            for (; nn > 0; nn--) {
                type_char(aa.p->ch[aa.c]);
                fwdc(&aa);
            }
        }
        /* if not in scroll mode, force full redraw on next refresh */
        if (!WN_scroll) {
            window(WIN_REDRAW);
        }
        break;

    /* type text from command string */
    case CTL('A'):
        /* set terminator */
        term_char = (atflag) ? getcmdc(trace_sw) : CTL('A');

        /* output chars */
        while (getcmdc(0) != term_char) {
            type_char(cmdc);
        }
        atflag = colonflag = esp->flag2 = esp->flag1 = 0;
        esp->op = OP_START;

        /* if not in scroll mode, force full redraw on next refresh */
        if (!WN_scroll) {
            window(WIN_REDRAW);
        }
        break;

    /* delete text */
    case 'd':
        /* if only one argument */
        if (!esp->flag2) {
            /* delete chars (default is 1) */
            delete1(get_value(1));
            break;
        }

    /* VVV if two args, fall through to treat m,nD as m,nK VVV */

    case 'k':				/* delete lines or chars */
        /* aa points to start, ll chars, leave dot at beginning */
        ll = line_args(1, &aa);
        delete1(ll);			/* delete ll chars */
        break;

    /* q-register text loading commands */
    case CTL('U'):
        mm = getqspec(0, getcmdc(trace_sw));

        /* X, ^U commands destroy previous contents */
        if (!colonflag) {
            dly_free_blist(qreg[mm].f);
            qreg[mm].f = NULL;
            qreg[mm].z = 0;
        }

        /* set terminator */
        term_char = (atflag) ? getcmdc(trace_sw) : ESC;
        atflag = 0;			/* clear flag */

        /* if an arg or a nonzero insert, find register */
        if ((esp->flag1) || (!peekcmdc(term_char))) {
            /* attach a text buffer to the q register */
            make_buffer(&qreg[mm]);

            /* find end of reg */
            for (bb.p = qreg[mm].f; bb.p->f != NULL;
                            bb.p = bb.p->f) {
                ;
            }
            bb.c = (colonflag) ? (qreg[mm].z % CELLSIZE) : 0;
        }
        if (!(esp->flag1)) {
            moveuntil(&cptr, &bb, term_char, &ll,
                cptr.z - cptr.dot, trace_sw);
            /*
             * Advance over that many chars, update q-reg,
             * and skip terminator
             */
            cptr.dot += ll;
            qreg[mm].z += ll;
            getcmdc(trace_sw);
        } else {
            /* must be zero length string */
            if (getcmdc(trace_sw) != term_char) {
                ERROR(E_IIA);
            }

            /*
             * Store char, extend register
             */
            bb.p->ch[bb.c] = esp->val1;
            fwdcx(&bb);
            ++qreg[mm].z;
            esp->flag1 = 0;			/* consume argument */
        }
        colonflag = 0;
        break;

    case 'x':
        mm = getqspec(0, getcmdc(trace_sw));

        /* X, ^U commands destroy previous contents */
        if (!colonflag) {
            /* return, but delayed (in case executing now) */
            dly_free_blist(qreg[mm].f);
            qreg[mm].f = NULL;
            qreg[mm].z = 0;
        }

        /* read args and move chars, if any */
        if (ll = line_args(0, &aa)) {
            /* attach a text buffer to the q register */
            make_buffer(&qreg[mm]);

            /* find end of reg */
            for (bb.p = qreg[mm].f; bb.p->f != NULL;
                    bb.p = bb.p->f) {
                ;
            }
            bb.c = (colonflag) ? qreg[mm].z % CELLSIZE : 0;

            movenchars(&aa, &bb, ll);

            /* update q-reg char count */
            qreg[mm].z += ll;
        }
        colonflag = 0;
        break;

    case 'g':				/* get q register */
        /* if any chars in it */
        if (qreg[mm = getqspec(1, getcmdc(trace_sw))].z) {

            /* point cc to start of reg */
            cc.p = qreg[mm].f;
            cc.c = 0;

            /* :Gx types q-reg */
            if (colonflag) {
                for (ll = qreg[mm].z; ll > 0; ll--) {
                    type_char(cc.p->ch[cc.c]);
                    fwdc(&cc);
                }
            } else {
                /*
                 * Insert copy of Q-reg text
                 */
                insert1();
                movenchars(&cc, &bb, qreg[mm].z);
                insert2(qreg[mm].z);
            }
        }
        colonflag = 0;
        break;

    /* q-register push and pop */
    case '[':
        /* stack full */
        if (qsp > &qstack[QSTACKSIZE-1]) {
            ERROR(E_PDO);
        } else {
            /* increment stack ptr and put a text buffer there */
            make_buffer(++qsp);

            /* get the q reg name */
            mm = getqspec(1, getcmdc(trace_sw));

            /*
             * Copy Q-reg to new place
             */
            aa.p = qreg[mm].f;
            aa.c = 0;
            bb.p = qsp->f;
            bb.c = 0;
            movenchars(&aa, &bb, qreg[mm].z);
            qsp->z = qreg[mm].z;
            qsp->v = qreg[mm].v;
        }
        break;

    case ']':
        /* get reg name */
        mm = getqspec(1, getcmdc(trace_sw));

        /* if stack empty */
        if (qsp < &qstack[0]) {

            /* :] returns 0 */
            if (colonflag) {
                esp->flag1 = 1;
                esp->val1 = 0;
                colonflag = 0;
            } else {
                /* Otherwise error */
                ERROR(E_CPQ);
            }
        } else {
            /* stack not empty */

            /*
             * Return original contents of register, substitute
             * stack entry, null stack entry.
             */
            free_blist(qreg[mm].f);
            qreg[mm].f = qsp->f;
            qsp->f->b = (struct buffcell *) &qreg[mm];
            qsp->f = NULL;
            qreg[mm].z = qsp->z;
            qreg[mm].v = qsp->v;

            /* :] returns -1 */
            if (colonflag) {
                esp->flag1 = 1;
                esp->val1 = -1;
                colonflag = 0;
            }
            --qsp;
        }
        break;

    case '\\':
        /* no argument; read number */
        if (!(esp->flag1)) {
            /* sign flag and initial value */
            ll = esp->val1 = 0;

            /* count digits; don't read beyond buffer */
            for (ctrl_s = 0; dot < z; dot++, ctrl_s--) {
                set_pointer(dot, &aa);	/* point to dot */
                if ((aa.p->ch[aa.c] == '+') ||
                        (aa.p->ch[aa.c] == '-')) {

                    /*
                     * Quit on second sign, set sign on
                     * first.
                     */
                    if (ll) {
                        break;
                    } else {
                        ll = aa.p->ch[aa.c];
                    }
                } else {
                    /* octal or decimal */

                    /* stop if not a valid digit */
                    if (ctrl_r != 16) {
                        if (!isdigit(aa.p->ch[aa.c])) {
                                break;
                        }
                        if (aa.p->ch[aa.c] - '0' >=
                                ctrl_r) {
                            break;
                        }
                        esp->val1 = esp->val1*ctrl_r +
                                (aa.p->ch[aa.c] - '0');
                    } else {
                        if (!isxdigit(aa.p->ch[aa.c])) {
                            break;
                        }
/*
 * This just doesn't fit out there on the left-hand margin
 */
esp->val1 = esp->val1 * 16 + ( (isdigit(aa.p->ch[aa.c])) ?
aa.p->ch[aa.c] - '0' : mapch_l[aa.p->ch[aa.c]] - 'a' + 10);
                    }		/* end of hex */
                }		/* end of digit processing */
            }		/* end of "for each char" */

            /* if minus sign */
            if (ll == '-') {
                esp->val1 = -(esp->val1);
            }
            esp->flag1 = 1;		/* always returns a value */
        } else {
            /* argument: insert it as a digit string */

            /* print as digits */
            if (ctrl_r == 8) {
                sprintf(t_bcell.ch, "%o", esp->val1);
            } else if (ctrl_r == 10) {
                sprintf(t_bcell.ch, "%d", esp->val1);
            } else {
                sprintf(t_bcell.ch, "%x", esp->val1);
            }
            insert1();			/* start insert */
            cc.p = &t_bcell;	/* point cc to the temp cell */
            cc.c = 0;

            /* copy the char string */
            moveuntil(&cc, &bb, '\0', &ins_count, CELLSIZE-1, 0);
            insert2(ins_count);	/* finish the insert */
            esp->flag1 = 0;		/* consume argument */
            esp->op = OP_START;
        }
        break;

    case CTL('T'):			/* type or input character */
        /* type */
        if (esp->flag1) {
            type_char(esp->val1);
            esp->flag1 = 0;
            if (!WN_scroll) {
                /*
                 * If not in scroll mode, force
                 * full redraw on next refresh
                 */
                window(WIN_REDRAW);
            }
        } else {
            esp->val1 = (et_val & ET_NOWAIT) ?
                    gettty_nowait() : gettty();

            if (!(et_val & ET_NOECHO) && (esp->val1 > 0) &&
                    !inp_noterm) {
                type_char(esp->val1);		/* echo */
            }
            esp->flag1 = 1;
        }
        break;

    /* search commands */

    case 's':			/* search within buffer */
        build_string(&sbuf);	/* read the search string */
        end_search(do_search(setup_search()));		/* search */
        break;

    case 'n':			/* search through rest of file */
    case '_':
        do_nsearch(command);	/* call routine for N, _, E_ */
        break;

    case 'o':					/* branch to tag */
        do_o();
        break;

    /* file I/O commands */

    /* write a page, get next (ignore args for now) */
    case 'p':
        /* if two args */
        if (esp->flag1 && esp->flag2) {
                /* write spec'd buffer with no FF */
                write_file(&aa, line_args(0, &aa), 0);
        } else {
            /* one arg */

            /* get count and loop */
            for (ll = get_value(1); ll > 0; ll--) {
                set_pointer(0, &aa);

                /* PW writes buffer, then FF */
                if (peekcmdc('w')) {
                        write_file(&aa, z, 1);
                } else {
                    /*
                     * P writes buffer, FF if read in,
                     * then gets next page
                     */
                    write_file(&aa, z, ctrl_e);

                    /* empty the buffer */
                    dot = z = 0;

                    /*
                     * set a pointer to the beginning of
                     * the buffer
                     */
                    set_pointer(0, &aa);

                    /* mark where new buffer starts */
                    buff_mod = 0;

                    /* read a page */
                    esp->val1 = read_file(&aa, &z,
                            (ed_val & ED_EXPMEM ? -1 : 0) );
                    esp->flag1 = colonflag;
                }
            }
        }

        /* if a PW command, consume the W */
        if (peekcmdc('w')) {
            getcmdc(trace_sw);
        }
        colonflag = 0;
        break;

    case 'y':				/* get a page into buffer */
        if (esp->flag1) {
            ERROR(E_NYA);
        }
        if ((z) && (!(ed_val & ED_YPROT))) {
            ERROR(E_YCA);	/* don't lose text */
        }
        dot = z = 0;		/* clear buffer */

        /* set a pointer to the beginning of the buffer */
        set_pointer(0, &aa);

        /* mark where new buffer starts */
        buff_mod = 0;

        /* read a page */
        read_file(&aa, &z, (ed_val & ED_EXPMEM ? -1 : 0) );
        esp->flag1 = colonflag;
        esp->op = OP_START;
        colonflag = 0;
        break;

    case 'a':				/* append, or ascii value */
        /* ascii value */
        if (esp->flag1 && !colonflag) {
            /* set a pointer before addr'd char */
            ll = dot + esp->val1;

            /* if character lies within buffer */
            if ((ll >= 0) && (ll < z)) {
                set_pointer(ll, &aa);

                /* get char (flag already set) */
                esp->val1 = (int) aa.p->ch[aa.c];
            } else {
                /* otherwise return -1 */
                esp->val1 = -1;
            }
        } else {
            /*
             * Set pointer to end of buffer, mark where new
             * buffer starts.
             */
            set_pointer(z, &aa);
            if (z < buff_mod) {
                buff_mod = z;
            }

            /* neg or 0 arg to :A */
            if (esp->flag1 && (esp->val1 <= 0)) {
                ERROR(E_IAA);
            }

            /* read a page */
            read_file(&aa, &z, (esp->flag1 ? esp->val1 : 0) );
            esp->flag1 = colonflag;
            colonflag = 0;
        }
        esp->op = OP_START;
        break;

    /* window commands */

    case 'w':
        do_window(0);	/* this stuff is with the window driver */
        break;

    case CTL('W'):
        do_window(1);	/* this is, too */
        break;

    default:
        ERROR(E_ILL);	/* invalid command */

    }		/* end of "switch" */
}			/* end of exec_cmds1 */
