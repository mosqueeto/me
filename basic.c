/*
 * The routines in this file
 * move the cursor around on the screen.
 * They compute a new value for the cursor, then
 * adjust ".". The display code always updates the
 * cursor location, so only moves between lines,
 * or functions that adjust the top line in the window
 * and invalidate the framing, are hard.
 */
#include    <stdio.h>
#include    "ed.h"

/*
 * Move the cursor to the
 * beginning of the current line.
 * Trivial.
 */
int gotobol(int f, int n)
{
    (void)defaultargs(f,n);
    curwp->doto  = 0;
    return (TRUE);
}

/*
 * Move the cursor backwards by
 * "n" characters. If "n" is less than
 * zero call "forwchar" to actually do the
 * move. Otherwise compute the new cursor
 * location. Error if you try and move
 * out of the buffer.
 */
int backchar(int f, int n)
{
    register LINE   *lp;

    (void) defaultargs(f,n);
    if (n < 0) return (forwchar(f, -n));
    while (n--) {
        if (curwp->doto == 0) {
            if ((lp=lback(curwp->dotp)) == curbp->lines)  return (FALSE);
            curwp->dotp = lp;
            curwp->doto = llength(lp)-1;
            curwp->doto++;
        } 
        else {
            curwp->doto--;
        }
    }
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*

Move the cursor to the end of the current line. 

*/
int gotoeol(int f, int n)
{
    (void)defaultargs(f,n);
    curwp->doto = llength(curwp->dotp);
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*

Move the cursor forwwards by "n" characters.  If "n" is less than zero call
"backchar" to actually do the move.  Otherwise compute the new cursor
location, and move ".".  Error if you try and move off the end of the buffer. 
Set the flag if the line pointer for dot changes. 

*/
int forwchar(int f, int n)
{
    int l;
    (void)defaultargs(f,n);
    if( n < 0 ) return (backchar(f, -n));
    while( n-- ){
        l = llength(curwp->dotp) - 1;
        l++;
        if( curwp->doto >= l  ){
            if( curwp->dotp->fp == curbp->lines ) return( FALSE );
            curwp->dotp  = lforw(curwp->dotp);
            curwp->doto  = 0;
            curwp->flag |= WFMOVE;
        } else
            curwp->doto++;
    }
    curwp->flag |= WFMOVE;
    return( TRUE );
}

/*
 * Goto the beginning of the buffer.
 * Massive adjustment of dot. This is considered
 * to be hard motion; it really isn't if the original
 * value of dot is the same as the new value of dot.
 * Normally bound to "M-<".
 */
int gotobob(int f, int n)
{
    (void)defaultargs(f,n);
    curwp->dotp  = lforw(curbp->lines);
    curwp->doto  = 0;
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*
 * Move to the end of the buffer.
 * Dot is always put at the end of the
 * file (ZJ). The standard screen code does
 * most of the hard parts of update. Bound to 
 * "M->".
 */
int gotoeob(int f, int n)
{
    (void)defaultargs(f,n);
    curwp->dotp  = lback(curbp->lines);
    curwp->doto  = llength(curwp->dotp);
    curwp->flag |= WFMOVE;
    return (TRUE);
}


/*
 * Goto line.
 * get a line number from the user, and scan from
 * the beginning of the current buffer until the 
 * appropriate line number is found.
 * Then set "." to be at the beginning of the line.
 * If we get to the end of the buffer, just leave the
 * pointer at the end.
 */
int gotoline(int f, int n)
{
    register LINE *clp;
    register int  linenum;
    register int  i;
    int  s;
    char numbuf[16];

    (void)defaultargs(f,n);
    if( (s= mlreply("Line: ",numbuf,16)) != TRUE )  return( s );
    if( (i = atoi(numbuf)) < 0 ) return FALSE;
    linenum = i;
    i = 1;
    clp = lforw(curbp->lines);
    while (clp != curbp->lines) {
        if( i == linenum ) break;
        ++i;
        clp = lforw(clp);
    }
    curwp->dotp  = clp;
    curwp->doto  = 0;
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*
 * Move forward by full lines.
 * If the number of lines to move is less
 * than zero, call the backward line function to
 * actually do it. The last command controls how
 * the goal column is set. Bound to "C-N". No
 * errors are possible.
 */
int forwline(int f, int n)
{
    register LINE   *dlp;

    (void)defaultargs(f,n);
    if (n < 0)  return (backline(f, -n));
    if ((lastflag&CFCPCN) == 0) // Reset goal if last
        curgoal = curcol;       // not C-P or C-N 
    thisflag |= CFCPCN;
    dlp = curwp->dotp;
    while (n-- && (dlp->fp != curbp->lines) ) {
        dlp = lforw(dlp);
    }
    curwp->dotp  = dlp;
    curwp->doto  = getgoal(dlp);
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*
 * This function is like "forwline", but
 * goes backwards. The scheme is exactly the same.
 * Check for arguments that are less than zero and
 * call your alternate. Figure out the new line and
 * call "movedot" to perform the motion. No errors
 * are possible. Bound to "C-P".
 */
int backline(int f, int n)
{
    register LINE   *dlp;

    (void)defaultargs(f,n);
    if (n < 0)  return (forwline(f, -n));
    if ((lastflag&CFCPCN) == 0) // Reset goal if the
        curgoal = curcol;       // last isn't C-P, C-N
    thisflag |= CFCPCN;
    dlp = curwp->dotp;
    while (n-- && lback(dlp)!=curbp->lines) {
        dlp = lback(dlp);
    }
    curwp->dotp  = dlp;
    curwp->doto  = getgoal(dlp);
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*
 * This routine, given a pointer to
 * a LINE, and the current cursor goal
 * column, return the best choice for the
 * offset. The offset is returned.
 * Used by "C-N" and "C-P".
 */
int getgoal(LINE *dlp)
{
    register int    c;
    register int    col;
    register int    newcol;
    register int    dbo;

    col = 0;
    dbo = 0;
    while (dbo != llength(dlp)) {
        c = lgetc(dlp, dbo);
        newcol = col;
        if (c == '\t') {
			newcol += (tabsize - newcol%tabsize - 1);
		}
        else if (c<0x20 || c==0x7F) ++newcol;
        ++newcol;
        if (newcol > curgoal) break;
        col = newcol;
        ++dbo;
    }
    return (dbo);
}

/*
 * Scroll forward by a specified number
 * of lines, or by a full page if no argument.
 * Bound to "C-V". The "2" in the arithmetic on
 * the window size is the overlap; this value is
 * the default overlap value in ITS EMACS.
 * Because this zaps the top line in the display
 * window, we have to do a hard update.
 */
int forwpage(int f, int n)
{
    register LINE   *lp;

    if (f == FALSE) {
        n = curwp->ntrows - 2;  // Default scroll.
        if (n <= 0)             // Forget the overlap
            n = 1;              // if tiny window.
    } else if (n < 0) return (backpage(f, -n));
#if CVMVAS
    else                        // Convert from pages
        n *= curwp->ntrows;     // to lines.
#endif
    lp = curwp->topp;
    while (n-- && lp!=curbp->lines) {
        lp = lforw(lp);
    }
    curwp->topp = lp;
    curwp->dotp  = lp;
    curwp->doto  = 0;
    showcpos(f,1);
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*
 * This command is like "forwpage",
 * but it goes backwards. The "2", like above,
 * is the overlap between the two windows. The
 * value is from the ITS EMACS manual. Bound
 * to "M-V". We do a hard update for exactly
 * the same reason.
 */
int backpage(int f, int n)
{
    register LINE   *lp;

    if (f == FALSE) {
        n = curwp->ntrows - 2;  // Default scroll.
        if (n <= 0)             // Don't blow up if the
            n = 1;              // window is tiny.
    } else if (n < 0)
        return (forwpage(f, -n));
#if CVMVAS
    else                        // Convert from pages
        n *= curwp->ntrows;     // to lines.
#endif
    lp = curwp->topp;
    while (n-- && lback(lp)!=curbp->lines)
        lp = lback(lp);
    curwp->topp = lp;
    curwp->dotp  = lp;
    curwp->doto  = 0;
    showcpos(f,1);
    curwp->flag |= WFMOVE;
    return (TRUE);
}

/*
 * Set the mark in the current window
 * to the value of "." in the window. No errors
 * are possible. Bound to "M-.".
 */
int setmark(int f, int n)
{
    (void)defaultargs(f,n);
    curwp->markp = curwp->dotp;
    curwp->marko = curwp->doto;
    mlwrite("[Mark set]");
    return (TRUE);
}

/*
 * Swap the values of "." and "mark" in
 * the current window. This is pretty easy, bacause
 * all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is
 * "no mark". Bound to "C-X C-X".
 */
int swapmark(int f, int n)
{
    register LINE   *odotp;
    register int    odoto;

    (void)defaultargs(f,n);
    if (curwp->markp == NULL) {
        mlwrite("No mark in this window");
        return (FALSE);
    }
    odotp = curwp->dotp;
    odoto = curwp->doto;
    curwp->dotp  = curwp->markp;
    curwp->doto  = curwp->marko;
    curwp->markp = odotp;
    curwp->marko = odoto;
    curwp->flag |= WFMOVE;
    return (TRUE);
}

int isblankline(LINE *lp)
{
    int i;
    char c;
    if( llength(lp) == 0 ) return TRUE;
    for( i=0;i<llength(lp);i++ ) {
        c = lgetc(lp,i);
        if( c != ' ' && c != TAB ) return FALSE;
    }
    return TRUE;
}

int firstnonblank(LINE *lp)
{
    int i;
    char c;
    if( llength(lp) == 0 ) return -1;
    for( i=0;i<llength(lp);i++ ) {
        c = lgetc(lp,i);
        if( c != ' ' && c != TAB ) return i;
    }
    return -1;
}

// should have a configurable condition for this...
int is_para_boundary(LINE *lp)
{
    int i;
    char c,c1;

    if( (i = firstnonblank(lp)) < 0 ) return TRUE;

    c  = lgetc(lp,i);
    c1 = lgetc(lp,i+1);

    if( c == '>' ) return TRUE; // email quote
    if( c == '#' ) return TRUE; // perl comment
    if( c == '=' ) return TRUE;     // perldoc delimiter
    if( (c == '/') && (c1 == '*') ) return TRUE;  // C comment
    if( (c == '*') && (c1 == '/') ) return TRUE;  // End comment
    if( isdigit(c) && (c1 == ')') ) return TRUE;  // numbered bullet
    if( (c == '.' || c == 'o') && 
        (c1 == ' ') ) return TRUE;  /* more bullet forms */
    if( (c == '-' ) ) {
        if(  isspace(c1) ) {
            return TRUE;  /* bullet */
        } 
        else if( c1 == '-' ) {
            if( (llength(lp) == 3) && 
                (i == 0) && 
                (lgetc(lp,2) == ' ') )
            {
                return TRUE; /* email .sig marker */    
            }
        }
    }
    if( (c == 'o') && isspace(c1) ) return TRUE;  /* bullet */

    return FALSE;
}
    

/* 

go back to the beginning of the current paragraph.  here we look for a blank
line delimit the beginning of a paragraph

 */
int gotobop(int f, int n)
{
    int suc; // success of last backchar

    (void)defaultargs(f,n);

    if (n < 0) return(gotoeop(f, -n));

    while (n-- > 0) {

        // first scan back until we are in a word
        suc = backchar(FALSE, 1);
        while (!inword() && suc) {
            suc = backchar(FALSE, 1);
        }
        curwp->doto = 0;  // go to the BOL and scan back
        while( lback(curwp->dotp) != curbp->lines ) {
            if( is_para_boundary( curwp->dotp ) ) break;
            curwp->dotp = lback(curwp->dotp);
        }

        // and then forward until we are in a word
/*
       if( curwp->dotp != (lforw(curbp->lines)) ) {
            curwp->dotp = lforw(curwp->dotp);
        }
*/
        suc = 1;
        while (suc && !inword()) {
            suc = forwchar(FALSE, 1);
        }
    }
    curwp->flag |= WFMOVE;        // force screen update
    return(TRUE);
}

/* 

go forword to the end of the current paragraph. here we look for a blank line
to delimit the beginning of a paragraph

 */
int gotoeop(int f, int n)
{
    int suc;  // success of last backchar

    (void)defaultargs(f,n);

    if (n < 0)  return(gotobop(f, -n)); // the other way...

    while (n-- > 0) {
        suc = forwchar(FALSE, 1); // scan forward until we are in a word
        while (!inword() && suc) {
            suc = forwchar(FALSE, 1);
        }
        curwp->doto = 0;    // go to the BOL
        if( suc ) curwp->dotp = lforw(curwp->dotp);
        // scan forword until we hit a blank line
        while( curwp->dotp != curbp->lines ) {
            if( is_para_boundary( curwp->dotp ) ) break;
            curwp->dotp = lforw(curwp->dotp);
        }
        // then backward to the last char in the paragraph
        curwp->dotp = lback(curwp->dotp);
        curwp->doto = llength(curwp->dotp);  // and to the EOL
    }
    curwp->flag |= WFMOVE;  // force screen update
    return(TRUE);
}
