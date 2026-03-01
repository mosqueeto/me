/*
 * Window management.
 * Some of the functions are internal,
 * and some are attached to keys that the
 * user actually types.
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	"ed.h"

/*
 * Reposition dot in the current
 * window to line "n". If the argument is
 * positive, it is that line. If it is negative it
 * is that line from the bottom. If it is 0 the window
 * is centered (this is what the standard redisplay code
 * does). With no argument it defaults to 1. Bound to
 * M-!. Because of the default, it works like in
 * Gosling.
 */
int reposition(int f, int n)
{
	(void)defaultargs(f,n);
	curwp->force = n;
//	curwp->flag |= WFFORCE;
	return (TRUE);
}

/*
 * Refresh the screen. With no
 * argument, it just does the refresh. With an
 * argument it recenters "." in the current
 * window. Bound to "C-L".
 */
int refresh(int f, int n)
{
	(void)defaultargs(f,n);
	if (f == FALSE)
		sgarbf = TRUE;
	else {
		curwp->force = 0;		/* Center dot.		*/
//		curwp->flag |= WFFORCE;
	}
	return (TRUE);
}

/*
 * The command make the next
 * window (next => down the screen)
 * the current window. There are no real
 * errors, although the command does
 * nothing if there is only 1 window on
 * the screen. Bound to "C-X C-N".
 */
int nextwind(int f, int n)
{
	register WINDOW	*wp;

	(void)defaultargs(f,n);
	if ((wp=curwp->wndp) == NULL)
		wp = wheadp;
	curwp = wp;
	curbp = wp->bufp;
	return (TRUE);
}

/*
 * This command makes the previous
 * window (previous => up the screen) the
 * current window. There arn't any errors,
 * although the command does not do a lot
 * if there is 1 window.
 */
int prevwind(int f, int n)
{
	register WINDOW	*wp1;
	register WINDOW	*wp2;

	(void)defaultargs(f,n);
	wp1 = wheadp;
	wp2 = curwp;
	if (wp1 == wp2)
		wp2 = NULL;
	while (wp1->wndp != wp2)
		wp1 = wp1->wndp;
	curwp = wp1;
	curbp = wp1->bufp;
	return (TRUE);
}

/*
 * This command moves the current
 * window down by "arg" lines. Recompute
 * the top line in the window. The move up and
 * move down code is almost completely the same;
 * most of the work has to do with reframing the
 * window, and picking a new dot. We share the
 * code by having "move down" just be an interface
 * to "move up". Magic. Bound to "C-X C-N".
 */
int mvdnwind(int f, int n)
{
	return (mvupwind(f, -n));
}

/*
 * Move the current window up by "arg"
 * lines. Recompute the new top line of the window.
 * Look to see if "." is still on the screen. If it is,
 * you win. If it isn't, then move "." to center it
 * in the new framing of the window (this command does
 * not really move "."; it moves the frame). Bound
 * to "C-X C-P".
 */
int mvupwind(int f, int n)
{
	register LINE	*lp;
	register int	i;

	(void)defaultargs(f,n);
	lp = curwp->topp;
	if (n < 0) {
		while (n++ && lp!=curbp->lines)
			lp = lforw(lp);
	} else {
		while (n-- && lback(lp)!=curbp->lines)
			lp = lback(lp);
	}
	curwp->topp = lp;
	for (i=0; i<curwp->ntrows; ++i) {
		if (lp == curwp->dotp)
			return (TRUE);
		if (lp == curbp->lines)
			break;
		lp = lforw(lp);
	}
	lp = curwp->topp;
	i  = curwp->ntrows/2;
	while (i-- && lp!=curbp->lines)
		lp = lforw(lp);
	curwp->dotp  = lp;
	curwp->doto  = 0;
	return (TRUE);
}

/*
 * This command makes the current
 * window the only window on the screen.
 * Bound to "C-X 1". Try to set the framing
 * so that "." does not have to move on
 * the display. Some care has to be taken
 * to keep the values of dot and mark
 * in the buffer structures right if the
 * distruction of a window makes a buffer
 * become undisplayed.
 */
int onlywind(int f, int n)
{
	register WINDOW	*wp;
	register LINE	*lp;
	register int	i;

	(void)defaultargs(f,n);
	while (wheadp != curwp) {
		wp = wheadp;
		wheadp = wp->wndp;
		if (--wp->bufp->nwnd == 0) {
			wp->bufp->dotp  = wp->dotp;
			wp->bufp->doto  = wp->doto;
			wp->bufp->markp = wp->markp;
			wp->bufp->marko = wp->marko;
		}
		free((char *) wp);
	}
	while (curwp->wndp != NULL) {
		wp = curwp->wndp;
		curwp->wndp = wp->wndp;
		if (--wp->bufp->nwnd == 0) {
			wp->bufp->dotp  = wp->dotp;
			wp->bufp->doto  = wp->doto;
			wp->bufp->markp = wp->markp;
			wp->bufp->marko = wp->marko;
		}
		free((char *) wp);
	}
	lp = curwp->topp;
	i  = curwp->toprow;
	while (i!=0 && lback(lp)!=curbp->lines) {
		--i;
		lp = lback(lp);
	}
	curwp->toprow = 0;
	curwp->ntrows = term.nrow-1;
	curwp->topp  = lp;
	curwp->flag  |= WFMODE;
	return (TRUE);
}

/*
 * Split the current window. A window
 * smaller than 3 lines cannot be split.
 * The only other error that is possible is
 * a "malloc" failure allocating the structure
 * for the new window. Bound to "C-X 2".
 */
int splitwind(int f, int n)
{
	register WINDOW	*wp;
	register LINE	*lp;
	register int	ntru;
	register int	ntrl;
	register int	ntrd;
	register WINDOW	*wp1;
	register WINDOW	*wp2;

	(void)defaultargs(f,n);
	if (curwp->ntrows < 3) {
		mlwrite("Cannot split a %d line window", curwp->ntrows);
		return (FALSE);
	}
	if ((wp = (WINDOW *) malloc(sizeof(WINDOW))) == NULL) {
		mlwrite("Cannot allocate WINDOW block");
		return (FALSE);
	}
	++curbp->nwnd;			/* Displayed twice.	*/
	wp->bufp  = curbp;
	wp->dotp  = curwp->dotp;
	wp->doto  = curwp->doto;
	wp->markp = curwp->markp;
	wp->marko = curwp->marko;
	wp->flag  = 0;
	wp->force = 0;
	ntru = (curwp->ntrows-1) / 2;		/* Upper size		*/
	ntrl = (curwp->ntrows-1) - ntru;	/* Lower size		*/
	lp = curwp->topp;
	ntrd = 0;
	while (lp != curwp->dotp) {
		++ntrd;
		lp = lforw(lp);
	}
	lp = curwp->topp;
	if (ntrd <= ntru) {			/* Old is upper window.	*/
		if (ntrd == ntru)		/* Hit mode line.	*/
			lp = lforw(lp);
		curwp->ntrows = ntru;
		wp->wndp = curwp->wndp;
		curwp->wndp = wp;
		wp->toprow = curwp->toprow+ntru+1;
		wp->ntrows = ntrl;
	} else {				/* Old is lower window	*/
		wp1 = NULL;
		wp2 = wheadp;
		while (wp2 != curwp) {
			wp1 = wp2;
			wp2 = wp2->wndp;
		}
		if (wp1 == NULL)
			wheadp = wp;
		else
			wp1->wndp = wp;
		wp->wndp   = curwp;
		wp->toprow = curwp->toprow;
		wp->ntrows = ntru;
		++ntru;				/* Mode line.		*/
		curwp->toprow += ntru;
		curwp->ntrows  = ntrl;
		while (ntru--)
			lp = lforw(lp);
	}
	curwp->topp = lp;			/* Adjust the top lines	*/
	wp->topp = lp;			/* if necessary.	*/
	curwp->flag |= WFMODE;
	wp->flag |= WFMODE;
	return (TRUE);
}

/*
 * Enlarge the current window.
 * Find the window that loses space. Make
 * sure it is big enough. If so, hack the window
 * descriptions, and ask redisplay to do all the
 * hard work. You don't just set "force reframe"
 * because dot would move. Bound to "C-X Z".
 */
int enlargewind(int f, int n)
{
	register WINDOW	*adjwp;
	register LINE	*lp;
	register int	i;

	(void)defaultargs(f,n);
	if (n < 0)
		return (shrinkwind(f, -n));
	if (wheadp->wndp == NULL) {
		mlwrite("Only one window");
		return (FALSE);
	}
	if ((adjwp=curwp->wndp) == NULL) {
		adjwp = wheadp;
		while (adjwp->wndp != curwp)
			adjwp = adjwp->wndp;
	}
	if (adjwp->ntrows <= n) {
		mlwrite("Impossible change");
		return (FALSE);
	}
	if (curwp->wndp == adjwp) {		/* Shrink below.	*/
		lp = adjwp->topp;
		for (i=0; i<n && lp!=adjwp->bufp->lines; ++i)
			lp = lforw(lp);
		adjwp->topp  = lp;
		adjwp->toprow += n;
	} else {				/* Shrink above.	*/
		lp = curwp->topp;
		for (i=0; i<n && lback(lp)!=curbp->lines; ++i)
			lp = lback(lp);
		curwp->topp  = lp;
		curwp->toprow -= n;
	}
	curwp->ntrows += n;
	adjwp->ntrows -= n;
	curwp->flag |= WFMODE;
	adjwp->flag |= WFMODE;
	return (TRUE);
}

/*
 * Shrink the current window.
 * Find the window that gains space. Hack at
 * the window descriptions. Ask the redisplay to
 * do all the hard work. Bound to "C-X C-Z".
 */
int shrinkwind(int f, int n)
{
	register WINDOW	*adjwp;
	register LINE	*lp;
	register int	i;

	(void)defaultargs(f,n);
	if (n < 0)
		return (enlargewind(f, -n));
	if (wheadp->wndp == NULL) {
		mlwrite("Only one window");
		return (FALSE);
	}
	if ((adjwp=curwp->wndp) == NULL) {
		adjwp = wheadp;
		while (adjwp->wndp != curwp)
			adjwp = adjwp->wndp;
	}
	if (curwp->ntrows <= n) {
		mlwrite("Impossible change");
		return (FALSE);
	}
	if (curwp->wndp == adjwp) {		/* Grow below.		*/
		lp = adjwp->topp;
		for (i=0; i<n && lback(lp)!=adjwp->bufp->lines; ++i)
			lp = lback(lp);
		adjwp->topp  = lp;
		adjwp->toprow -= n;
	} else {				/* Grow above.		*/
		lp = curwp->topp;
		for (i=0; i<n && lp!=curbp->lines; ++i)
			lp = lforw(lp);
		curwp->topp  = lp;
		curwp->toprow += n;
	}
	curwp->ntrows -= n;
	adjwp->ntrows += n;
	curwp->flag |= WFMODE;
	adjwp->flag |= WFMODE;
	return (TRUE);
}

/*
 * Pick a window for a pop-up.
 * Split the screen if there is only
 * one window. Pick the uppermost window that
 * isn't the current window. An LRU algorithm
 * might be better. Return a pointer, or
 * NULL on error.
 */
WINDOW	*
wpopup()
{
	register WINDOW	*wp;

	if( wheadp->wndp == NULL		/* Only 1 window	*/
	&& splitwind(FALSE, 0) == FALSE ) 	/* and it won't split	*/
		return (NULL);
	wp = wheadp;				/* Find window to use	*/
	while( wp!=NULL && wp==curwp )
		wp = wp->wndp;
	return (wp);
}





