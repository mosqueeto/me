/*

The functions in this file are a general set of line management utilities. 
They are the only routines that touch the text.  They also touch the buffer
and window structures, to make sure that the necessary updating gets done. 
There are routines in this file that handle the kill buffer too.  It isn't
here for any good reason. 

Note that this code only updates the dot and mark values in the window list. 
Since all the code acts on the current window, the buffer that we are editing
must be being displayed, which means that "nwnd" is non zero, which means
that the dot and mark values in the buffer headers are nonsense. 

"Lines" in this version have a direct correspondence with "text lines" -- 
that is, records delimited by "newline" characters in the input text.

Since in a typical editing session there may be many lines unchanged, the
original text is read into a single buffer, if possible, and the lines just
point to character positions in that buffer.  This situation is signified by
a flag setting.

*/
#include    <stdio.h>
#include    <stdlib.h>
#include    "ed.h"

#define KBLOCK  256         // Kill buffer block size
#define LEXTRA  16          // extra space allocated for lines

char    *kbufp  = NULL;     // Kill buffer data
int kused   = 0;            // # of bytes used in KB
int ksize   = 0;            // # of bytes allocated in KB

static LINE *free_line = (LINE *)NULL;

/*

This routine allocates a line descriptor block, without filling in the char
pointer to the data.  It is used during the file read logic to point lines
into the file buffer.  Doesn't set *any* flags or links; re-uses old lines 
if possible.

*/
LINE * 
lballoc()
{
    register LINE   *lp;

    if( free_line != (LINE *)NULL ) {
        lp = free_line;
        free_line = free_line->fp;
        return lp;
    }
    return (LINE*)malloc(sizeof(LINE));
}

/*

Allocate a block of memory large enough to hold a LINE containing "used"
characters.  The block is always rounded up a bit.  Return a pointer to the
new block, or NULL if there isn't any memory left.  Print a message in the
message line if no space. 

*/
LINE *
lalloc(int used)
{
    register LINE   *lp;
    register int    size;
    char message[80];

    if( used == 0 ) size = term.ncol + LEXTRA;
    else size = used + LEXTRA;

    if( (lp = lballoc()) == NULL ) {
        mlwrite("Cannot allocate a new line");
        return NULL;
    }

    // if the used < 0, then we don't allocate a text block;
    // the caller is responsible for pointing to already existing
    // text, and setting any flags
    if( used >= 0 ) {    
        if( (lp->text = (BYTE *) malloc(size)) == NULL ) {
            mlwrite("Cannot allocate %d bytes", size);
            return (NULL);
        }
    }
    lp->size = size;
    lp->used = 0;
    lp->flags = 0;
    return (lp);
}


/*
 * Add a line with enough space for "used" characters just
 * above "lp"
 */
 
LINE *
ladd(LINE *lp, int used)
{
    LINE *lp1,*lp2;
    
    lp1 = lalloc(used);
    if( lp1 != (LINE *)NULL ) {
        lp1->flags = L_NL;
        lp2 = lback(lp);
        lp2->fp = lp1;
        lp1->fp = lp;
        lp1->bp = lp2;
        lp->bp = lp1;
        lp1->flags = L_NL;
    }
    return lp1;
}
    
/*

Delete line "lp".  Fix all of the links that might point at it (they are
moved to offset 0 of the next line.  Unlink the line from whatever buffer it
might be in.  Release the memory.  The buffers are updated too; the magic
conditions described in the above comments don't hold here. 

*/
void
lfree(LINE *lp)
{
    register BUFFER *bp;
    register WINDOW *wp;

    wp = wheadp;
    while (wp != NULL) {
        if (wp->topp == lp) wp->topp = lp->fp;

        if (wp->dotp  == lp) {
            wp->dotp  = lp->fp;
            wp->doto  = 0;
        }

        if (wp->markp == lp) {
            wp->markp = lp->fp;
            wp->marko = 0;
        }

        wp = wp->wndp;
    }

    bp = bheadp;
    while (bp != NULL) {
        if (bp->nwnd == 0) {
            if (bp->dotp  == lp) {
                bp->dotp = lp->fp;
                bp->doto = 0;
            }

            if (bp->markp == lp) {
                bp->markp = lp->fp;
                bp->marko = 0;
            }
        }
        bp = bp->bufp;
    }
    lp->bp->fp = lp->fp;
    lp->fp->bp = lp->bp;
    if ( lp->text != (unsigned char *)NULL && ! (lp->flags & L_EXT) ) {
		free((unsigned char *) lp->text);
    }
    lp->text = (BYTE *)NULL;
    lp->bp = (LINE *)NULL;
    lp->used = 0;
    lp->size = 0;
    lp->flags = 0;
    // line descriptors are kept on a free list
    lp->fp = free_line;
    free_line = lp;
}

/*

This routine gets called when a character is changed in place in the current
buffer.  It updates all of the required flags in the buffer and window
system.  

*/
void 
lchange()
{
    WINDOW *wp;

    text_chg_flag++;
    if ((curbp->flag&BFCHG) == 0) { // First change, so
        curbp->flag |= BFCHG;       // update mode lines
    }
    wp = wheadp;
    while (wp != NULL) {
        if (wp->bufp == curbp)
            wp->flag |= WFMODE;
        wp = wp->wndp;
    }
}

/*

Insert the character "c" in the specified line and offset.  When the window
list is updated, take special care.  You always update dot in the current
window.  You update mark, and a dot in another window, if it is greater than
the place where you did the insert.  Return TRUE if all is well, and FALSE on
errors. 

*/

int linsertc(int c, LINE *lp, int offset)
{
    register unsigned char   *src;
    register unsigned char   *snk;
    register LINE   *lp1;
    register int new_size;
    register int    i,j;
    register unsigned char tc;
    register WINDOW *wp;

    lchange();
    
    //  There is a truly simple case where the  insertion
    // takes place at the end of the buffer.  Under this
    // circumstance it isn't possible for any other window
    // to have dot or mark in the currently non-existent 
    // line.

// update:  I believe that this special case is not needed, since the
// only place that linsertc is called is from linsert, and linsert already 
// has a fix for it.

    if( lp == curbp->lines ) {  // end of buffer
mlwrite("bug: linsertc");
        if( curwp->doto != 0 ) {
            mlwrite("bug: linsert");
            return (FALSE);
        }
        if( (lp1=ladd(curbp->lines,0)) == NULL ) return (FALSE);
        lp1->text[0] = c;
        lp1->used = 1;
        curwp->dotp = curbp->dotp = lp1;
        curwp->doto = curbp->doto = 0;
        return (TRUE);
    }

    // next is the case where the character fits in
    // current text block...
    
    if( lp->used < lp->size )    {
        /* shift upper chars right 1 space... */
        snk = &lp->text[lp->used];
        src = snk-1;
        while (src >= &lp->text[offset])  *snk-- = *src--;
        // ...and stick the character in place
        lp->text[offset] = c;
        lp->used++;
        lp1 = lp;           // no lines changed
    } 
    else {   
        // Ok, used is >= size.  we need to reallocate the text block.
        new_size = lp->size * 2 + LEXTRA;
        if( (snk = (BYTE *)malloc(new_size)) == NULL ) return FALSE;
        j = 0;
        for( i=0;i<offset;i++ ) snk[j++] = lp->text[i];
        snk[j++] = c;
        for( ;i<lp->used;i++ ) snk[j++] = lp->text[i];

        // free old text, if possible, and update flags
        if( lp->flags & L_EXT ) lp->flags &= ~L_EXT;
        else free(lp->text);
        lp->text = snk;
        lp->used++;
        lp->size = new_size;
        lchange();      // this could mess up the window...
        lp1 = lp;       // for the window update code, below
    }

    // Update windows.

    wp = wheadp;
    while( wp != NULL ) {
        if( wp->dotp == lp && wp->doto >= offset ) {
            wp->doto++;
        }
    
        if( wp->markp == lp ) {
            if( wp->marko > offset ) {
                if( offset < term.ncol ) {
                    wp->marko++;
                } else {
                    wp->markp = lforw(lp);
                    wp->marko = 1;
                }
            }
        }
        wp = wp->wndp;
    }
    return (TRUE);
}

/*

Insert "n" copies of the character "c" at the current location of dot. 

*/
int linsert(int n, int c)
{
    if( n <= 0 ) return FALSE;
    // handle special case when at very beginning of empty file
    // this maybe should be handled in the buffer initialization
    if( curwp->dotp == curwp->bufp->lines ) {
        lnewline();
        backchar();
    }
    while( n-- > 0 ) {
        if( linsertc(c,curwp->dotp,curwp->doto) == FALSE ) 
            return FALSE;
    }
    return TRUE;
}

/*
 
Insert a newline into the buffer at the current location of dot in the
current window.  Return TRUE if everything works out and FALSE on error
(memory allocation failure).  The update of dot and mark is a bit easier then
in the above case, because the split forces more updating. 

*/
lnewline()
{
    unsigned char   *cp1;
    unsigned char   *cp2;
    LINE   *lp1;
    LINE   *lp2;
    int    doto;
    int    newz;
    WINDOW *wp;

    lchange();
    lp1  = curwp->dotp;         // Get the address and
    doto = curwp->doto;         // offset of "."

    // special case at eob
    if( lp1 == curbp->lines ) {     
        if( (lp2=ladd(lp1,0)) == NULL ) return FALSE;
        lp2->used = 0;
        curwp->dotp = lp2;
        curwp->doto = 0;
        return ( TRUE );
    } 

    // normal case.  append a new line
    newz = (lp1->used - doto);
    if( newz < term.ncol ) newz = term.ncol;
    if( (lp2=ladd(lforw(lp1),newz)) == NULL ) return (FALSE);
//    lp2->flags = lp1->flags;
    
    // copy the trailing text to the new line
    cp1 = &lp1->text[doto];
    cp2 = lp2->text;
    while (cp1 < &lp1->text[lp1->used]) *cp2++ = *cp1++;
    lp2->used = lp1->used - doto;
    lp1->used = doto;
    
    // clean up the windows
    wp = wheadp;
    while (wp != NULL) {
        if (wp->dotp == lp1) {
            if (wp->doto >= doto) {
                wp->dotp = lp2;
                wp->doto -= doto;
            }
        }
        if (wp->markp == lp1) {
            if (wp->marko > doto) {
                wp->markp = lp2;
                wp->marko -= doto;
            }
        }
        wp = wp->wndp;
    }   
    return (TRUE);
}
    
/*

This function deletes "n" bytes, starting at dot.  It understands how do deal
with end of lines, etc.  It returns TRUE if all of the characters were
deleted, and FALSE if they were not (because dot ran into the end of the
buffer.  The "kflag" is TRUE if the text should be put in the kill buffer. 

*/
int ldelete(int n, int kflag)
{
    register unsigned char   *cp1;
    register unsigned char   *cp2;
    register LINE   *dotp;
    register int    doto;
    register int    chunk;
    register WINDOW *wp;

    if( n > 30 ) kexpand( n );  // expand the size of the killbuffer

    while( n > 0) {
        dotp = curwp->dotp;
        doto = curwp->doto;

        if( dotp == curbp->lines)  return (FALSE);  //  end of buffer

        chunk = dotp->used - doto;    // Size of chunk at end.
        if( chunk > n) chunk = n;

        if( chunk == 0) {           // End of line, merge and redo
            lchange();
            --n;
            if( ldelnewline() == FALSE )  return FALSE;
            if( kflag  && kinsert('\n') == FALSE) return FALSE;
            continue;
        }
        lchange();

        cp1 = &dotp->text[doto];    // Scrunch text
        cp2 = cp1 + chunk;
        if( kflag != FALSE) {       // Kill?
            while( cp1 != cp2) {
                if( kinsert(*cp1) == FALSE) return (FALSE);
                ++cp1;
            }
            cp1 = &dotp->text[doto];
        }
        while( cp2 != &dotp->text[dotp->used]) {
            *cp1++ = *cp2++;
        }
        dotp->used -= chunk;
        wp = wheadp;            // Fix windows
        while( wp != NULL) {
            if( wp->dotp==dotp && wp->doto>=doto) {
                wp->doto -= chunk;
                if( wp->doto < doto)
                    wp->doto = doto;
            }   
            if( wp->markp==dotp && wp->marko>=doto) {
                wp->marko -= chunk;
                if( wp->marko < doto)
                    wp->marko = doto;
            }
            wp = wp->wndp;
        }
        n -= chunk;
    }
    return (TRUE);
}

/*

Delete a newline.  Join the current line with the next line.  If the next
line is the magic header line always return TRUE; merging the last line with
the header line can be thought of as always being a successful operation,
even if nothing is done, and this makes the kill buffer work "right".  Easy
cases can be done by shuffling data around.  Hard cases require that lines be
moved about in memory.  Return FALSE on error and TRUE if all looks ok. 
Called by "ldelete" only. 

 */
ldelnewline()
{
    LINE   *lp1;
    LINE   *lp2;
	LINE   *lp3;
    int     i;
    int     j;
    

    lp1 = curwp->dotp;
    lp2 = lp1->fp;

    // At the buffer end.  Is this code right????
    if( lp2 == curbp->lines ) {
        lp2->flags &= (~(L_NL));
//		if( curwp->doto > 0 ) curwp->doto--;
        return (FALSE);
    }

    // Blank second line.  free it, and link in successor
    if( lp2->used == 0 ) {
        lfree(lp2);
		return( TRUE );
	}

    // Blank first line.  free it, and link in successor
    if( lp1->used == 0 ) {
        lfree(lp1);
		return( TRUE );
	}

    // neither is blank.  insert a new line with enough room
    // for all the text just before the current line, insert the text,
    // and free the old lines
    lp3 = ladd(lp1, lp1->used + lp2->used + LEXTRA );
    curwp->dotp = lp3;
    curwp->doto = lp1->used;
    i = 0;
    for( j=0; j<lp1->used; j++ ) lp3->text[i++] = lp1->text[j];
    for( j=0; j<lp2->used; j++ ) lp3->text[i++] = lp2->text[j];
    lp3->used = lp1->used + lp2->used;
    lfree(lp1);
    lfree(lp2);

    return(TRUE);
}

/*
 * Delete all of the text
 * saved in the kill buffer. Called by commands
 * when a new kill context is being created. The kill
 * buffer array is released, just in case the buffer has
 * grown to immense size. No errors.
 */
void kdelete()
{
    if (kbufp != NULL) {
        free((char *) kbufp);
        kbufp = NULL;
        kused = 0;
        ksize = 0;
    }
}

/*
 * Insert a character to the kill buffer,
 * enlarging the buffer if there isn't any room. Always
 * grow the buffer in chunks, on the assumption that if you
 * put something in the kill buffer you are going to put
 * more stuff there too later. Return TRUE if all is
 * well, and FALSE on errors.
 */
int kinsert(int c)
{
    register char   *nbufp;
    register int    i;

    if (kused == ksize) {
        if ((nbufp=(char *)malloc(ksize+KBLOCK)) == NULL)
            return (FALSE);
        for (i=0; i<ksize; ++i)
            nbufp[i] = kbufp[i];
        if (kbufp != NULL)
            free((char *) kbufp);
        kbufp  = nbufp;
        ksize += KBLOCK;
    }
    kbufp[kused++] = c;
    return (TRUE);
}

int kexpand(int n)
{
    register char   *nbufp;
    register int    i;

    if ((nbufp=(char *)malloc(ksize+n+KBLOCK)) == NULL)
        return (FALSE);
    for (i=0; i<ksize; ++i)
        nbufp[i] = kbufp[i];
    if (kbufp != NULL)
        free((char *) kbufp);
    kbufp  = nbufp;
    ksize += KBLOCK + n;

    return (TRUE);
}


/*
 * This function gets characters from
 * the kill buffer. If the character index "n" is
 * off the end, it returns "-1". This lets the caller
 * just scan along until it gets a "-1" back.
 */
int kremove(int n)
{
    if (n >= kused)
        return (-1);
    else
        return (kbufp[n] & 0xFF);
}
