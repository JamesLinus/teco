/*
 * TECO for Ultrix   Copyright 1986 Matt Fichtenbaum
 * This program and its components belong to GenRad Inc, Concord MA 01742
 * They may be copied if this copyright notice is included
 *
 * te_window.c   window for teco   10/10/86
 * This attempts to be a real window, without unecessary redraw
 * it is very VT-100 specific, and ought to be rewritten to be general
 */
#include <signal.h>
#ifdef SIGWINCH
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/termio.h>
#undef TAB
#endif
#include "te_defs.h"

extern int ospeed;

/* maximum screen height and width (horiz and vert, not height and vidth) */
#define W_MAX_V 70
#define W_MAX_H 150

/* maximum positive integer, for "last modified" location */
#define MAX 0x7fffffff
#define W_MARK 0200	/* "this loc is special" in screen image */


/* image of current window */

struct w_line			/* data associated with one screen line */
{
	int start, end;		/* dot at beginning, at end */
	short n;		/* # char positions used */
	short cflag;		/* continuation flag */
	short col;		/* starting col */
	char ch[W_MAX_H];	/* image of line */
}
w_image[W_MAX_V];


/* define "this line is continued" / "this line is a continuation" flags */
#define WF_BEG 1
#define WF_CONT 2

/* each word points to the corresponding line's data structure */
struct w_line *wlp[W_MAX_V];

struct qp w_p1;			/* pointer for window access to buffer */

short curr_x, curr_y;		/* active character position */
short term_x, term_y;		/* current terminal cursor position */
short curs_x, curs_y;		/* current teco dot screen coordinates */
short last_y;			/* last used line in window */
char curs_c;			/* code for char at cursor */
char *curs_p;			/* pointer to cursor loc in window image */
short curs_crflag;		/* flag that cursor is on a CR */
short redraw_sw;		/* forces absolute redraw */


/*
 * fill characters and terminal speeds:
 * 0th entry used when std out is not a terminal
 */
char win_speeds[] = { 
	0, 0, B9600, B4800, B2400, B1800, B1200, B600,
	B300, B200, B150, B134, B110 };

/* delay for erase-screen */
char win_dlye[] =   { 
	0, 90, 45, 23, 11, 9, 6, 3, 1, 1, 1, 1, 1 };

/* delay for scroll ops */
char win_dlys[] =   { 
	0, 60, 30, 15, 7, 6, 4, 2, 1, 1, 0, 0, 0 };

/* delay for erase line */
char win_dlyl[] =   { 
	0, 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* delay for other control functions */
char win_dlyc[] =   {
	0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
short win_speed;

/*
 * routine to perform simple scope operations
 * (an attempt to concentrate VT-100 specific things in one place)
 */
vt(func)
	int func;
{
	short t;
	switch (func)
	{
	case VT_CLEAR:			/* clear screen */
		fputs("\033[H\033[J", stdout);
		for (t = 0; t < win_dlye[win_speed]; t++)
			putchar('\0');	/* fill chars */
		break;

	case VT_EEOL:			/* erase to end of line */
		fputs("\033[K", stdout);
		for (t = 0; t < win_dlyl[win_speed]; t++)
			putchar('\0');	/* fill chars */
		break;

	case VT_EBOL:			/* erase from beginning of line */
		fputs("\033[1K", stdout);
		for (t = 0; t < win_dlyl[win_speed]; t++)
			putchar('\0');	/* fill chars */
		break;

	case VT_SETSPEC1:		/* reverse video */
		fputs("\033[7m", stdout);
		break;

	case VT_SETSPEC2:		/* bright reverse video */
		fputs("\033[1;7m", stdout);
		break;

	case VT_CLRSPEC:		/* normal video */
		fputs("\033[0m", stdout);
		break;

	case VT_BS1:			/* backspace 1 spot */
		fputs("\b \b", stdout);
		break;

	case VT_BS2:			/* backspace 2 spots */
		fputs("\b \b\b \b", stdout);
		break;

	case VT_LINEUP:			/* up one line */
		fputs("\033[1A", stdout);
		break;
	}
}

/* routine to set window parameters */

/*
 * 0: scope type, 1: width, 2: height, 3: seeall, 4: mark position,
 * 5: hold mode, 6: upper left corner position, 7: scroll region size
 */

/* min values for window parameters */
int win_min[]  = { 
	4,  20,		4,		 0, 0,	 -1,  1,   0 };

/* max values */
int win_max[]  = { 
	4,  W_MAX_H,	W_MAX_V, 1, MAX, 12, -1,  20 };

/* window parameters	*/
int win_data[] = { 
	4,  132,		24,		 0, 0,	  0,  0,   0 };

/* # of lines in a window */
int window_size;

do_window(ref_flag)
	int ref_flag;		/* nonzero forces "refresh" operation */
{
	int i;

	if (colonflag && !ref_flag) {
		i = get_value(0);	/* get sub-function */
		if ((i < 0) || (i > 7))
			ERROR(E_IWA);

		/* it's a "get" */
		if (!esp->flag2) {
			esp->val1 = win_data[i];
			esp->flag1 = 1;
		} else {
			/* check range */
			if ((esp->val2 < win_min[i]) ||
					(esp->val2 > win_max[i]))
				ERROR(E_IWA);
			if (i == 7) {
				if (esp->val2) {
					WN_scroll = esp->val2;

					/* define size of window area */
					window_size = WN_height - WN_scroll;

					/* turn on window */
					window(WIN_INIT);
				} else
					window(WIN_OFF);/* turn off window */
			}

			/* redundant for ~0,7:w, but no harm */
			win_data[i] = esp->val2;
			esp->flag2 = 0;

			/* redraw window */
			window(WIN_REDRAW);
		}
	} else {
		/* no colon, or ^W command */
		if (esp->flag1 || ref_flag) {
			/* -1000W: "forget that output was done" */
			if (!ref_flag &&
					((esp->val1 == -1000) ||
					 (esp->val1 == 32768)))
				redraw_sw = 0;
			else
				/* nW or ^W refreshes window */
				window(WIN_DISP);
		}
		/* no colon, consume args */
		esp->flag2 = esp->flag1 = 0;
	}
	colonflag = 0;
	esp->op = OP_START;
}

/*
 * routine to update screen size with numbers obtained from environment
 * (called by main program's initialization)
 */
#ifdef SIGWINCH
static struct winsize w;
void
recalc_tsize()
{
	if (ioctl(0, TIOCGWINSZ, &w) >= 0) {
		WN_height = w.ws_row;
		WN_width = w.ws_col;
		window_size = WN_height - WN_scroll;
		window(WIN_INIT);
		window(WIN_REDRAW);
		window(WIN_REFR);
	}
}
#else
recalc_tsize()
{
	/* No-op */
}
#endif

set_term_par(lines, cols)
int lines, cols;
{
#ifdef SIGWINCH
	struct sigaction act;
	static int act_setup = 0;
#endif

	if ((lines >= win_min[2]) && (lines <= win_max[2]))
		window_size = win_data[2] = lines;
	if ((cols >= win_min[1]) && (cols <= win_max[1]))
		win_data[1] = cols;
#ifdef SIGWINCH
	if (!act_setup) {
		act.sa_handler = recalc_tsize;
		act.sa_mask = 0L;
		act.sa_flags = 0;
		(void)sigaction(SIGWINCH, &act, (struct sigaction *)0);
		act_setup = 1;
	}
#endif
}


/*
 * window routine.  performs function as indicated by argument
 * WIN_OFF:		disables split-screen scrolling
 * WIN_SUSP:	disables split-screen scrolling temporarily
 * WIN_INIT:	sets up display support if
 *			split-screen scrolling enabled, else nop
 * WIN_RESUME:	re-enables display support
 * WIN_REDRAW:	causes window to be redrawn on next refresh call
 * WIN_REFR:	if scrolling enabled, redoes
 *			window, else if ev or es enabled, does
 *				that, else nop
 * WIN_LINE:	does WIN_REFR unless that wouldn't do anything, in which case
 *				it does effective 1EV output
 */
int last_dot = -1;				/* last dot location */

window(arg)
	int arg;
{
	int i;

	switch (arg)
	{
	case WIN_OFF:				/* final window off */
	case WIN_SUSP:				/* temp window off */
		/* if reset/clean up */
		if (WN_scroll) {
			/* full margins, cursor to last line, erase line */
			printf("\033[r\033[%d;0H\033[K", WN_height);
		}
		break;

	/* initialize window - find output speed */
	case WIN_INIT:
		if (out_noterm) {
			/* std out is not a terminal */
			win_speed = 0;
		} else {
			for (win_speed = 1;
				(win_speeds[win_speed] != ospeed) &&
					(win_speed < B38400);
				win_speed++)
					;
			if (win_speed == 0)
				win_speed = B19200;
		}

		/* set up screen image buffer */
		w_init();

		/* if split-screen is enabled, clear screen */
		if (WN_scroll)
			vt(VT_CLEAR);

		/* (fall through to "resume") */

	/* re-enable window */
	case WIN_RESUME:

		if (WN_scroll) {
			/* set scroll region, cursor to bottom */
			printf("\033[%d;%dr\033[%d;0H",
				WN_height - WN_scroll + 1,
				WN_height, WN_height);
		}
		break;

	/* force redraw of window */
	case WIN_REDRAW:
		redraw_sw = 1;
		break;

	/* display one line unless window enabled or ev */
	case WIN_LINE:
		if (WN_scroll || ev_val) {
			/* if a real window is set, do it */
			window(WIN_REFR);
		} else if (w_setptr(dot, &w_p1)) {
			/*
			 * set pointer to dot... and if there's a buffer
			 * get to beginning of line and type 1 line
			 */
			w_lines(0, &w_p1, &w_p1);
			window0(1);
		}
		break;

	/* if enabled, refresh window; else do ev or es */
	case WIN_REFR:
		if (WN_scroll) {
			/* if scrolling enabled, refresh the window */
			window1();
		} else if ((ev_val) || (es_val && search_flag)) {
			/* else if ev or es, do that */
			i = (ev_val) ? ev_val : es_val;

			/*
			 * set a pointer at dot... and if there's a buffer
			 * go back (i-1) lines and ahead (i) lines
			 */
			if (w_setptr(dot, &w_p1))
				window0(i - w_lines(1 - i, &w_p1, &w_p1));
		}
		break;

	/* display buffer independent of whether scroll mode is enabled */
	case WIN_DISP:
		window1();
		break;

	} /* end of switch */

	/*
	 * Send output out end of window()
	 */
	fflush(stdout);
}

/*
 * routine to type n lines with character at "dot" in reverse video
 * used for ev, es, and <BS> or <LF> as immediate commands
 * starting char position is in w_p1; argument is number of lines
 */
window0(num)
	int num;
{
	int wi;
	char wc;

	/* for each character */
	for (wi = w_p1.dot; (num > 0) && (wi < z); wi++) {
		wc = w_p1.p->ch[w_p1.c];	/* get character */

		/* if about to exceed width */
		if ((char_count >= WN_width) &&
			(wc != CR) && !(spec_chars[wc] & A_L)) {

			/* truncate: don't print this */
			if (et_val & ET_TRUNC)
				goto w0_noprint;
			else {
				/* <eeol> "NL space" */
				fputs("\033[K\015\012\033(0h\033(B ", stdout);
				char_count = 2;

				/* one fewer line remaining */
				--num;
			}
		}

		/* if this char is at the pointer */
		if (wi == dot) {

			/* set reverse video */
			vt(VT_SETSPEC2);
			if (wc == TAB) {
				/*
				 * Illuminate first space of tab, and
				 * clear reverse video
				 */
				type_char(' ');
				vt(VT_CLRSPEC);
				if (char_count & tabmask)
					type_char(TAB);
			} else {
				/*
				 * Not a tab; if CR at right-hand margin,
				 * don't display cursor.
				 */
				if ((wc == CR) && (char_count < WN_width)) {
					/*
					 * CR: put space, erase to eol
					 */
					type_char(' ');
					vt(VT_EEOL);
				}

				/* type the char, or exec CR */
				type_char(wc);
				if (wc == LF) {
					fputs("\033(0", stdout);
					type_char('e');
					fputs("\033(B", stdout);
				}

				/* clear reverse video */
				vt(VT_CLRSPEC);
			}
		} else {
			/*
			 * This is not the char at pointer... erase eol
			 */
			if (wc == CR && curr_x < WN_width)
				vt(VT_EEOL);
			type_char(wc);
		}

		/* FF & VT end a line */
		if ((wc == FF) || (wc == VT)) {
			/*
			 * Erase rest of this line and leave a blank one
			 */
			vt(VT_EEOL);
			crlf();

			/* if FF and VT count as line sep's, count them */
			if (!(ez_val & EZ_NOVTFF))
				--num;
		}

w0_noprint:
		/* next char */
		if (++w_p1.c > CELLSIZE-1) {
			w_p1.p = w_p1.p->f;
			w_p1.c = 0;
		}

		/* if this is a line feed, count lines */
		if (wc == LF)
			--num;
	}

	/* type one space and erase rest of line */
	if (dot == z)
		fputs("\033[1;7m \033[0m\033[0K", stdout);
	else
		/* else just erase to EOL */
		fputs("\033[0K", stdout);
}

/*
 * Routine to maintain the screen window
 * if scroll mode is enabled, the VT100 screen is split and only the upper part
 * is used by this routine; else the whole screen is used
 */
window1()
{
	int i, j, m, lflag;

	/* return if nothing has changed */
	if (!redraw_sw && (dot == last_dot) && (buff_mod == MAX))
		return;

	/* disable ^C interrupts */
	block_inter(1);

	/* scroll mode: redefine scroll region */
	if (WN_scroll)
		printf("\033[1;%dr", window_size);

	/* home */
	printf("\033[H");
	term_y = term_x = 0;		/* indicate cursor is at home */

	/* forced redraw, or z before start of screen */
	if ((redraw_sw) || (z <= wlp[0]->start))
		window1_abs();


	/* check whether pointer is before modified buffer location */

	else if (buff_mod >= dot) {
		/* yes */

		/* if dot is before screen */
		if (dot < wlp[0]->start) {

			/* get to beginning of screen */
			w_setptr(wlp[0]->start, &w_p1);

			/*
			 * check whether screen begins with the last
			 * part of a continued line
			 */
			for (j = 0; (wlp[j]->cflag & WF_CONT) &&
					(j < window_size/2); j++)
				;
			/*
			 * If so, does it continue less than halfway down
			 * the screen?
			 */
			if (j < window_size/2) {
				/* is there a partial line? */
				if (j) {
					w_lines(0, &w_p1, &w_p1);
					/*
					 * Now j is number of display lines
					 * before screen
					 */
					j -= w_lines(1, &w_p1, NULL);
				}
/*
 * now look for how many lines back "dot" is: if screen starts with
 * partial line, w_p1 has already been moved to beginning of the line
 * and j equals the count of extra lines to scroll
 */

				for (i = 0; (dot < w_p1.dot) &&
						(i < window_size/2); )
					i -= w_lines(-1, &w_p1, &w_p1);

				/* found point within reason */
				if ((dot >= w_p1.dot) && (i < window_size)) {
					/* scroll down that many lines */
					w_scroll(j - i);

					/* start from top of screen */
					curr_y = wlp[0]->cflag =
						wlp[0]->col = curr_x = 0;

					/* save starting char position */
					wlp[0]->start = w_p1.dot;

					/* and rewrite screen */
					window2(0);
				} else {
					/* farther back than that - redraw */
					window1_abs();
				}
			} else {
				/*
				 * Continuation was too long:
				 * give up and redraw
				 */
				window1_abs();
			}
		} /* end of "dot is before screen" */

		/* on screen - redraw incrementally */
		else if (dot <= wlp[last_y]->end) {
			window1_inc(dot);

		} else {
			/* dot is after screen: scroll or redraw */
			window1_after();
		}
	} /* end of "dot is before modified point" */


	/* the modified point in the buffer is before dot */

	else
	{
		/* modified point before screen - redraw fully */
		if (buff_mod < wlp[0]->start)
			window1_abs();

		/* modified point on screen */
		else if (buff_mod <= wlp[last_y]->end) {
			/* find line with buff_mod */
			for (m = 0; buff_mod > wlp[m]->end; m++)
				;
			/* set a pointer to start of line with buff_mod */
			w_setptr(wlp[m]->start, &w_p1);

			/* maximum # of lines between buff_mod & dot */
			j = (m < window_size/2) ?
				window_size - 1 - m : window_size/2;

			/* count lines from buff_mod to first line after dot */
			for (i = 0; (dot >= w_p1.dot) && (w_p1.dot < z) &&
					(i <= j); )
				i += (lflag = w_lines(1, &w_p1, &w_p1) ) ?
					lflag : 1;
			/* too far - redraw */
			if (i > j)
				window1_abs();
			else {
				/* if at end, following a LF */
				if (lflag && (dot == z))
					i++;

				/* pointer to start of area to redraw */
				w_setptr(wlp[m]->start, &w_p1);

				/* if not enough blank lines on screen */
				if (i >= window_size - m) {
					/* scroll up the difference */
					w_scroll(i = i - window_size + m),
						curr_y = m - i, curs_y -= i;
				} else {
					curr_y = m;
				}
				/* line starts at left unless continuation */
				curr_x = (wlp[curr_y]->cflag & WF_CONT) ?
					2 : wlp[curr_y]->col;
				/* remove old cursor if won't be written over */
				if ((curr_y > curs_y) && (curs_y >= 0))
					w_rmcurs();

				/* rewrite newly cleared region */
				window2(0);

				/* clear rest of screen if needed */
				for (curr_x = 0; ++curr_y < window_size; ) {
					wlp[curr_y]->cflag = 0;
					if (wlp[curr_y]->n) {
						wlp[curr_y]->n = 0;
						vtm(VT_EEOL);
					}
				}
			}
		} /* end "modified point on screen */

		else {
			/*
			 * Modified point after screen: scroll
			 * or redraw as appropriate
			 */
			window1_after();
		}
	}

	/* done redrawing: do cleanup work */

	if (WN_scroll) {
		/* reset margins */
		printf("\033[%d;%dr", window_size+1, WN_height);

		/* cursor to bottom */
		printf("\033[%d;0H", WN_height);
	} else {
		printf("\033[H");		/* no split screen: set home */
	}

	fflush(stdout);			/* flush output */
	WN_origin = wlp[0]->start;	/* save first char pos on screen */
	redraw_sw = 0;			/* mark screen as updated */
	buff_mod = MAX;
	last_dot = dot;
	block_inter(0);			/* reenable interrupts */
}

/* routine to redraw screen absolutely */
window1_abs()
{
	int i, j;

	curr_y = wlp[0]->col = curr_x = 0; /* indicate where refresh starts */

	/* make a text buffer, if none, and refresh the display */
	set_pointer(dot, &w_p1);
	w_lines(0, &w_p1, &w_p1);

	/* check how many lines after dot */
	if ((i = w_lines(window_size/2, &w_p1, NULL)) == 0)
		i = 1;

	/* limit amount after dot */
	if (i > window_size/2)
		i = window_size/2;

	/* find start of display area */
	for (j = 0; (j < window_size - i) && (w_p1.dot > 0); ) {
		j -= w_lines(-1, &w_p1, &w_p1);
	}

	/* if too far back, move up one line */
	if (j > window_size - i) w_lines(1, &w_p1, &w_p1);

	/* indicate where first window line starts */
	wlp[0]->start = w_p1.dot;

	/* refresh the whole display */
	window2(0);

	/* blank out lines not written by window2 */
	for (curr_x = 0; ++curr_y < window_size; ) {
		if (wlp[curr_y]->n || redraw_sw)
			wlp[curr_y]->n = 0, vtm(VT_EEOL);
	}
}

/* redraw screen incrementally */

window1_inc(wd)
int wd;						/* argument is earliest change */
{
	short temp_y;

	/* find the line containing the character at wd */

	for (temp_y = 0; wd > wlp[temp_y]->end; temp_y++)
		;

	/* if the cursor line won't be rewritten */
	if ((curs_y != temp_y) || (buff_mod == MAX) || curs_crflag) {
		/* remove the old cursor */
		w_rmcurs();
	}

	/* and go to work on the beginning of the line with dot */
	curr_y = temp_y;

	/* line starts at left unless continuation */
	curr_x = (wlp[curr_y]->cflag & WF_CONT) ? 2 : wlp[curr_y]->col;

	/* make a pointer there */
	w_setptr(wlp[curr_y]->start, &w_p1);

	/* if buffer not modified, redraw only the line with dot */
	window2(buff_mod == MAX);

	/* if buffer has changed, erase display lines beyond end of buffer */
	if (buff_mod < MAX) {
		for (curr_x = 0; ++curr_y < window_size; )
			if ( ((wlp[curr_y]->start >= z) ||
				(wlp[curr_y]->start <= wlp[curr_y-1]->end)) &&
				(wlp[curr_y]->n || redraw_sw) ) {
			  wlp[curr_y]->n = 0, vtm(VT_EEOL);
			  wlp[curr_y]->cflag = 0;
			}
	}
}

/* routine to move window downwards: scroll up or redraw as appropriate */

window1_after()
{
	int i, lflag;

	/* remove old cursor */
	w_rmcurs();

	/* set pointer to start of last line on screen */
	w_setptr(wlp[window_size-1]->start, &w_p1);

	for (i = 0; (dot >= w_p1.dot) &&
			(w_p1.dot < z) && (i <= window_size/2); ) {
		/* fwd one line at a time until > dot or end of buffer */
		i += (lflag = w_lines(1, &w_p1, &w_p1)) ? lflag : 1;
	}
	/* found within n lines */
	if (i <= window_size/2) {
		/* if dot is at end of buffer after a LF */
		if (lflag && (dot == z))
			++i;

		/* if there are not enough blank lines on screen */
		if (i >= window_size - last_y) {
			/* scroll up the difference */
			w_scroll(i - window_size + last_y),
				curr_y = window_size - i;
		} else {
			curr_y = last_y;
		}

		/* get to start of cont'd lines */
		while (curr_y && (wlp[curr_y]->cflag & WF_CONT))
			--curr_y;

		/* pointer to start of area to redraw */
		w_setptr(wlp[curr_y]->start, &w_p1);

		/* redraw starts at line's first column */
		curr_x = wlp[curr_y]->col;

		/* rewrite newly cleared region */
		window2(0);
	} else {
		/* move down is too far: redraw fully */
		window1_abs();
	}
}

/* routine to remove the existing cursor */
w_rmcurs()
{
	/* if there was a cursor */
	if (curs_c) {
		/* go remove the old cursor */
		w_move(curs_y, curs_x);

		/* if prev char was a spec char */
		if (curs_c & W_MARK)
			fputs("\033(0", stdout);

		/* put back the char that was there */
		putchar(*curs_p = curs_c);

		if (curs_c & W_MARK)
			fputs("\033(B", stdout);

		/* and keep the terminal cursor loc. happy */
		++term_x;
	}
}

/*
 * routine to do actual display refresh
 * called with w_p1 at starting char, curr_y, curr_x at starting coordinate
 * rewrites to end of screen if arg = 0, or only until line with cursor if
 * arg = 1
 */
window2(arg)
	int arg;
{
	register int wdot;
	register char wc;
	register short dflag;		/* nonzero if this is char at dot */
	short cr_found;			/* indicates a cr found on this line */

	/* clear "cr" flag in first line written */
	cr_found = 0;

	/* for each character */
	for (wdot = w_p1.dot; (curr_y < window_size) && (wdot < z); wdot++) {

		/* get character */
		wc = w_p1.p->ch[w_p1.c] & 0177;

		/* save "this is char at dot", "on line with dot" */
		if (dflag = (wdot == dot))
			if (arg) arg = -1;

		/* dispatch control characters */
		if (wc < ' ') switch (wc) {
		case CR:
			/* if cursor on this CR */
			if (dflag) {
				/* display a space, unless at end */
				if (curr_x < WN_width)
					w_makecurs(' ', 1), w_type(' ', 1);
				else
					/* else set "no cursor displayed" */
					curs_crflag = curs_c = 0;
			}

			/*
			 * trim remainder of line if this is
			 * first cr and old line was longer
			 */
			if (!cr_found && ((curr_x < wlp[curr_y]->n) ||
					redraw_sw)) {
				wlp[curr_y]->n = curr_x;
				if (curr_x < WN_width)
					vtm(VT_EEOL);
			}
			cr_found = 1;

			/* this line is not continued */
			wlp[curr_y]->cflag &= ~WF_BEG;

			/* if line is a continuation, scan up */
			while (curr_y && (wlp[curr_y]->cflag & WF_CONT))
				--curr_y;
			curr_x = 0;
			break;

		case TAB:
			if (curr_x >= WN_width) {
				if (et_val & ET_TRUNC)
					goto noprint;

				/* extend line */
				if (w_overflow(wdot))
					goto w2_exit;
			}
			if (dflag)
				w_makecurs(' ', 0);

			/* type one space */
			w_type(' ', dflag);
			if (dflag) {
				vt(VT_CLRSPEC);		/* end reverse video */
				dflag = 0;
			}

			/* finish tab */
			while ((curr_x & tabmask) && (curr_x < WN_width))
				w_type(' ', 0);
			break;
		case LF:
			/* last screen row of this line */
			while ((curr_y < window_size) &&
					(wlp[curr_y]->cflag & WF_BEG))
				++curr_y;

			/* save char position that ended this line */
			wlp[curr_y]->end = wdot;

			/* if this LF is at dot */
			if (dflag) {
				/* put cursor, save char that was there */
				w_makecurs( (curr_x < wlp[curr_y]->n) ?
					wlp[curr_y]->ch[curr_x] : ' ', 0);

				/* put in a "LF" char */
				fputs("\033(0", stdout);
				w_type('e', 1);

				fputs("\033(B", stdout);
			}

			/*
			 * if no cr found and not in last column,
			 * erase rest of line
			 */
			if (!cr_found && (curr_x < wlp[curr_y]->n)) {
				wlp[curr_y]->n = curr_x;
				if (curr_x < WN_width) vtm(VT_EEOL);
			}
			/*
			 * put the cursor back before the artificial LF char,
			 * if any
			 */
			if (dflag)
				--curr_x;

			/* if at end of screen, exit, but... */
			if (curr_y >= window_size-1) {
				/*
				 * if cursor is here,
				 * clear reverse video first
				 */
				if (dflag)
					vt(VT_CLRSPEC);
				goto w2_exit;
			}
			/*
			 * if a now-empty cont. line, flush it
			 */
			if ((wlp[curr_y]->cflag & WF_CONT) &&
				(wlp[curr_y]->end - wlp[curr_y]->start == 1)) {

				/* remove "cont'd" flag from prev line */
				if (curr_y > 0)
					wlp[curr_y-1]->cflag &= ~WF_BEG;

				/* and force redraw of rest of screen */
				arg = 0;

				/*
				 * if cursor was on this line,
				 * it will disappear
				 */
				if (curs_y == curr_y)
					curs_c = 0;
			} else {
				/*
				 * down one line if not absorbing blank
				 * contin. line
				 */
				++curr_y;
			}
			/* assume line starts with next char */
			wlp[curr_y]->start = wdot + 1;

			/* save starting column */
			wlp[curr_y]->col = curr_x;

			/* clear line continuation flags */
			cr_found = wlp[curr_y]->cflag = 0;

			/* if not at left margin, erase beginning of line */
			if (curr_x)
				w_ebol();

			/* finished line with dot... quit if spec'd */
			if (arg == -1) {
				/*
				 * but first, if at cursor,
				 * clear reverse video
				 */
				if (dflag) {
					vt(VT_CLRSPEC);
					dflag = 0;
				}
				return;
			}
			break;

		case ESC:
			if (curr_x >= WN_width) {
				if (et_val & ET_TRUNC)
					goto noprint;

				/* extend line */
				if (w_overflow(wdot))
					goto w2_exit;
			}
			if (dflag)
				w_makecurs('$', 0);
			w_type('$', dflag);
			break;

		/* all other control chars print as ^X */
		default:
			if (curr_x >= WN_width - 1) {
				if (et_val & ET_TRUNC)
					goto noprint;
				if (w_overflow(wdot))
					goto w2_exit;
			}
			if (dflag)
				w_makecurs('^', 0);
			w_type('^', dflag);
			if (dflag) {
				/* if at cursor, clear reverse video */
				vt(VT_CLRSPEC);
				dflag = 0;
			}
			w_type(wc | 0100, 0);
			break;
		} else {
			/* a printing character */
			if (curr_x >= WN_width) {
				if (et_val & ET_TRUNC)
					goto noprint;

				/* extend line */
				if (w_overflow(wdot))
					goto w2_exit;
			}
			if (dflag)
				w_makecurs(wc, 0);
			w_type(wc, dflag);
		}

		if (dflag) {
			/* if at cursor, clear reverse video */
			vt(VT_CLRSPEC);
		}

		/* these chars leave a display line blank */
		if ((wc == FF) || (wc == VT)) {
			if (redraw_sw || (curr_x < wlp[curr_y]->n)) {
				wlp[curr_y]->n = curr_x;

				/* erase rest of line */
				if (curr_x < WN_width)
					vtm(VT_EEOL);
			}
			wlp[curr_y]->end = wdot;

			/* quit if overflow screen */
			if (curr_y >= window_size-1)
				goto w2_exit;
			wlp[++curr_y]->start = wdot + 1;

			/* init new line */
			cr_found = wlp[curr_y]->cflag = 0;

			/*
			 * back up over ^X; if not at left margin,
			 * erase beginning of line
			 */
			if (curr_x -= 2)
				w_ebol();

			/* save starting column */
			wlp[curr_y]->col = curr_x;
		}
noprint:
		if (++ w_p1.c > CELLSIZE - 1) {
			w_p1.p = w_p1.p->f;
			w_p1.c = 0;	/* next char in buffer */
		}
	} /* end of "for all characters" */

	if (dot == z) {
		if (curr_x < WN_width) {
			/* display a space, unless at end */
			w_makecurs(' ', 1);
			w_type(' ', 1);
			vt(VT_CLRSPEC);
		} else {
			/* else set "no cursor displayed" */
			curs_crflag = curs_c = 0;
		}
	}

	/* clear rest of line if needed */
	if (!cr_found && (redraw_sw || (curr_x < wlp[curr_y]->n))) {
		wlp[curr_y]->n = curr_x;
		if (curr_x < WN_width)
			vtm(VT_EEOL);
	}

	/* save char at end of last line */
	wlp[curr_y]->end = wdot;
w2_exit:
	/* record last used line on screen */
	last_y = curr_y;
}

/* routine to move cursor to current location and then call vt */
vtm(arg)
	int arg;
{
	w_move(curr_y, curr_x);
	vt(arg);
}

/*
 * routine to set reverse video and save cursor location
 * first argument is char at cursor, 2nd is value for curs_crflag
 */
w_makecurs(wc, crflag)
	char wc;
	short crflag;
{
	/* save cursor coord and char */
	curs_y = curr_y;
	curs_x = curr_x;
	curs_c = wc;

	/* save location of cursor spot in window image */
	curs_p = &wlp[curr_y]->ch[curr_x];

	/* save crflag */
	curs_crflag = crflag;

	/* set flag and reverse video */
	vt(VT_SETSPEC2);
}

/*
 * routine to handle line overflow
 * returns nonzero if at end of screen, zero otherwise
 * arg is current character position
 */
w_overflow(wd)
{
	/* last character was end of this line */
	wlp[curr_y]->end = wd-1;
	if (wlp[curr_y]->n > curr_x) {
		wlp[curr_y]->n = curr_x;

		/* if old line was wider, erase */
		if (curr_x < WN_width)
			vtm(VT_EEOL);
	}
	if (curr_y >= window_size-1)
		return(1);

	/* mark this line as "continued" */
	wlp[curr_y]->cflag |= WF_BEG;

	/* next line is a continuation line */
	wlp[++curr_y]->cflag = WF_CONT;

	/* char about to be printed is this line's first */
	wlp[curr_y]->start = wd;

	/* new line starts at left margin */
	wlp[curr_y]->col = curr_x = 0;

	/* alternate char set */
	fputs("\033(0", stdout);

	/* "NL" space */
	w_type('h', W_MARK);
	w_type(' ', W_MARK);
	fputs("\033(B", stdout);
	return(0);
}

/*
 * routine to type one character:  arguments are char and a
 * "mark" bit.  If mark is set, the char is always retyped
 */
w_type(c, m)
	char c;
	int m;
{
	register char *p;

	/* pointer to char image */
	p = &wlp[curr_y]->ch[curr_x];
	if ((c != *p) || (m) || (redraw_sw) || (curr_x >= wlp[curr_y]->n)) {
		w_move(curr_y, curr_x);
		putchar(c);
		*p = (m) ? c | W_MARK : c;
		++term_x;
	}
	++curr_x;

	/* if we've lengthened the line, record that fact */
	if (wlp[curr_y]->n < curr_x)
		wlp[curr_y]->n = curr_x;
}

/* initialize display image */
w_init()
{
	short i, j;

	/* for each row */
	for (i = 0; i < window_size; i++) {
		/* set pointer to this line's data */
		wlp[i] = &w_image[i];

		/* no chars used, cr flag clear */
		w_image[i].n = w_image[i].cflag = 0;

		/* clear line */
		for (j = 0; j < W_MAX_H; w_image[i].ch[j++] = ' ')
			;
	}
}

/*
 * move terminal cursor to stated y, x position
 * uses incremental moves or absolute cursor position, whichever is shorter
 */
w_move(y, x)
	short y, x;
{
	register short i;

	/* if practical, use CR to get to left margin */
	if ((curr_x == 0) && (term_x != 0)) {
		putchar(CR);
		term_x = 0;
	}

	/* if term x is beyond last char, use abs positioning */
	if ((y == term_y) && (term_x < WN_width)) {
		if (x == term_x)
			return;
		if (x > term_x) {
			if (x - term_x == 1)
				fputs("\033[C", stdout);
			else
				printf("\033[%dC", x - term_x);
		} else {
			if ((i = term_x - x) < 4) for (; i > 0; i--)
				putchar('\b');	/* use BS */
			else
				/* use incremental jump */
				printf("\033[%dD", term_x - x);
		}
		term_x = x;
	} else {
		if ((x == term_x) && (term_x < WN_width)) {
			if (y > term_y) {
				if ((i = y - term_y) < 4)
					for (; i >0; i--)
						putchar(LF);	/* use LF */
				else
					/* use incremental jump */
					printf("\033[%dB", i);
			} else if ((i = term_y - y) == 1)
				fputs("\033[A", stdout);	/* move 1 */
			else
				printf("\033[%dA", i);
			term_y = y;
		} else {
			/* absolute jump */
			printf("\033[%d;%dH", (term_y = y) + 1,
				(term_x = x) + 1);
		}
	}
}

/* scroll screen: argument is count: + up, - down */
w_scroll(count)
	int count;
{
	register int i;
	struct w_line *p[W_MAX_V];	/* temp copy of pointer array */

	/* scrolling up */
	if (count > 0) {
		/* cursor to bottom of window */
		w_move(window_size-1, 0);
		for (i = 0; i < count; i++) {
			/* scroll terminal, blank out image line */
			putchar(LF);
			wlp[i]->n = 0;
		}
	} else {
		/* scroll down */
		w_move(0, 0);		/* cursor to top */
		for (i = 0; i > count; i--) {
			fputs("\033M", stdout), wlp[window_size-1+i]->n = 0;
		}
	}
	/* rearrange */
	for (i = 0; i < window_size; i++)
		p[i] = wlp[(window_size + i + count) % window_size];
	for (i = 0; i < window_size; i++)
		wlp[i] = p[i];
}

/*
 * clear line to left of curr_x
 * if some chars nonblank, does erase from start of line
 */
w_ebol()
{
	short i, j;

	for (j = i = 0; i < curr_x; i++)
		if (wlp[curr_y]->ch[i] != ' ')
			wlp[curr_y]->ch[i] = ' ', j++;
	if (j || redraw_sw) {
		w_move(curr_y, curr_x-1);
		vt(VT_EBOL);
	}
}

/*
 * routine to set a pointer to a given location (like set_pointer)
 * returns nonzero if a text buffer exists, otherwise 0
 */
w_setptr(loc, pp)
	register int loc;			/* location */
	register struct qp *pp;			/* address of pointer */
{
	register int i;

	if (buff.f) {
		for (i = loc / CELLSIZE, pp->p = buff.f; i > 0; i--)
			pp->p = pp->p->f;
		pp->c = loc % CELLSIZE;
		pp->dot = loc;
	}
	return( (int) buff.f);
}

/*
 * routine to move N lines (back, forward, or 0)
 * w_lines(n, &source, &dest) where n is the line count, source
 * points to a qp at the current pointer, dest, if nonzero,
 * it points to a qp where the result is to go.
 * routine returns actual number of display lines
 */
struct qp w_lines_p;	/* to compute # of display lines in -N lines */

w_lines(n, ps, pd)
	int n;				/* number of lines */
	register struct qp *ps, *pd;	/* source, destination qp's */
{
	register struct buffcell *tp;	/* local copy of the qp */
	register int tc, tdt, tn;
	int tcnt, tl;			/* chars/line and display line count */
	char tch;

	tdt = ps->dot;
	tp = ps->p;
	tc = ps->c;

	/* argument is positive */
	if (n > 0) {
		/* forward over N line separators */
		for (tcnt = tl = tn = 0; (tn < n) && (tdt < z); tdt++) {
			if (spec_chars[ tch = tp->ch[tc] ] & A_L) {
				/* count separators */
				++tl, ++tn;
			} else if (!(et_val & ET_TRUNC)) {
				/* if text lines can overflow screen lines */

				/* if character is a control char */
				if (!(tch & 0140)) {
					/* CR resets count */
					if (tch == CR) {
						if (tcnt > WN_width) ++tl;
						tcnt = 0;
					} else if (tch == TAB)
						/* tab to next tab stop */
						tcnt = (tcnt | tabmask) +1;
					else
						/* ESC takes one space */
						if (tch == ESC)
							++tcnt;
					/* normal control chars take 2 spaces */
					else
						tcnt += 2;
				} else
					/* not a ctrl char: takes one space */
					++tcnt;

				/* if overflow, one more line */
				if (tcnt > WN_width) {
					++tl;
					tcnt = 2;
				}
			}

			/* next character position */
			if (++tc > CELLSIZE-1) {
				tp = tp->f;
				tc = 0;
			}
		}
		/*
		 * if counting display lines and there's more of them,
		 * return that
		 */
		if (tl > tn)
			tn = tl;
	} else {
		/* argument is zero or negative */

		/* back up over (n+1) line feeds */
		for (tn = 0; (tn >= n) && (tdt > 0); ) {
			--tdt;
			if (--tc < 0) {
				tp = tp->b;
				tc = CELLSIZE -1;
			}
			if (spec_chars[tp->ch[tc]] & A_L)
				--tn;
		}

		/* if stopped on a line sep, fwd over it */
		if (tn < n) {
			++tn;
			++tdt;
			if (++tc > CELLSIZE-1) {
				tp = tp->f;
				tc = 0;
			}
		}

		/* if text line can overflow display line */
		if (!(et_val & ET_TRUNC) && (n != 0)) {
			/* then count the number of display */
			w_lines_p.dot = tdt;
			w_lines_p.p = tp;
			w_lines_p.c = tc;

			/* lines in the N text lines we just backed up over */
			tn = -w_lines(-n, &w_lines_p, 0);
		}
	}

	if (pd) {
		/* if an "after" pointer given, update it */
		pd->dot = tdt;
		pd->p = tp;
		pd->c = tc;
	}
	return(tn);
}
