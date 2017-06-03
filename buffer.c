/*
 * Buffer management.
 * Some of the functions are internal,
 * and some are actually attached to user
 * keys. Like everyone else, they set hints
 * for the display system.
 */
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    "ed.h"

/*
 * Attach a buffer to a window. The
 * values of dot and mark come from the buffer
 * if the use count is 0. Otherwise, they come
 * from some other window.
 */
usebuffer(f, n)
{
    register BUFFER *bp;
    register WINDOW *wp;
    register int    s;
    BYTE        bufn[NBUFN];

    (void) defaultargs(f,n);
    if( (s=mlreply("Use buffer: ", bufn, NBUFN)) != TRUE )
        return (s);
    if( (bp=bfind(bufn, TRUE, 0)) == NULL )
        return (FALSE);
    if( --curbp->nwnd == 0 ){       /* Last use.        */
        curbp->dotp  = curwp->dotp;
        curbp->doto  = curwp->doto;
        curbp->markp = curwp->markp;
        curbp->marko = curwp->marko;
    }
    curbp = bp;             /* Switch.      */
    curwp->bufp  = bp;
    curwp->topp = bp->lines;        /* For macros, ignored. */
//  curwp->flag |= WFMODE|WFFORCE;  /* Quite nasty.     */
    if( bp->nwnd++ == 0 ){      /* First use.       */
        curwp->dotp  = bp->dotp;
        curwp->doto  = bp->doto;
        curwp->markp = bp->markp;
        curwp->marko = bp->marko;
        return (TRUE);
    }
    wp = wheadp;                /* Look for old.    */
    while( wp != NULL ){
        if( wp!=curwp && wp->bufp==bp ){
            curwp->dotp  = wp->dotp;
            curwp->doto  = wp->doto;
            curwp->markp = wp->markp;
            curwp->marko = wp->marko;
            break;
        }
        wp = wp->wndp;
    }
    return (TRUE);
}

/*
 * Dispose of a buffer, by name.
 * Ask for the name. Look it up (don't get too
 * upset if it isn't there at all!). Get quite upset
 * if the buffer is being displayed. Clear the buffer (ask
 * if the buffer has been changed). Then free the header
 * line and the buffer header. Bound to "C-X K".
 */
killbuffer(f, n)
{
    register BUFFER *bp;
    register int    s;
    BYTE        bufn[NBUFN];

    (void)defaultargs(f,n);
    if( (s=mlreply("Kill buffer: ", bufn, NBUFN)) != TRUE )
        return (s);
    if( (bp=bfind(bufn, FALSE, 0)) == NULL ) /* Easy if unknown.    */
        return (TRUE);
    if( bp->nwnd != 0 ){            /* Error if on screen.  */
        mlwrite("Buffer is being displayed");
        return (FALSE);
    }
    if( (s=bclear(bp)) != TRUE )        /* Blow text away.  */
        return (s);
    return freebuffer(bp);
}

int
freebuffer(bp)
BUFFER *bp;
{
    BUFFER  *bp1;
    BUFFER  *bp2;

    free((void *) bp->lines);       /* Release header line. */
    bp1 = NULL;             /* Find the header. */
    bp2 = bheadp;
    while( bp2 != bp ){
        bp1 = bp2;
        bp2 = bp2->bufp;
    }
    bp2 = bp2->bufp;            /* Next one in chain.   */
    if( bp1 == NULL )           /* Unlink it.       */
        bheadp = bp2;
    else
        bp1->bufp = bp2;
    free((void *) bp);          /* Release buffer block */
    return (TRUE);
}

/*
 * List all of the active
 * buffers. First update the special
 * buffer that holds the list. Next make
 * sure at least 1 window is displaying the
 * buffer list, splitting the screen if this
 * is what it takes. Lastly, repaint all of
 * the windows that are displaying the
 * list. Bound to "C-X C-B".
 */
listbuffers(f, n)
{
    register WINDOW *wp;
    register BUFFER *bp;
    register int    s;

    (void)defaultargs(f,n);
    if( (s=makelist()) != TRUE )
        return (s);
    if( blistp->nwnd == 0 ){        /* Not on screen yet.   */
        if( (wp=wpopup()) == NULL )
            return (FALSE);
        bp = wp->bufp;
        if( --bp->nwnd == 0 ){
            bp->dotp  = wp->dotp;
            bp->doto  = wp->doto;
            bp->markp = wp->markp;
            bp->marko = wp->marko;
        }
        wp->bufp  = blistp;
        ++blistp->nwnd;
    }
    wp = wheadp;
    while( wp != NULL ){
        if( wp->bufp == blistp ){
            wp->topp = lforw(blistp->lines);
            wp->dotp  = lforw(blistp->lines);
            wp->doto  = 0;
            wp->markp = NULL;
            wp->marko = 0;
            wp->flag |= WFMODE;
        }
        wp = wp->wndp;
    }
    return (TRUE);
}
/*
 * show the help buffer
 * make sure at least 1 window is displaying the
 * help buffer, splitting the screen if this
 * is what it takes. Lastly, repaint all of
 * the windows that are displaying the
 * list. Bound to "C-X C-B".
 */
helpkeys(f, n)
{
    register WINDOW *wp;
    register BUFFER *bp;
    register int    s;

    (void)defaultargs(f,n);
    if( helpbp->nwnd == 0 ){        /* Not on screen yet.   */
        if( (wp=wpopup()) == NULL )
            return (FALSE);
        bp = wp->bufp;
        if( --bp->nwnd == 0 ){
            bp->dotp  = wp->dotp;
            bp->doto  = wp->doto;
            bp->markp = wp->markp;
            bp->marko = wp->marko;
        }
        wp->bufp  = helpbp;
        ++helpbp->nwnd;
    }
    wp = wheadp;
    while( wp != NULL ){
        if( wp->bufp == helpbp ){
            wp->topp = lforw(helpbp->lines);
            wp->dotp  = lforw(helpbp->lines);
            wp->doto  = 0;
            wp->markp = NULL;
            wp->marko = 0;
            wp->flag |= WFMODE;
        }
        wp = wp->wndp;
    }
    return (TRUE);
}

/*
 * This routine rebuilds the
 * text in the special secret buffer
 * that holds the buffer list. It is called
 * by the list buffers command. Return TRUE
 * if everything works. Return FALSE if there
 * is an error (if there is no memory).
 */
makelist()
{
    BYTE    *cp1;
    BYTE    *cp2;
    int c;
    BUFFER  *bp;
    LINE    *lp;
    int nbytes;
    int s;
    int type;
    BYTE           b[6+1];
    BYTE           line[128];

    blistp->flag &= ~BFCHG;     /* Don't complain!  */
    if( (s=bclear(blistp)) != TRUE )    /* Blow old text away   */
        return (s);
    strcpy((char *)blistp->fname, "");
    if( addline(blistp,"C   Size Buffer           File") == FALSE
    ||  addline(blistp,"-   ---- ------           ----") == FALSE )
        return (FALSE);
    bp = bheadp;                /* For all buffers  */
    while( bp != NULL ){
        if( (bp->flag&BFTEMP) != 0 ){   /* Skip magic ones. */
            bp = bp->bufp;
            continue;
        }
        cp1 = &line[0];         /* Start at left edge   */
        if( (bp->flag&BFCHG) != 0 ) /* "*" if changed   */
            *cp1++ = '*';
        else
            *cp1++ = ' ';
        *cp1++ = ' ';           /* Gap.         */
        nbytes = 0;         /* Count bytes in buf.  */
        lp = lforw(bp->lines);
        while( lp != bp->lines ){
            nbytes += llength(lp)+1;
            lp = lforw(lp);
        }
        citoa(b, 6, nbytes);        /* 6 digit buffer size. */
        cp2 = &b[0];
        while( (c = *cp2++) != 0 )
            *cp1++ = c;
        *cp1++ = ' ';           /* Gap.         */
        cp2 = &bp->bname[0];        /* Buffer name      */
        while( (c = *cp2++) != 0 )
            *cp1++ = c;
        cp2 = &bp->fname[0];        /* File name        */
        if( *cp2 != 0 ){
            while( cp1 < &line[1+1+6+1+NBUFN+1] )
                *cp1++ = ' ';       
            while( (c = *cp2++) != 0 ){
                if( cp1 < &line[128-1] )
                    *cp1++ = c;
            }
        }
        *cp1 = 0;           /* Add to the buffer.   */
        if( addline(blistp,line) == FALSE )
            return (FALSE);
        bp = bp->bufp;
    }
    return (TRUE);              /* All done     */
}

citoa(buf, width, num)
BYTE   buf[];
int    width;
int    num;
{
    buf[width] = 0;             /* End of string.   */
    while( num >= 10 ){         /* Conditional digits.  */
        buf[--width] = (num%10) + '0';
        num /= 10;
    }
    buf[--width] = num + '0';       /* Always 1 digit.  */
    while( width != 0 )         /* Pad with blanks. */
        buf[--width] = ' ';
    return TRUE;
}

/*
 * add a line to a buffer
 * The argument "text" points to
 * a string. Append this line to the
 * buffer list buffer. Handcraft the EOL
 * on the end. Return TRUE if it worked and
 * FALSE if you ran out of room.
 */
addline(bp,text)
BUFFER  *bp;
BYTE    *text;
{
    register LINE   *lp;
    register int    i;
    register int    ntext;

    ntext = strlen((char *)text);
    if( (lp=ladd(bp->lines,ntext)) == NULL )
        return (FALSE);
    for( i=0; i<ntext; ++i )
        lputc(lp, i, text[i]);
    lp->used = ntext;
    if( bp->dotp == bp->lines ) /* If "." is at the end */
        bp->dotp = lp;      /* move it to new line  */

    return (TRUE);
}

/*
 * Look through the list of
 * buffers. Return TRUE if there
 * are any changed buffers. Buffers
 * that hold magic internal stuff are
 * not considered; who cares if the
 * list of buffer names is hacked.
 * Return FALSE if no buffers
 * have been changed.
 */
anycb()
{
    register BUFFER *bp;

    bp = bheadp;
    while( bp != NULL ){
        if( (bp->flag&BFTEMP)==0 && (bp->flag&BFCHG)!=0 )
            return (TRUE);
        bp = bp->bufp;
    }
    return (FALSE);
}

/*
Find a buffer, by name.  Return a pointer to the BUFFER structure associated
with it.  If the named buffer is found, but is a TEMP buffer (like the buffer
list) complain.  If the buffer is not found and the "cflag" is TRUE, create
it.  The "bflag" is the settings for the flags in in buffer. 
 */
BUFFER  *
bfind(bname, cflag, bflag)
BYTE   *bname;
int cflag;
int bflag;
{
    BUFFER *bp;
    LINE   *lp;

    bp = bheadp;
    while( bp != NULL ){
        if( strcmp((char *)bname, (char *)bp->bname) == 0  ){
            if( (bp->flag&BFTEMP) != 0  ){
                mlwrite("Cannot select builtin buffer");
                return (NULL);
            }
            return (bp);
        }
        bp = bp->bufp;
    }
    if( cflag ){
        if( (bp=(BUFFER *)malloc(sizeof(BUFFER))) == NULL )
            return (NULL);
        if( (lp=lalloc(8)) == NULL  ){
            free((void *) bp);
            return (NULL);
        }
        strcpy((char *)lp->text,"*EOB*");
        lp->flags = L_HEAD;
        bp->bufp  = bheadp;
        bheadp = bp;
        bp->mode = MDMAGIC;
        bp->dotp  = lp;
        bp->doto  = 0;
        bp->markp = NULL;
        bp->marko = 0;
        bp->flag  = bflag;
        bp->nwnd  = 0;
        bp->lines = lp;
        strcpy((char *)bp->fname, "");
        strcpy((char *)bp->bname, (char *)bname);
        bp->passwd[0] = '\0';
        lp->fp = lp;
        lp->bp = lp;
    }
    return (bp);
}

/*
This routine blows away all of the text in a buffer.  If the buffer is marked
as changed then we ask if it is ok to blow it away; this is to save the user
the grief of losing text.  The window chain is nearly always wrong if this
gets called; the caller must arrange for the updates that are required. 
Return TRUE if everything looks good. 
*/
bclear(bp)
register BUFFER *bp;
{
    register LINE   *lp;
    register int    s;
    
    if( (bp->flag&BFTEMP) == 0      /* Not scratch buffer.  */
    && (bp->flag&BFCHG) != 0        /* Something changed    */
    && (s=mlyesno("Discard changes")) != TRUE )
        return (s);
    bp->flag  &= ~BFCHG;            /* Not changed      */
    while( (lp=lforw(bp->lines)) != bp->lines )
        lfree(lp);
    bp->dotp  = bp->lines;      /* Fix "."      */
    bp->doto  = 0;
    bp->markp = NULL;           /* Invalidate "mark"    */
    bp->marko = 0;
    return (TRUE);
}


// figure out how many characters in a buffer
buffer_size(bp)
BUFFER *bp;
{
    register LINE *clp;
    int nch;    
    clp = lforw(bp->lines);
    nch = 0;
    while( clp != bp->lines ) { // go through all lines
        if( clp->flags ) { nch++; }
        nch += llength(clp);
        clp = lforw(clp);
    }
    return nch;
}    

pack(f,n)
{

    register BUFFER *bp;
    register int    s;
    BYTE        bufn[NBUFN];
//    if( (s=mlreply("Packbuffer: ", bufn, NBUFN)) != TRUE )
//        return (s);
//    if( (bp=bfind(bufn, FALSE, 0)) == NULL ) /* Easy if unknown.    */
//        return (TRUE);
//    if( (s=pack_buffer(bp)) != TRUE ) 
    if( (s=pack_buffer(curbp)) != TRUE ) 
        return (s);
    return 1;
}

int
pack_buffer(bp)
BUFFER *bp;
{
    mlwrite("pack called %d",buffer_size(bp));
    return TRUE;
}
