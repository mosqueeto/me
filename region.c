/*

The routines in this file deal with the region, that magic space between "."
and mark.  Some functions are commands.  Some functions are just for internal
use. 

 */
#include    <stdio.h>
#include    "ed.h"

/*

Kill the region.  Ask "getregion" to figure out the bounds of the region. 
Move "." to the start, and kill the characters.  Bound to "C-W". 

 */
int killregion(int f, int n)
{
    register int    s;
    REGION      region;

    (void)defaultargs(f,n);
    
    if ((s=getregion(&region)) != TRUE) {
        return (s);
    }
    if ((lastflag&CFKILL) == 0) {     /* This is a kill type  */
        kdelete();                    /* command, so do magic */
    }
    thisflag |= CFKILL;               /* kill buffer stuff.   */
    curwp->dotp = region.lines;
    curwp->doto = region.offset;
    return (ldelete(region.size, TRUE));
}

/*

Copy all of the characters in the region to the kill buffer.  Don't move dot
at all.  This is a bit like a kill region followed by a yank.  Bound to
"M-W". 

*/
int copyregion(int f, int n)
{
    register LINE   *lp;
    register int    loffs;
    register int    s;
    REGION          region;

    (void)defaultargs(f,n);
    
    if ((s=getregion(&region)) != TRUE) {
        return (s);
    }
    if ((lastflag&CFKILL) == 0) {   /* Kill type command.   */
        kdelete();
    }
    thisflag |= CFKILL;
    lp = region.lines;           /* Current line.    */
    loffs = region.offset;          /* Current offset.  */
    while (region.size--) {
        if (loffs == llength(lp)) {  /* End of line.     */
            if ((s=kinsert('\n')) != TRUE) {
                return (s);
            }
            lp = lforw(lp);
            loffs = 0;
        } else {            /* Middle of line.  */
            if ((s=kinsert(lgetc(lp, loffs))) != TRUE) {
                return (s);
            }
            ++loffs;
        }
    }
    return (TRUE);
}

/*

Lower case region.  Zap all of the upper case characters in the region to
lower case.  Use the region code to set the limits.  Scan the buffer, doing
the changes.  Call "lchange" to ensure that redisplay is done in all buffers. 
Bound to "C-X C-L". 

*/
int lowerregion(int f, int n)
{
    register LINE   *lp;
    register int    loffs;
    register int    c;
    register int    s;
    REGION          region;

    (void)defaultargs(f,n);
    
    if ((s=getregion(&region)) != TRUE){
        return (s);
    }
    lchange();
    lp = region.lines;
    loffs = region.offset;
    while (region.size--) {
        if (loffs == llength(lp)) {
            lp = lforw(lp);
            loffs = 0;
        }
        else {
            c = lgetc(lp, loffs);
            if (c>='A' && c<='Z') {
                lputc(lp, loffs, c+'a'-'A');
            }
            ++loffs;
        }
    }
    return (TRUE);
}

/*

Upper case region.  Zap all of the lower case characters in the region to
upper case.  Use the region code to set the limits.  Scan the buffer, doing
the changes.  Call "lchange" to ensure that redisplay is done in all buffers. 
Bound to "C-X C-L". 

*/
int upperregion(int f, int n)
{
    register LINE   *lp;
    register int    loffs;
    register int    c;
    register int    s;
    REGION          region;

    (void)defaultargs(f,n);
    
    if ((s=getregion(&region)) != TRUE) {
        return (s);
    }
    lchange();
    lp = region.lines;
    loffs = region.offset;
    while (region.size--) {
        if (loffs == llength(lp)) {
            lp = lforw(lp);
            loffs = 0;
        }
        else {
            c = lgetc(lp, loffs);
            if (c>='a' && c<='z') {
                lputc(lp, loffs, c-'a'+'A');
            }
            ++loffs;
        }
    }
    return (TRUE);
}

/*

This routine figures out the bounds of the region in the current window, and
fills in the fields of the "REGION" structure pointed to by "rp".  Because
the dot and mark are usually very close together, we scan outward from dot
looking for mark.  This should save time.  Return a standard code.  Callers
of this routine should be prepared to get an "ABORT" status; we might make
this have the conform thing later. 

*/
int getregion(REGION *rp)
{
    register LINE   *flp;
    register LINE   *blp;
    register int    fsize;
    register int    bsize;

    if (curwp->markp == NULL) {
        mlwrite("No mark set in this window");
        return (FALSE);
    }
    if (curwp->dotp == curwp->markp) {
        rp->lines = curwp->dotp;
        if (curwp->doto < curwp->marko) {
            rp->offset = curwp->doto;
            rp->size = curwp->marko-curwp->doto;
        }
        else {
            rp->offset = curwp->marko;
            rp->size = curwp->doto-curwp->marko;
        }
        return (TRUE);
    }
    blp = curwp->dotp;
    bsize = curwp->doto;
    flp = curwp->dotp;
    fsize = llength(flp)-curwp->doto+1;
    while( flp!=curbp->lines || lback(blp)!=curbp->lines ){
        if( flp != curbp->lines ) {
            flp = lforw(flp);
            if( flp == curwp->markp ) {
                rp->lines = curwp->dotp;
                rp->offset = curwp->doto;
                rp->size = fsize+curwp->marko;
                return (TRUE);
            }
            fsize += llength(flp);
            fsize++; // newline
        }
        if( lback(blp) != curbp->lines ) {
            blp = lback(blp);
            bsize += llength(blp);
            bsize ++; // newline
            if( blp == curwp->markp ) {
                rp->lines = blp;
                rp->offset = curwp->marko;
                rp->size = bsize - curwp->marko;
                return (TRUE);
            }
        }
    }
    mlwrite("Bug: lost mark");
    return (FALSE);
}

