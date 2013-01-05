/*
 * TECO for Ultrix   Copyright 1986 Matt Fichtenbaum
 * This program and its components belong to GenRad Inc, Concord MA 01742
 * They may be copied if this copyright notice is included

/* te_utils.c utility subroutines  10/28/85 */

#include "te_defs.h"

/*
 * routines to handle storage
 * get a buffcell
 * if there are no buffcells available, call malloc for more storage
 */
struct buffcell*
get_bcell()
{
	struct buffcell *p;
	int i;

	if (freebuff == NULL) {
		p = (struct buffcell *) malloc(BLOCKSIZE);
		if (!p)
			ERROR(E_MEM);
		else {
			/*
			 * Take the given address as storage for all
			 * cells in the new block, chain forward pointers
			 * together, make last one NULL
			 */
			freebuff =  p;
			for (i = 0;
				i < (BLOCKSIZE/sizeof(struct buffcell)) - 1;
				i++)
					(p+i)->f = p+i+1;
			(p+i)->f = NULL;
		}
	}

	p = freebuff;			/* cut out head of "free" list */
	freebuff = freebuff->f;
	p->f = p->b = NULL;
	return(p);
}


/* free a list of buffcells */
free_blist(p)
	struct buffcell *p;
{
	struct buffcell *t;

	if (p != NULL) {
		 /* find end of ret'd list */
		for (t = p; t -> f != NULL; t = t -> f)
			;

		/* put ret'd list at head of "free" list */
		t->f = freebuff;
		freebuff = p;
	}
}

/* free a list of buffcells to the "delayed free" list */
dly_free_blist(p)
	struct buffcell *p;
{
	struct buffcell *t;

	if (p != NULL) {
		/* find end of ret'd list */
		for (t = p; t -> f != NULL; t = t -> f)
			;

		/* put ret'd list at head of "free" list */
		t->f = dly_freebuff;
		dly_freebuff = p;
	}
}

/* get a cell */
/* if there are no cells available, get a buffcell and make more */
struct qp *
get_dcell()
{
	struct qp *t;
	int i;

	if (freedcell == NULL) {
		/* get a buffcell */
		t = freedcell = (struct qp *) get_bcell();

		for (i = 0;
			i < (sizeof(struct buffcell)/sizeof(struct qp)) - 1;
			i++) {


				/*
				 * Chain the forward pointers together,
				 * make last one's forward pointer NULL
				 */
				(t+i)->f = t+i+1;
				(t+i)->f = NULL;
		}
	}

	/* cut out head of "free" list */
	t = freedcell;
	freedcell = (struct qp *) freedcell->f;
	t->f =  NULL;
	return(t);
}

/* build a buffer:  called with address of a qh */
/* if no buffer there, get a cell and link it in */
make_buffer(p)
	struct qh *p;
{
	if (!(p->f)) {
		p->f = get_bcell();
		p->f->b = (struct buffcell *) p;
	}
}

/* routines to advance one character forward or backward */
/* argument is the address of a qp */
/* fwdc, backc return 1 if success, 0 if beyond extremes of buffer) */
/* fwdcx extends buffer if failure */
fwdc(arg)
	struct qp *arg;
{
	/* test char count for max */
	if ((*arg).c >= CELLSIZE-1) {
		/* last cell: fail */
		if ((*arg).p->f == NULL)
			return(0);


		/*
		 * Chain through list & reset char count
		 */
		(*arg).p = (*arg).p->f;
		(*arg).c = 0;
	} else
		/* otherwise just incr char count */
		++(*arg).c;
	++(*arg).dot;
	return(1);
}

fwdcx(arg)
	struct qp *arg;
{
	/* test char count for max */
	if ((*arg).c >= CELLSIZE-1) {

		/* last cell: extend */
		if ((*arg).p->f == NULL) {
			(*arg).p->f = get_bcell();
			(*arg).p->f->b = (*arg).p;
		}
		(*arg).p = (*arg).p->f;		/* chain through list */
		(*arg).c = 0;			/* and reset char count */
	} else
		/* otherwise just incr char count */
		++(*arg).c;
	++(*arg).dot;
	return(1);
}

backc(arg)
	struct qp *arg;
{
	/* test char count for min */
	if ((*arg).c <= 0) {
		/* first cell: fail */
		if ((*arg).p->b->b == NULL)
			return(0);

		/*
		 * Chain through list & reset char count
		 */
		(*arg).p = (*arg).p->b;
		(*arg).c = CELLSIZE-1;
	} else
		/* otherwise just decr char count */
		--(*arg).c;
	--(*arg).dot;
	return(1);
}

/* set up a pointer to a particular text buffer position */
/* first arg is position, 2nd is addr of pointer */
set_pointer(pos, ptr)
	int pos;
	struct qp *ptr;
{
	struct buffcell *t;
	int i;

	if (!buff.f) {
		/* if no text buffer, make one	*/
		buff.f = get_bcell();
		buff.f->b = (struct buffcell *) &buff;
	}
	for (i = pos/CELLSIZE, t = buff.f; (i > 0) && (t->f != NULL); i--)
		t = t->f;
	ptr->p = t;
	ptr->c = pos % CELLSIZE;
	ptr->dot = pos;
	ptr->z = z;
}

/*
 * routines to get next character from command buffer
 * getcmdc0, when reading beyond command string, pops
 * macro stack and continues.
 * getcmdc, in similar circumstances, reports an error
 * if pushcmdc() has returned any chars, read them first
 * routines type characters as read, if argument != 0
 */
char
getcmdc0(trace)
{
	/* if at end of this level, pop macro stack */
	while (cptr.dot >= cptr.z) {
		/* pop stack; if top level */
		if (--msp < &mstack[0]) {
			/* restore stack pointer */
			msp = &mstack[0];

			/* return an ESC (ignored) */
			cmdc = ESC;

			/* set to terminate execution */
			exitflag = 1;

			/* exit "while" and return */
			return(cmdc);
		}
	}

	/* get char */
	cmdc = cptr.p->ch[cptr.c++];

	/* increment character count */
	++cptr.dot;

	/* trace */
	if (trace)
		type_char(cmdc);

	/* and chain if need be */
	if (cptr.c > CELLSIZE-1) {
		cptr.p = cptr.p->f;
		cptr.c = 0;
	}
	return(cmdc);
}

char
getcmdc(trace)
{
	if (cptr.dot++ >= cptr.z) {
		ERROR((msp <= &mstack[0]) ? E_UTC : E_UTM);
	} else {
		cmdc = cptr.p->ch[cptr.c++];	/* get char */
		if (trace)
			type_char(cmdc);

		/* and chain if need be */
		if (cptr.c > CELLSIZE-1) {
			cptr.p = cptr.p->f;
			cptr.c = 0;
		}
	}
	return(cmdc);
}

/*
 * peek at next char in command string, return 1 if it is equal
 * (case independent) to argument
 */
peekcmdc(arg)
	char arg;
{
	return(
		((cptr.dot < cptr.z) &&
		(mapch_l[cptr.p->ch[cptr.c]] == mapch_l[arg])) ? 1 : 0
	);
}
