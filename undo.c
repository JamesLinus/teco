/*
 * undo.c
 *	Implement undo/redo
 *
 * This is more along the lines of vim's undo/redo; changes to the
 *  edit buffer are managed, but not changes to Q registers.  The
 *  changes are grouped by cycles through prompt/execute, so the
 *  granularity of undo/redo is WRT everything that happens between
 *  one prompt and the next.
 * The changes are kept in a doubly linked list, so changes can be
 *  undone, but also re-done (until a new change starts a new list
 *  of changes).
 *
 * "undo_head" always points at the oldest undo entry recorded, and
 *  "undo" points at the newest whose effects are still
 *  present in the edit buffer.  When undo->f is not NULL, the
 *  changes described beyond this point are not present in the edit
 *  buffer, because they have been undone.
 */
#include <strings.h>
#include "defs.h"

/* Current undo position, and head of list of all recorded undo records */
static struct undo *undo, *undo_head;

/* Transaction grouping of undo sets */
static unsigned int grpid;

/*
 * alloc_undo()
 *	Allocate a new "struct undo" entry
 */
static struct undo *
get_undo(int op)
{
    struct undo *u = (struct undo *)get_dcell();

    bzero(u, sizeof(struct undo));
    u->op = op;
    u->grpid = grpid;
    return(u);
}

/*
 * free_undo()
 *	Release undo header chain, along with any text storage
 */
static void
free_undo(struct undo *u)
{
    struct undo *u2;

    /* Scan chain, releasing buffcell storage */
    for (u2 = u; u2; u2 = u2->f) {
	if (u2->p) {
	    free_blist(u2->p);
	    u2->p = NULL;
	}
    }

    /* Now release the struct undo headers */
    free_dcell((struct qp *)u);
}

/*
 * add_undo()
 *	Put a "struct undo" entry onto the undo/redo chain
 */
static void
add_undo(struct undo *u)
{
    ASSERT(u->f == NULL);
    ASSERT(u->b == NULL);

    /* First undo entry in list */
    if (undo_head == NULL) {
	ASSERT(undo == NULL);
	undo_head = undo = u;
	return;
    }
    ASSERT(undo != NULL);

    /* We've undone, and now are going forward with new changes */
    if (undo->f) {
	free_undo(undo->f);
    }

    /* Add this entry to the tail */
    undo->f = u;
    u->b = undo;
    undo = u;
}

/*
 * rev_undo()
 *	Bump revision number of undo transactions
 *
 * Undo/redo is done at the granularity of transactions, by default
 *  the changes between one teco prompt and the next.  teco macros
 *  can also bump this value, useful for editor macros.
 */
void
rev_undo(void)
{
    struct undo *t;

    /* No undo state, so no need */
    if (undo == NULL) {
	return;
    }

    /* Nothing has happened since the last rev, no need */
    if (undo->grpid != grpid) {
	return;
    }

    /* Ok, advance by one */
    grpid += 1;

    /* Trim off undo history older than NUNDO */
    for (t = undo_head; (grpid - t->grpid) > NUNDO; t = t->f) {
	/* Break off the search when we reach the current undo point */
	if (t == undo) {
	    break;
	}
    }
    if ((t != NULL) && (t != undo_head)) {
	t->b->f = NULL;
	t->b = NULL;
	free_undo(undo_head);
	undo_head = t;
    }

}

/*
 * undo_insert()
 *	Add undo record for insertion of this much text
 */
void
undo_insert(int pos, int nchars)
{
    struct undo *u;

    u = get_undo(UNDO_INS);
    u->dot = pos;
    u->count = nchars;
    add_undo(u);
}

/*
 * undo_del()
 *	Add undo record for deletion of this text
 */
void
undo_del(int pos, int nchars)
{
    struct undo *u;
    struct buffcell *bc;
    struct qp src, dest;

    /* Our undo record */
    u = get_undo(UNDO_INS);
    u->dot = pos;
    u->count = nchars;

    /* Make a snapshot of the deleted chars */
    set_pointer(pos, &src);
    bc = dest.p = get_bcell();
    dest.c = 0;
    movenchars(&src, &dest, nchars);

    /* Record the change */
    u->p = bc;
    add_undo(u);
}

/*
 * roll_back()
 *	Revert one generation of changes
 */
void
roll_back(void)
{
    int gid, uc;
    struct qp src, dest;
    struct buffcell *bc;

    /* Nothing to undo */
    if (undo == NULL) {
	return;
    }

    /*
     * Don't apply things already applied; this deals with the
     *  ambiguity of the head of the undo/undo_head list, where
     *  "undo" points to the first thing, and we don't know if it's
     *  been undone yet.
     */
    if (undo->flags & UNDOF_UNDONE) {
	return;
    }

    /* Roll back everything with this grpid */
    gid = undo->grpid;
    while (undo->grpid == gid) {

	/* Move to position of change */
	dot = undo->dot;
	if (dot < buff_mod) {
	    buff_mod = dot;
	}

	/* Put chars back into text buffer */
	uc = undo->count;
	if (undo->op == UNDO_DEL) {
	    cc.p = undo->p;
	    cc.c = 0;
	    insert1();
	    movenchars(&cc, &bb, uc);
	    insert2(uc, 0);

	/* Remove chars from text buffer */
	} else if (undo->op == UNDO_INS) {
	    /*
	     * Save this text first time we undo this insertion; this
	     *  is in case we are asked to "redo" the undo.
	     */
	    if (undo->p == NULL) {
		set_pointer(undo->dot, &src);
		bc = dest.p = get_bcell();
		dest.c = 0;
		movenchars(&src, &dest, uc);
		undo->p = bc;
	    }

	    /* Cribbed from delete1() */
	    set_pointer(dot, &dest);
	    set_pointer(dot + uc, &src);
	    movenchars(&src, &dest, z - (dot + uc));
	    free_blist(dest.p->f);
	    z -= uc;

	/* Shouldn't happen */
	} else {
	    ASSERT(0);
	}

	/* Ok, this effect is undone */
	undo->flags |= UNDOF_UNDONE;

	/* Continue backwards until head of chain */
	if (undo->b == NULL) {
	    break;
	}
	undo = undo->b;
    }
}

/*
 * roll_forward()
 *	Re-apply one generation of changes
 */
void
roll_forward(void)
{
    int gid, uc;
    struct qp src, dest;

    /* Nothing to redo */
    if (undo == NULL) {
	return;
    }

    /*
     * Is our current position one before the latest
     *  undone operation?
     */
    if ((undo->flags & UNDOF_UNDONE) == 0) {
	/*
	 * Nope, so nothing to do here
	 */
	if (undo->f == NULL) {
	    return;
	}
	if ((undo->f->flags & UNDOF_UNDONE) == 0) {
	    return;
	}

	/*
	 * Yes, so point at first of operations to be redone
	 */
	undo = undo->f;

    /*
     * If we're pointing directly at an un-done operation, then
     *  this should be the first in the chain.
     */
    } else {
	if (undo->b != NULL) {
	    ASSERT((undo->b->flags & UNDOF_UNDONE) == 0);
	}
    }

    /* Re-do everything with this grpid */
    gid = undo->grpid;
    while (undo->grpid == gid) {

	/* Move to position of change */
	dot = undo->dot;
	if (dot < buff_mod) {
	    buff_mod = dot;
	}

	/* Make the delete happen again */
	uc = undo->count;
	if (undo->op == UNDO_DEL) {

	    /* Cribbed from delete1() */
	    set_pointer(dot, &dest);
	    set_pointer(dot + uc, &src);
	    movenchars(&src, &dest, z - (dot + uc));
	    free_blist(dest.p->f);
	    z -= uc;

	/* Make the insertion happen again */
	} else if (undo->op == UNDO_INS) {
	    ASSERT(undo->p);
	    cc.p = undo->p;
	    cc.c = 0;
	    insert1();
	    movenchars(&cc, &bb, uc);
	    insert2(uc, 0);

	/* Shouldn't happen */
	} else {
	    ASSERT(0);
	}

	undo->flags &= ~(UNDOF_UNDONE);
	if (undo->f == NULL) {
	    break;
	}
	undo = undo->f;
    }
}
