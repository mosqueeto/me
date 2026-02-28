#include <stdio.h>
#include "ed.h"

int mouse_selecting = 0;   /* 1 while a mouse-defined region is highlighted */

static int   dragging = 0; /* 1 while the left button is physically held   */
static LINE *press_lp = NULL;
static int   press_o  = 0;

static int
mouse_setpos(int col, int row)
{
    LINE *lp;
    int   i, vcol, ts;

    /* advance 'row' lines from top of window */
    lp = curwp->topp;
    for (i = 0; i < row; i++) {
        if (lforw(lp) == curbp->lines) break;
        lp = lforw(lp);
    }
    curwp->dotp = lp;

    /* walk chars counting visual columns to find byte offset for 'col' */
    ts = tabsize > 0 ? tabsize : 8;
    vcol = 0;
    for (i = 0; i < llength(lp); i++) {
        if (vcol >= col) break;
        if (lgetc(lp, i) == '\t')
            vcol = (vcol / ts + 1) * ts;
        else
            vcol++;
    }
    curwp->doto = i;
    curwp->flag |= WFMOVE;
    return TRUE;
}

int
mouse_event(int f, int n)
{
    int b, col, row;

    b   = (*term.getchar)() - 32;   /* button byte                    */
    col = (*term.getchar)() - 33;   /* column, 0-based                */
    row = (*term.getchar)() - 33;   /* row, 0-based                   */

    if (col < 0) col = 0;
    if (row < 0) row = 0;

    /* motion event: button held and mouse moved */
    if (b & 0x20) {
        if (dragging) {
            if (!mouse_selecting) {
                /* first motion after press: set mark at the press position */
                curwp->markp = press_lp;
                curwp->marko = press_o;
                mouse_selecting = 1;
            }
            mouse_setpos(col, row);
        }
        return TRUE;
    }

    switch (b & 3) {
    case 0:  /* left press: move cursor, arm for possible drag */
        mouse_setpos(col, row);
        press_lp = curwp->dotp;
        press_o  = curwp->doto;
        mouse_selecting = 0;   /* clear any existing highlight */
        dragging = 1;
        return TRUE;
    case 3:  /* release: drop the button; if we dragged, region persists */
        dragging = 0;
        return TRUE;
    default: /* middle or right: no mouse semantics */
        dragging = 0;
        return TRUE;
    }
}
