/*
 * TECO for Ultrix   Copyright 1986 Matt Fichtenbaum
 * This program and its components belong to GenRad Inc, Concord MA 01742
 * They may be copied if this copyright notice is included
 */

/* subs.c subroutines  11/8/85 */
#include <ctype.h>
#include "defs.h"

/*
 * routines to copy a string of characters
 * movenchars(from, to, n)
 *  	from, to are the addresses of qps
 *    n is the number of characters to move
 * moveuntil(from, to, c, &n, max)
 *	  c is the match character that ends the move
 *    n is the returned number of chars moved
 * max is the maximum number of chars to move
 */
void
movenchars(struct qp *from, struct qp *to, int n)
{
    struct buffcell *fp, *tp;	/* local qp ".p" pointers */
    int fc, tc;			/* local qp ".c" subscripts */

    /* No-op */
    if (n == 0) {
        return;
    }

    /* copy pointers to local registers */
    fp = from->p;
    fc = from->c;
    tp = to->p;
    tc = to->c;

    for (; n > 0; n--) {
        /* move one char */
        tp->ch[tc++] = fp->ch[fc++];

        /* check current cell done */
        if (tc > CELLSIZE-1) {
            /* is there another following? */
            if (!tp->f) {
                /* no, add one */
                tp->f = get_bcell();
                tp->f->b = tp;
            }
            tp = tp->f;
            tc = 0;
        }

        /* check current cell done */
        if (fc > CELLSIZE-1) {

            /* oops, run out of source */
            if (!fp->f) {
                /* error if not done */
                if (n > 1) {
                    ERROR(E_UTC);
                }
            } else {
                /* chain to next cell */
                fp = fp->f;
                fc = 0;
            }
        }
    }

    /* restore arguments */
    from->p = fp;
    to->p = tp;
    from->c = fc;
    to->c = tc;
}

/*
 * moveuntil()
 *      (Comment above)
 *      
 *  struct qp *from, *to;	address of buffer pointers
 *  char c;             	match char that ends move
 *  int *n;			pointer to returned value
 *  int max;    		limit on chars to move
 *  int trace;  		echo characters if nonzero
 */
void
moveuntil(struct qp *from, struct qp *to,
    char c, int *n, int max, int trace)
{
    struct buffcell *fp, *tp;	/* local qpr ".p" pointers */
    int fc, tc;			/* local qpr ".c" subscripts */

    /* copy pointers to local registers */
    fp = from->p;
    fc = from->c;
    tp = to->p;
    tc = to->c;

    /* until terminating char... */
    for (*n = 0; fp->ch[fc] != c; (*n)++) {
        if (max-- <= 0) {
            ERROR((msp <= &mstack[0]) ? E_UTC : E_UTM);
        }

        /* move one char */
        tp->ch[tc++] = fp->ch[fc++];

        /* type it out if trace mode */
        if (trace) {
            type_char(tp->ch[tc-1]);
        }

        /* check current cell done */
        if (tc > CELLSIZE-1) {
            /* is there another following? */
            if (!tp->f) {
                tp->f = get_bcell();	/* no, add one */
                tp->f->b = tp;
            }
            tp = tp->f;
            tc = 0;
        }

        /* check current cell done */
        if (fc > CELLSIZE-1) {
             /* oops, run out of source */
            if (!fp->f) {
                ERROR(E_UTC);
            } else {
                fp = fp->f;	/* chain to next cell */
                fc = 0;
            }
        }
    }

    /* restore arguments */
    from->p = fp;
    to->p = tp;
    from->c = fc;
    to->c = tc;
}

/* routine to get numeric argument */
/* get a value, default is argument */
int
get_value(int d)
{
    int v;

    v = (esp->flag1) ? esp->val1 : 
    (esp->op == OP_SUB) ? -d : d;
    esp->flag1 = 0;		/* consume argument */
    esp->op = OP_START;
    return(v);
}

/* routine to convert a line count */
/* returns number of chars between dot and nth line feed */
int
lines(int arg)
{
    int i, c;
    struct buffcell *p;

    /* find dot */
    for (i = dot / CELLSIZE, p = buff.f; (i > 0) && (p->f); i--) {
        p = p->f;
    }
    c = dot % CELLSIZE;

    /* scan backwards */
    if (arg <= 0) {
        /* repeat for each line */
        for (i = dot; (arg < 1) && (i > 0); ) {
            /* count characters */
            --i;

            /* back up the pointer */
            if (--c < 0) {
                if (!(p = p->b)) {
                    break;
                }
                c = CELLSIZE - 1;
            }

            /* if line sep found */
            if ( (ez_val & EZ_NOVTFF) ?
                    (p->ch[c] == LF) :
                    (spec_chars[p->ch[c & 0xFF] & 0xFF] & A_L) ) {
                ++arg;
            }
        }

        /*
         * If terminated on a line separator,
         * advance over the separator
         */
        if (arg > 0) {
            ++i;
        }

    } else {

        /* scan forwards */
        for (i = dot; (arg > 0) && (i < z); i++) {
            if ( (ez_val & EZ_NOVTFF) ?
                    (p->ch[c] == LF) :
                    (spec_chars[p->ch[c & 0xFF] & 0xFF] & A_L) ) {
                --arg;
            }

            if (++c > CELLSIZE-1) {
                if (!(p = p->f)) {
                    break;
                }
                c = 0;
            }
        }	/* this will incr over the separator anyway */
    }
    return(i - dot);
}

/*
 * routine to handle args for K, T, X, etc.
 * if two args, 'char x' to 'char y'
 * if just one arg, then n lines (default 1)
 * sets a pointer to the beginning of the specd
 * string, and a char count value
 *
 * When d is nonzero: leave dot at start
 */
int
line_args(int d, struct qp *p)
{
    int n;

    /* if two args */
    if (esp->flag1 && esp->flag2) {

        /* in right order */
        if (esp->val1 <= esp->val2) {
            if (esp->val1 < 0) {
                esp->val1 = 0;
            }
            if (esp->val2 > z) {
                esp->val2 = z;
            }

            /* update dot */
            if (d) {
                dot = esp->val1;
            }

            /*
             * Set pointer, consume arguments, return count
             */
            set_pointer(esp->val1, p);
            esp->flag2 = esp->flag1 = 0;
            esp->op = OP_START;
            return(esp->val2 - esp->val1);

        }

        if (esp->val2 < 0) {
            esp->val2 = 0;
        }
        if (esp->val1 > z) {
            esp->val1 = z;
        }

        /* update dot */
        if (d) {
            dot = esp->val2;
        }


        /*
         * Use args in reverse order, consume them,
         * return
         */
        set_pointer(esp->val2, p);
        esp->flag2 = esp->flag1 = 0;
        esp->op = OP_START;
        return(esp->val1 - esp->val2);

    }

    n = lines(get_value(1));
    if (n < -dot) {
        n = -dot;
    } else if (n > z-dot) {
        n = z-dot;
    }
    if (n >= 0) {
        set_pointer(dot, p);
    } else {
        n = -n;
        set_pointer(dot - n, p);
        if (d) {
            dot -= n;
        }
    }
    return(n);
}

/* convert character c to a q-register spec */
/* fors ("file or search") nonzero = allow _ or * */
int
getqspec(int fors, char c)
{
    if (isdigit(c)) {
        return(c - '0' + 1);
    }
    if (isalpha(c)) {
        return(mapch_l[c & 0xFF] - 'a' + 11);
    }

    /*
     * q# is special because we need to be able to load macro commands
     * from an external command, and then execute them.
     */
    if (c == '#') {
        return(TIMBUF);
    }

    if (fors) {
        if (c == '_') {
            return (SERBUF);
        }
        if (c == '*') {
            return (FILBUF);
        }
        if (c == '%') {
            return (SYSBUF);
        }
    }
    ERROR(E_IQN);
    /*NOTREACHED*/
    return(0);
}

/*
 * routines to do insert operations
 * insert1() copies current cell up to dot into a new cell
 * leaves bb pointing to end of that text
 * insert2() copies rest of buffer
 */
extern struct buffcell *insert_p;
void
insert1(void)
{
    int nchars;			/* number of chars in cell */

    /* convert dot to a qp */
    set_pointer(dot, &aa);

    /* update earliest char loc touched */
    if (dot < buff_mod) {
        buff_mod = dot;
    }

    /* get a new cell */
    insert_p = bb.p = get_bcell();
    bb.c = 0;
    nchars = aa.c;		/* save char position of dot in cell */
    aa.c = 0;

    /*
     * now aa points to the beginning of the buffer cell that
     * contains dot, bb points to the beginning of a new cell,
     * nchars is the number of chars before dot
     */
    movenchars(&aa, &bb, nchars);	/* copy cell up to dot */
}

/* count is the number of chars added */
void
insert2(int count, int undoable)
{
    /* Record the change */
    if (undoable) {
	undo_insert(dot, count);
    }

    /* put the new cell where the old one was */
    aa.p->b->f = insert_p;
    insert_p->b = aa.p->b;
    insert_p = NULL;

    /* splice rest of buffer to end */
    bb.p->f = aa.p;
    aa.p->b = bb.p;

    /* squeeze buffer */
    movenchars(&aa, &bb, z-dot);

    /* return unused cells */
    free_blist(bb.p->f);

    /* and end the buffer */
    bb.p->f = NULL;

    /* add # of chars inserted */
    z += count;
    dot += count;

    /* save string length */
    ctrl_s = -count;
}

/*
 * subroutine to delete n characters starting at dot
 * argument is number of characters
 */
void
delete1(int nchars)
{
    /* 0 chars is a nop */
    if (!nchars) {
        return;
    }

    /* delete negative number of characters? */
    if (nchars < 0) {
        /* make ll positive */
        nchars = -nchars;

        /* don't delete beyond beg of buffer */
        if (nchars > dot) {
            ERROR(E_POP);
        }

        /* put pointer before deleted text */
        dot -= nchars;

    } else if (dot + nchars > z) {
        /* don't delete beyond end of buffer */
        ERROR(E_POP);
    }

    /* record for undo */
    undo_del(dot, nchars);

    /* pointer to beginning of area to delete */
    set_pointer(dot, &aa);

    /* and to end */
    set_pointer(dot+nchars, &bb);

    /* update earliest char loc touched */
    if (dot < buff_mod) {
        buff_mod = dot;
    }

    /* move text unless delete ends at z */
    movenchars(&bb, &aa, z-(dot+nchars));

    /* return any cells after end */
    free_blist(aa.p->f);

    /* end the buffer */
    aa.p->f = NULL;

    /* adjust z */
    z -= nchars;
}

/*
 * routine to process "O" command
 */
struct qh obuff;		/* tag string buffer */
void
do_o(void)
{
    int i, j;		/* i used as start of tag, j as end */
    int p,			/* pointer to tag string */
            level;		/* iteration level */
    int epfound;		/* flag for "second ! found" */

    /* no tag spec'd: continue */
    if (!build_string(&obuff)) {
        return;
    }

    /* string too long */
    if (obuff.z > CELLSIZE) {
        ERROR(E_STL);
    }
    esp->op = OP_START;	/* consume any argument */
    if (esp->flag1) {	/* is there one? */
        esp->flag1 = 0;		/* consume it */
        if (esp->val1 < 0) {
            return;		/* computed goto out of range - */
        }

        /* scan to find right tag */
        for (i = 0; (i < obuff.z) && (esp->val1 > 0); i++) {
            /* count commas */
            if (obuff.f->ch[i] == ',') {
                esp->val1--;
            }
        }

        /* computed goto out of range + */
        if (esp->val1 > 0) {
            return;
        }

        /* now i is either at 0 or after the nth comma */

        for (j = i; j < obuff.z; j++) {	/* find end of tag */
            /* stop at next comma */
            if (obuff.f->ch[j] == ',') {
                break;
            }
        }

        /* two adjacent commas: zero length tag */
        if (j == i) {
            return;
        }

    } else {
        /* not a computed goto: use whole tag buffer */
        i = 0;
        j = obuff.z;
    }

    /* start from beginning of iteration or macro, and look for tag */

    if (cptr.flag & F_ITER) {			/* if in iteration */
            cptr.p = cptr.il->p;		/* restore */
            cptr.c = cptr.il->c;
            cptr.dot = cptr.il->dot;

    } else {
        /* find macro start */
        for (cptr.dot = cptr.c = 0; cptr.p->b->b != NULL;
                cptr.p = cptr.p->b) {
            ;
        }
    }

    /* search for tag */

    /* look through rest of command string */
    for (level = 0; ;) {

        /* look for interesting things, including ! */
        switch (skipto(1)) {
        case '<':			/* start of iteration */
            ++level;
            break;

        case '>':			/* end of iteration */
            if ((level == 0) && (cptr.flag & F_ITER)) {
                pop_iteration(1);
            } else {
                --level;
            }
            break;

        case '!':			/* start of tag */

            /* keep looking for tag */
            for (;;) {
                    epfound = 0;
                    for (p = i; p < j; p++) {
                        /* mark trailing ! found */
                        if (getcmdc(0) == '!') {
                            epfound = 1;
                            break;
                        }

                        /* compare */
                        if (mapch_l[cmdc & 0xFF] !=
                                mapch_l[obuff.f->ch[p] & 0xFF]) {
                            break;
                        }
                    }

                    /* If all comparison chars matched, done */
                    if ((p == j) && peekcmdc('!')) {
                        getcmdc(0);	/* Junk last '!' */
                        return;
                    } else if (!epfound) {
                        /* Get rest of non-matching tag, toss */
                        while (getcmdc(0) != '!') {
                            continue;
                        }
                    }
                    break;
            }
            break;
        }		/* end of switch */
    }		/* end of scan loop */
}		/* end of subroutine */

/*
 * routine to skip to next ", ', |, <, or >
 * skips over these chars embedded in text strings
 * stops in ! if argument is nonzero
 * returns character found, and leaves it in skipc
 */
char
skipto(int arg)
{
    int atsw;		/* "at" prefix */
    char ta, term;		/* temp attributes, terminator */

    for (atsw = 0; ;) {
        /* read until something interesting found */
        for (;;) {
            skipc = getcmdc(0);
            ta = spec_chars[skipc & 0xFF];
            ta &= (A_X | A_S | A_T | A_Q);
            if (ta) {
                break;
            }
        }
again:
        /* if command takes a Q spec, skip the spec */
        if (ta & A_Q) {
            getcmdc(0);
        }

        /* sought char found: quit */
        if (ta & A_X) {
            /* quote must skip next char */
            if (skipc == '"') {
                    getcmdc(0);
            }
            return(skipc);
        }

        /* other special char */
        if (ta & A_S) {
            skipc = mapch_l[skipc & 0xFF];
            switch (skipc) {
            case '^': /* treat next char as CTL */
                skipc = getcmdc(0);
                skipc &= 0x1f;
                ta = spec_chars[skipc & 0xFF];
                if (ta) {
                    goto again;
                }
                break;

            case '@': /* use alternative text terminator */
                atsw = 1;
                break;

            /* ^^ is value of next char: skip that char */
            case CTL('^'):
                getcmdc(0);
                break;

            case CTL('A'): /* type text */
                term = (atsw) ? getcmdc(0) : CTL('A');
                atsw = 0;

                /* skip text */
                while (getcmdc(0) != term) {
                    ;
                }
                break;

            case '!': /* tag */
                if (arg) {
                    return(skipc);
                }

                /* skip until next ! */
                while (getcmdc(0) != '!') {
                    ;
                }
                break;

            case 'e': /* first char of two-letter E or F command */
            case 'f':
                /* if one with a text arg */
                if (spec_chars[getcmdc(0) & 0xFF] &
                        ((skipc == 'e') ? A_E : A_F)) {
                    term = (atsw) ? getcmdc(0) : ESC;
                    atsw = 0;

                    /* read past terminator */
                    while (getcmdc(0) != term) {
                        ;
                    }
                }
                break;
            } /* end "switch" */

    /* command with a text argument */
        } else if (ta & A_T) {
            term = (atsw) ? getcmdc(0) : ESC;
            atsw = 0;

            /* skip text */
            while (getcmdc(0) != term) {
                ;
            }
        }
    } /* end "forever" */
} /* end "skipto()" */

/* find number of characters to next matching (, [, or {  (like '%' in vi) */
void
do_ctlp(void)
{
    int i, l;
    char c, c1;

    set_pointer(dot, &aa);			/* point to text buffer */
    switch(c1 = aa.p->ch[aa.c])
    {
    case '(':
        c = ')';			/* match char is ) */
        i = 1;				/* direction is positive */
        break;

    case ')':
        c = '(';			/* match char is ( */
        i = -1;				/* direction is negative */
        break;

    case '[':
        c = ']';
        i = 1;
        break;

    case ']':
        c = '[';
        i = -1;
        break;

    case '{':
        c = '}';
        i = 1;
        break;

    case '}':
        c = '{';
        i = -1;
        break;

    case '<':
        c = '>';
        i = 1;
        break;

    case '>':
        c = '<';
        i = -1;
        break;

    case '"':
        c = '\'';
        i = 1;
        break;

    case '\'':
        c = '"';
        i = -1;
        break;

    default:
            /* not on a matchable char, return 0 */
            esp->val1 = i = 0;
    }

    l = 1;			/* start with one unmatched char */

    /* if searching forward */
    if (i > 0) {
        for (i = dot, fwdc(&aa); (i < z) && (l); fwdc(&aa) ) {
            ++i;
            if (aa.p->ch[aa.c] == c) {
                --l;
            } else if (aa.p->ch[aa.c] == c1) {
                ++l;
            }
        }
        esp->val1 = (i < z) ? i - dot : 0;

    } else if (i < 0) {
        for (i = dot, backc(&aa); (i >= 0) && (l); backc(&aa) ) {
            --i;
            if (aa.p->ch[aa.c] == c) {
                --l;
            } else if (aa.p->ch[aa.c] == c1) {
                ++l;
            }
        }
        esp->val1 = (i >= 0) ? i - dot : 0;
    }
    esp->flag1 = 1;
}
