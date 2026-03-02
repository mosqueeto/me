#include <stdio.h>
#include "ed.h"

extern int yank(int, int);
extern int killregion(int, int);
extern int backline(int, int);
extern int forwline(int, int);

int mouse_selecting = 0;   /* 1 while a mouse-defined region is highlighted */
int mouse_enabled   = 1;   /* 1 while terminal mouse reporting is active     */

static int   dragging = 0; /* 1 while the left button is physically held   */
static LINE *press_lp = NULL;
static int   press_o  = 0;

static int
mouse_setpos(int col, int row)
{
    LINE *lp;
    int   i, vcol, ts;

    /* navigate from the current cursor (at terminal row currow) to the
       clicked terminal row -- this is always consistent with what update()
       just placed on screen, even when topp/topo have an unusual offset */
    lp = curwp->dotp;
    i  = row - currow;
    if (i > 0) {
        while (i-- > 0) {
            if (lforw(lp) == curbp->lines) break;
            lp = lforw(lp);
        }
    } else if (i < 0) {
        while (i++ < 0) {
            if (lback(lp) == curbp->lines) break;
            lp = lback(lp);
        }
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

    /* scroll wheel: up=64, down=65 */
    if (b == 64) { return backline(f, n); }
    if (b == 65) { return forwline(f, n); }

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
    case 1:  /* middle press: yank from kill buffer at current cursor */
        dragging = 0;
        return yank(f, n);
    case 2:  /* right press: kill region (cut to kill buffer) */
        dragging = 0;
        mouse_selecting = 0;
        return killregion(f, n);
    case 3:  /* release: drop the button; if we dragged, region persists */
        dragging = 0;
        return TRUE;
    default:
        dragging = 0;
        return TRUE;
    }
}

int
mouse_toggle(int f, int n)
{
    (void)defaultargs(f, n);
    if (mouse_enabled) {
        fputs("\033[?1002l", stdout);   /* disable mouse reporting */
        fflush(stdout);
        mouse_selecting = 0;
        dragging        = 0;
        mouse_enabled   = 0;
        mlwrite("[Mouse off]");
    } else {
        fputs("\033[?1002h", stdout);   /* enable mouse reporting */
        fflush(stdout);
        mouse_enabled = 1;
        mlwrite("[Mouse on]");
    }
    return TRUE;
}
