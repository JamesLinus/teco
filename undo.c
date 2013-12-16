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

struct undo *undo, *undo_head;

/* Transaction grouping of undo sets */
static unsigned int grpid;

/*
 * alloc_undo()
 *	Allocate a new "struct undo" entry
 */
struct undo *
get_undo(int op)
{
    struct undo *u = (struct undo *)get_dcell();

    bzero(u, sizeof(struct undo));
    u->op = op;
    u->dot = dot;
    u->grpid = grpid;
    return(u);
}

/*
 * add_undo()
 *	Put a "struct undo" entry onto the undo/redo chain
 */
void
add_undo(struct undo *u)
{
    struct undo *t;

    ASSERT(u->f == NULL);
    ASSERT(u->b == NULL);

    /* First undo entry in list */
    if (undo_head == NULL) {
	ASSERT(undo == NULL);
	undo_head = undo = u;
	return;
    }
    ASSERT(undo != NULL);

    /* Trim off undo history older than NUNDO */
    for (t = undo_head; (grpid - t->grpid) > NUNDO; t = t->f) {
	;
    }
    if ((t != NULL) && (t != undo_head)) {
	t->b->f = NULL;
	t->b = NULL;
	free_dcell((struct qp *)undo_head);
	undo_head = t;
    }

    /* We've undone, and now are going forward with new changes */
    if (undo->f) {
	free_dcell((struct qp *)undo->f);
	undo->f = NULL;
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
}
