/*
 * TECO for Ultrix   Copyright 1986 Matt Fichtenbaum
 * This program and its components belong to GenRad Inc, Concord MA 01742
 * They may be copied if this copyright notice is included
 */

/* te_srch.c   routines associated with search operations   2/5/86 */

#include "te_defs.h"

/*
 * routine to read in a string with string-build characters
 * used for search, tag, file name operations
 * returns 0 if empty string entered, nonzero otherwise
 */
build_string(lbuff)
	struct qh *lbuff;		/* arg is addr of q-reg header */
{
	int count;			/* char count */
	struct buffcell *tp;		/* pointer to temporary string */
	char c;				/* temp character */

	/* read terminator */
	term_char = (atflag) ? getcmdc(trace_sw) : ESC;
	count = atflag = 0;			/* initialize char count */

	/* if string is not empty */
	if (!peekcmdc(term_char)) {

		/*
		 * Create a temporary string and read chars into
		 * it until the terminator
		 */
		for (tp = bb.p = get_bcell(), bb.c = 0;
			(c = getcmdc(trace_sw)) != term_char; )
		{
			/* read next char as CTL */
			if ((c == '^') && !(ed_val & ED_CARET)) {
				if ((c = getcmdc(trace_sw)) == term_char)
					ERROR(msp <= &mstack[0] ?
						E_UTC : E_UTM);
				c &= 0x1f;
			}

			/* if a control char */
			if ((c &= 0177) < ' ') {
				switch (c) {
				/* take next char literally */
				case CTL (Q):
				case CTL (R):
					if ((c = getcmdc(trace_sw)) ==
							term_char)
						ERROR((msp <= &mstack[0]) ?
							E_UTC : E_UTM);

					/* fetch character and go store */
					break;

				/* take next char as lower case */
				case CTL (V):
					if (getcmdc(trace_sw) == term_char)
						ERROR((msp <= &mstack[0]) ?
							E_UTC : E_UTM);
					c = mapch_l[cmdc];
					break;

				/* take next char as upper case */
				case CTL (W):
					if ((c = getcmdc(trace_sw)) ==
							term_char)
						ERROR((msp <= &mstack[0]) ?
							E_UTC : E_UTM);
					if (islower(c)) c = toupper(c);
					break;

				/* expanded constructs */
				case CTL (E):
					if (getcmdc(trace_sw) == term_char)
						ERROR((msp <= &mstack[0]) ?
							E_UTC : E_UTM);
					switch (mapch_l[cmdc]) {

					/* use char in q-reg */
					case 'u':
						if (getcmdc(trace_sw) ==
							term_char) ERROR((msp
								<= &mstack[0])
								? E_UTC :
								E_UTM);
						c = qreg[getqspec(1,
							cmdc)].v & 0x7f;
						break;
					/* use string in q-reg */
					case 'q':
						if (getcmdc(trace_sw) ==
							term_char) ERROR((msp
							<= &mstack[0]) ?
							 E_UTC : E_UTM);

						/* read the reg spec */
						ll = getqspec(1, cmdc);

						/* set a pointer to it */
						aa.p = qreg[ll].f;
						aa.c = 0;
						for (mm = 0; mm < qreg[ll].z;
								mm++) {

							/* store char */
							bb.p->ch[bb.c] =
								aa.p->ch[aa.c];

							/* store next char */
							fwdcx(&bb);
							fwdc(&aa);
							++count;
						}

						/*
						 * Repeat loop without
						 * storing
						 */
						continue;

					default:

						/* not special: store the ^E */
						bb.p->ch[bb.c] = CTL (E);
						fwdcx(&bb);
						++count;

						/*
						 * And go store the following
						 * char
						 */
						c = cmdc;
						break;

					}
				}
			}

			/* store character */
			bb.p->ch[bb.c] = c;

			/* advance pointer */
			fwdcx(&bb);

			/* count characters */
			++count;
		}
		free_blist(lbuff->f);		/* return old buffer */
		lbuff->f = tp;			/* put in new one */
		lbuff->f->b = (struct buffcell *) lbuff;

		/* store count of chars in string */
		lbuff->z = count;
	} else
		/* empty string: consume terminator */
		getcmdc(trace_sw);

	/* return char count */
	return(count);
}			

/*
 * routine to handle end of a search operation
 * called with pass/fail result from search
 * returns same pass/fail result
 */
end_search(result)
	int result;
{
	/* if search failed */
	if (!result) {

		/* if an unbounded search failed, clear ptr */
		if (!(esp->flag2 || (ed_val & ED_SFAIL)))
			dot = 0;

		/* if no real or implied colon, error if failure */
		if (!colonflag && !peekcmdc(';'))
			ERROR(E_SRH);
	}

	/* return a value if a :S command */
	esp->flag1 = colonflag;
	srch_result = esp->val1 = result;	/* and leave it for next ";" */
	esp->flag2 = colonflag = atflag = 0;	/* consume arguments */
	esp->op = OP_START;
	return(result);
}

/*
 * routine to set up for search operation
 * reads search arguments, returns search count
 */
static struct qp sm, sb;	/* match-string and buffer pointers */
static char *pmap;		/* pointer to character mapping table */
static int locb;		/* reverse search limit */
static int last_z;		/* end point for reverse search */

setup_search()
{
	int count;				/* string occurrence counter */

	/* set a pointer to start of search */
	set_pointer(dot, &aa);

	/* ::S is 1,1S */
	if (colonflag >= 2)
		esp->flag2 = esp->flag1 = esp->val2 = esp->val1 = 1;

	/* read search count: default is 1 */
	if ((count = get_value(1)) == 0)
		ERROR(E_ISA);

	/* search forward */
	else if (count > 0) {

		/* if bounded search */
		if (esp->flag2) {

			/* set limit */
			if (esp->val2 < 0)
				esp->val2 = -(esp->val2);

			/* or z, whichever less */
			if ((aa.z = dot + esp->val2) > z)
				aa.z = z;
		} else
			aa.z = z;
	} else {

		/* if bounded search */
		if (esp->flag2) {

			/* set limit */
			if (esp->val2 < 0)
				esp->val2 = -(esp->val2);

			/* or 0, whichever greater */
			if ((locb = dot - esp->val2) < 0)
				locb = 0;
		} else
			locb = 0;
	}
	return(count);
}

/*
 * routine to do N, _, E_ searches:  search, if search fails, then get
 * next page and continue
 */
do_nsearch(arg)
	char arg;	/* arg is 'n', '_', or 'e' to define which search */
{
	int scount;		/* search count */

	/* read the search string */
	build_string(&sbuf);

	/* count must be >0 */
	if ((scount = get_value(1)) <= 0)
		ERROR(E_ISA);

	/* start search at dot */
	set_pointer(dot, &aa);

	/* make it unbounded */
	esp->flag2 = locb = 0;

	/* search until found */
	while (scount > 0) {

		/* search forwards */
		if (!do_search(1)) {
			/*   if search fails... */

			/* if no input, quit */
			if (infile->eofsw || !infile->fd)
				break;
			if (arg == 'n') {
				set_pointer(0, &aa);	/* write file if 'n' */
				write_file(&aa, z, ctrl_e);
			}

			/*
			 * Not 'n': if _, and an output file,
			 * and data to lose, error
			 */
			else if ((arg == '_') && (outfile->fd) && (z) &&
					(ed_val & ED_YPROT))
				ERROR(E_YCA);

			/* clear buffer */
			buff_mod = dot = z = 0;
			set_pointer(0, &aa);

			/* read next page */
			read_file(&aa, &z, (ed_val & ED_EXPMEM ? -1 : 0) );

			/* search next page from beginning */
			set_pointer(0, &aa);
		} else
			/* search successful: one fewer to look for */
			--scount;
	}

	/* use end_search to clean up */
	return( end_search( (scount == 0) ? -1 : 0) );
}

/*
 * routine to do "FB" search - m,nFB is search from m to n,
 * nFB is search from . to nth line
 * convert arguments to args of normal m,nS command
 */
do_fb()				/* returns search result */
{
	/* if two arguments */
	if (esp->flag1 && esp->flag2) {

		/* start from "m" arg */
		dot = esp->val2;

		/* get number of chars */
		esp->val2 = esp->val1 - esp->val2;
	} else {
		/* if no or one args, treat as number of lines */

		/* number of chars */
		esp->val2 = lines(get_value(1));

		/* conjure up two args */
		esp->flag2 = esp->flag1 = 1;
	}
	esp->val1 = (esp->val2 > 0) ? 1 : -1;	/* set search direction */

	/* read search string and terminator */
	build_string(&sbuf);

	/* do search and return result */
	return(end_search(  do_search( setup_search() )  ));
}

/*
 * routine to do search operation: called with search count as argument
 * returns -1 (pass) or 0 (fail)
 */
do_search(count)
	int count;
{
	/* set approp. mapping table */
	pmap = (ctrl_x) ? &mapch[0] : &mapch_l[0];

	/* copy # of chars in search buffer */
	sm.z = sbuf.z;

	if (count > 0) {

		/* loop to count occurrences */
		for (sm.dot = 0; count > 0; count--) {

			 /* loop to advance search pointer */
			for (; aa.dot < aa.z; aa.dot++) {
				sb.p = aa.p; sb.c = aa.c; sb.dot = aa.dot;
				sm.p = sbuf.f; sm.dot = sm.c = 0;

				/* for each char in search string */
				for ( ; (sb.dot < z) && (sm.dot < sm.z);
						sm.dot++, sb.dot++) {

					/* if search string char is "special" */
					if (spec_chars[sm.p->ch[sm.c]] & A_A) {

						/*
						 * Then use expanded comparison
						 * routine
						 */
						if (!srch_cmp())
							break;
					} else if (*(pmap + sb.p->ch[sb.c]) !=
						   *(pmap + sm.p->ch[sm.c]))
						break;

					/* else just compare */

					/* advance search-string ptr */
					if (++sm.c > CELLSIZE-1) {
						sm.p = sm.p->f;
						sm.c = 0;
					}

					/* advance buffer ptr */
					if (++sb.c > CELLSIZE-1) {
						sb.p = sb.p->f;
						sb.c = 0;
					}
				}

				/* exit if found */
				if (sm.dot >= sm.z)
					break;

				/* else not found: advance buffer pointer */
				if (++aa.c > CELLSIZE-1) {
					aa.p = aa.p->f;
					aa.c = 0;
				}
			}

			/* if one search failed, don't do more */
			if (sm.dot < sm.z)
				break;
			else {

				/* otherwise save -length of string found */
				ctrl_s = aa.dot - sb.dot;

				/* if funny "advance by 1" mode */
				if ((ed_val & ED_SMULT) && (count > 1)) {

					/* advance buffer pointer by one only */
					++aa.dot;
					if (++aa.c > CELLSIZE-1) {
						aa.p = aa.p->f;
						aa.c = 0;
					}
				} else {

					/* advance search pointer past string */
					aa.dot = sb.dot;
					aa.p = sb.p;
					aa.c = sb.c;
				}
			}
		}
	} else {
		/* search backwards */

		/* loop to count occurrences */
		for (last_z = z, sm.dot = 0; count < 0; count++) {

			/* loop to advance (backwards) search pointer */
			for (; aa.dot >= locb; aa.dot--) {
				sb.p = aa.p; sb.c = aa.c; sb.dot = aa.dot;
				sm.p = sbuf.f; sm.dot = sm.c = 0;

				/* loop to compare string */
				for ( ; (sb.dot < last_z) && (sm.dot < sm.z);
						sm.dot++, sb.dot++) {

					/* if search string char is "special" */
					if (spec_chars[sm.p->ch[sm.c]] & A_A) {

						/*
						 * Then use expanded comparison
						 * routine
						 */
						if (!srch_cmp())
							break;
					} else if (*(pmap + sb.p->ch[sb.c])
						!= *(pmap + sm.p->ch[sm.c]))

						/* else just compare */
						break;

					/* advance search-string ptr */
					if (++sm.c > CELLSIZE-1) {
						sm.p = sm.p->f;
						sm.c = 0;
					}

					/* advance buffer ptr */
					if (++sb.c > CELLSIZE-1) {
						sb.p = sb.p->f;
						sb.c = 0;
					}
				}

				/* search matches: */
				if (sm.dot >= sm.z) {

					/*
					 * Set last_z to point where this
					 * string was found
					 */
					if (!(ed_val & ED_SMULT))
						last_z = aa.dot;
					break;
				}

				/* or if string is beyond end of buffer */
				if (sb.dot >= last_z) {

					/*
					 * Make search appear to have
					 * succeeded
					 */
					sm.dot = sm.z;

					/*
					 * So as to back up pointer, and
					 * force one more look
					 */
					--count;
					break;
				}

				/* else advance buffer pointer (backwards) */
				if (--aa.c < 0) {
					aa.p = aa.p->b;
					aa.c = CELLSIZE-1;
				}
			}

			/* if one search failed, don't do more */
			if (sm.dot < sm.z)
				break;
			else {

				/*
				 * If this is not last search, back
				 * pointer up one
				 */
				if (count < -1)
					backc(&aa);
				else {

					/*
					 * Otherwise save -length of string
					 * found
					 */
					ctrl_s = aa.dot - sb.dot;

					/* advance pointer past string */
					aa.dot = sb.dot;
					aa.p = sb.p;
					aa.c = sb.c;
				}
			}
		}
	}

	/* if search succeeded, update pointer	*/
	if (sm.dot >= sm.z)
		dot = aa.dot;

	/* set "search occurred" (for ES) */
	search_flag = 1;

	/* and return -1 (pass) or 0 (fail) */
	return((sm.dot >= sm.z) ? -1 : 0);
}

/*
 * expanded search comparison
 * returns 1 if match, 0 if not
 */
srch_cmp()
{
	int tq;					/* q-reg name for ^EGq */
	struct qp tqp;				/* pointer to read q reg */

	/* what is search character */
	switch (mapch_l[sm.p->ch[sm.c]]) {

		/* match anything but following construct */
		case CTL (N):
			/* don't read past end of string */
			if (sm.dot >= sm.z)
				ERROR(E_ISS);

			/* skip the ^N */
			fwdc(&sm);
			return(!srch_cmp());

		/* match any character */
		case CTL (X):
			return(1);

		/* take next char literally */
		case CTL (Q):
		case CTL (R):

			/* don't read past end of string */
			if (sm.dot >= sm.z)
				ERROR(E_ISS);

			/* skip the ^Q */
			fwdc(&sm);
			return(*(pmap + sb.p->ch[sb.c]) ==
				*(pmap + sm.p->ch[sm.c]));

		/* match any nonalphanumeric */
		case CTL (S):
			return(!isalnum(sb.p->ch[sb.c]));

		case CTL (E):
			/* don't read past end of string */
			if (sm.dot >= sm.z)
				ERROR(E_ISS);

			/* skip the ^E */
			fwdc(&sm);

			switch (mapch_l[sm.p->ch[sm.c]]) {

			/* match any alpha */
			case 'a':
				return(isalpha(sb.p->ch[sb.c]));

			/* match any nonalpha */
			case 'b':
				return(!isalnum(sb.p->ch[sb.c]));

			/* rad50 symbol constituent */
			case 'c':
				return(!isalnum(sb.p->ch[sb.c]) ||
					(sb.p->ch[sb.c] == '$') ||
					(sb.p->ch[sb.c] == '.'));

			/* digit */
			case 'd':
				return(isdigit(sb.p->ch[sb.c]));

			/* line terminator LF, VT, FF */
			case 'l':
				return((sb.p->ch[sb.c] == LF) ||
					(sb.p->ch[sb.c] == FF) ||
					(sb.p->ch[sb.c] == VT));

			/* alphanumeric */
			case 'r':
				return(isalnum(sb.p->ch[sb.c]));

			/* lower case */
			case 'v':
				return(islower(sb.p->ch[sb.c]));

			/* upper case */
			case 'w':
				return(isupper(sb.p->ch[sb.c]));

			/* any non-null string of spaces or tabs */
			case 's':
				/* failure */
				if (((sb.p->ch[sb.c]&0177) != ' ') &&
						((sb.p->ch[sb.c]&0177) != TAB))
					return(0);

				/* skip remaining spaces or tabs */
				for ( fwdc(&sb);
					((sb.p->ch[sb.c]&0177) == ' ') ||
					((sb.p->ch[sb.c]&0177) == TAB);
					fwdc(&sb) )
				   ;

				/*
				 * Back up one char (calling routine will
				 * skip it)
				 */
				backc(&sb);
				return(1);		/* success */

			/* any char in specified q register */
			case 'g':
				/* don't read past end of string */
				if (sm.dot >= sm.z)
					ERROR(E_ISS);

				/* get to the next char */
				fwdc(&sm);

				/* read q-reg spec */
				tq = getqspec(1, sm.p->ch[sm.c]);

				for (tqp.dot = tqp.c = 0, tqp.p = qreg[tq].f;
						tqp.dot < qreg[tq].z;
						fwdc(&tqp))

					/* match */
					if (*(pmap + tqp.p->ch[tqp.c]) ==
						*(pmap + sb.p->ch[sb.c]))
							return(1);

				return(0);		/* fail */

			default:
				ERROR(E_ISS);
			}
	default:
		return(*(pmap + sb.p->ch[sb.c]) == *(pmap + sm.p->ch[sm.c]));
	}
}
