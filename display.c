/*

The functions in this file handle redisplay.

*/
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    "ed.h"
#include    <stdarg.h>

typedef struct  VIDEO {
    int   v_flag;    // Flags
    BYTE *v_text;    // Screen data
}   VIDEO;

int sgarbf  = TRUE;     // TRUE if screen is garbage
int mpresf  = FALSE;    // TRUE if message in last line
int vtrow   = 0;        // Row location of SW cursor
int vtcol   = 0;        // Column location of SW cursor
//int ttrow   = HUGE;     // Row location of HW cursor
//int ttcol   = HUGE;     // Column location of HW cursor

int ttrows = 0;
int ttcols = 0;

/*

The two "screens" are for bookkeeping.  Editing changes are done to the
"virtual screen", vscreen.  Then vscreen is compared to the "reference
screen", rscreen, and writes to the physical device are done wherever there
is a difference detected.  rscreen is updated at the same time, so it is
always maintained as an in-core mirror of the physical screen.  This
technique is necessary because we can't read the data currently in the
physical display. 

*/

VIDEO   *vscreen;       // virtual screen
VIDEO   *rscreen;       // reference screen

static BYTE hexdigit[] = "0123456789abcdef";

void resize()
{
    int    i,j;
    BYTE   *cp;
    register WINDOW *wp;

    for( i=0; i<term.nrow; ++i) { free(vscreen[i].v_text); }
    free(vscreen);

    for( i=0; i<term.nrow; ++i) { free(rscreen[i].v_text); }
    free(rscreen);

    (*term.close)();
    (*term.open)();

    vscreen = (VIDEO *) malloc(term.nrow*sizeof(VIDEO));
    if( vscreen == NULL) abort();

    rscreen = (VIDEO *) malloc(term.nrow*sizeof(VIDEO));
    if( rscreen == NULL) abort();

    for( i=0; i<term.nrow; ++i) {
        // allocate a new "virtual screen"
        cp = (BYTE *) malloc(term.ncol+4);
        if( cp == NULL) abort();
        vscreen[i].v_text = cp;
        vscreen[i].v_flag = 0;
        for(  j=0;j<term.ncol;j++ ) cp[j]=' ';

        // allocate a new "reference screen
        cp = (BYTE *) malloc(term.ncol+4);
        if( cp == NULL) abort();
        rscreen[i].v_text = cp;
        rscreen[i].v_flag = 0;
        for(  j=0;j<term.ncol;j++ ) cp[j]=' ';
    }

    sgarbf = TRUE;
    wp = wheadp;
    while( wp != NULL) {
//        wp->flag |= ( WFFORCE );
        wp->ntrows = term.nrow-1;
        wp = wp->wndp;
    }
}


/*

Initialize the data structures used by the display code.  The edge vectors
used to access the screens are set up.  The operating system's terminal I/O
channel is set up.  All the other things get initialized at compile time. 

*/
vtinit()
{
    int    i,j;
    BYTE   *cp;

    (*term.open)();

    vscreen = (VIDEO *) malloc(term.nrow*sizeof(VIDEO));
    if( vscreen == NULL) abort();

    rscreen = (VIDEO *) malloc(term.nrow*sizeof(VIDEO));
    if( rscreen == NULL) abort();

    for( i=0; i<term.nrow; ++i) {
        cp = (BYTE *) malloc(term.ncol+4);
        if( cp == NULL) abort();

        for(  j=0;j<term.ncol;j++ ) cp[j]=' ';
        vscreen[i].v_text = cp;
        vscreen[i].v_flag = 0;

        cp = (BYTE *) malloc(term.ncol+4);
        if( cp == NULL) abort();

        rscreen[i].v_text = cp;
        rscreen[i].v_flag = 0;
    }
    return TRUE;
}

/*

Clean up the virtual terminal system, in anticipation for a return to the
operating system.  Move down to the last line and clear it out (the next
system prompt will be written in the line).  Shut down the channel to the
terminal. 

*/
vttidy()
{
    movecursor(term.nrow, 0);
    (*term.eeol)();
    (*term.close)();
    return TRUE;
}

/*

Set the virtual cursor to the specified row and column on the virtual screen. 

*/
vtmove(row, col)
{
    if( row > term.nrow ) die("vtmove: nonsense value for row\n");
    if( col > term.ncol ) die("vtmove: nonsense value for col\n");
    if( row < 0 ) die("vtmove: row < 0\n");
    if( col < 0 ) die("vtmove: col < 0\n");
    vtrow = row;
    vtcol = col;
    return TRUE;
}

/*

Write a character to the virtual screen.  The virtual row and column are
updated.  Overflow if the line is too long.  

*/
vtputc(c)
register int    c;
{
    BYTE *cp;

    if( vtrow >= term.nrow ) return FALSE;
    if( vtcol >= term.ncol ) {
        vtrow++;
        if( vtrow >= term.nrow ) return FALSE;
        vtcol = 0;
        vtputc(c);
    }
    else if( c == '\t') {
        do {
            vtputc(' ');
        } while( (vtcol % tabsize) != 0 && vtcol < term.ncol );
    } 
    else {
        cp = vscreen[vtrow].v_text;
        cp[vtcol++] = c;
    }
    return TRUE;
}

vline_offset(LINE *lp, int p) {
    int offset = 0;
    int i = 0;
    int c;
    if( p > lp->used ) {
        die("vline_offset: p > lp->used\n");
    }
    while( i < p ) {
        c = lgetc(lp,i);
        if( c == '\t' ) {
            offset = ( ( offset/tabsize ) + 1) * tabsize;
        }
        else {
            offset++;
        }
        i++;
    }
    return offset;
}

vtcrlf() {
    if( vtrow < term.nrow ) {
        vtrow++;
        vtcol = 0;
    }
}

/*

Erase from the end of the software cursor to the end of the line on which the
software cursor is located. 

*/
vteeol()
{
    BYTE *cp;
    int i;
    i = vtcol;
    if( vtrow >= term.nrow ) return FALSE;
    cp = vscreen[vtrow].v_text;
    while( i < term.ncol) {
        cp[i++] = ' ';
    }
    return TRUE;
}

/*

Make sure that the display is right.  This is a three part process.  First,
scan through all of the windows looking for dirty ones.  Check the framing,
and refresh the screen.  Second, make sure that "currow" and "curcol" are
correct for the current window.  Third, make the virtual and physical screens
the same. 

*/
update()
{
    LINE   *lp;
    WINDOW *wp;
    VIDEO  *vp1;
    VIDEO  *vp2;
    BYTE   *cp1;
    BYTE   *cp2;
    int    target;
    int    row;
    int    remaining_lines;
    int    i;
    int    o;
    int    j;
    int    c;
    int    lines_used;

    static int ncalls = 0;

    if( ncalls++ > 2000 ) bkp();

    wp = wheadp;
    while( wp != NULL) {
        // check the framing for the window
        lp = wp->topp;
        // see if the dot is on the screen
        row = 0;
        while( row < wp->ntrows ) {
            if( lp == wp->dotp ) {
                if( wp->topp == lp && wp->doto < wp->topo ) break;
                lines_used = vline_offset(wp->dotp,wp->doto) / term.ncol + 1;
                if( row + lines_used >= wp->ntrows ) break;
                if( vline_offset(wp->dotp,wp->doto) % term.ncol < term.ncol ) 
                    goto out; // fits on last line, framing ok
                break; // found dot, no need to continue
            }
            else {
                lines_used = vline_offset(lp,lp->used) / term.ncol + 1;
            }
            if( lp == wp->bufp->lines ) break;
            row += lines_used;
            lp = lforw(lp);
        }

        // framing not acceptable. compute a new value
        // for the line at the top of the window.
        // "target" is the screen line we want dot to be on.
        target = wp->force;
        if( target > 0) {
            --target;
            if( target >= wp->ntrows) { target = wp->ntrows-1; }
        }
        else if( target < 0) {
            target += wp->ntrows;
            if( target < 0) { target = 0; }
        } 
        else {
            target = wp->ntrows/2; // middle of the screen
        }

        // find new value for wp->topp and wp->doto
        // count screen lines backward, using up lines,
        // until we find the line at top
        lp = wp->dotp;
        remaining_lines = target 
            - vline_offset(wp->dotp,wp->doto) / term.ncol + 1;
        while( remaining_lines > 0 && lback(lp) != wp->bufp->lines ){
            if( remaining_lines > 0 ) lp = lback(lp);
            lines_used = vline_offset(lp,lp->used) / term.ncol + 1;
            remaining_lines -= lines_used;
        }
        wp->topp = lp;
        wp->topo = 0;
        if( remaining_lines < 0 ) {
            wp->topo = -term.ncol * (remaining_lines + 1);
        }

    out:  // framing check done

        // there used to be code here to try to optimize the case
        // where editing takes place in a single line, but it didn't
        // have any noticable performance impact.  updating the screen
        // simply doesn't take that much time.
        lp = wp->topp;
        row  = wp->toprow;
        vtmove(row,0); 
        // new top line was found above, if needed
        lp = wp->topp;
        j = wp->topo;
        while( vtrow < (wp->toprow + wp->ntrows) ) {
            if( lp == wp->bufp->lines) {  // last line in the buffer
                vteeol();                 // blank the rest
                vtcrlf();
                continue;
            }
            if( j >= llength(lp) ) {
                // precise multiple of line length?
                if( vtcol > 0 && vtcol%term.ncol == 0 ) { 
                    vteeol();
                    vtcrlf();
                }
                vteeol();
                vtcrlf();
                lp = lforw(lp);
                j = 0;
            }
            else {
                vtputc(lgetc(lp, j++));
            }
        }

        modeline(wp);
        wp->flag =  0;
        wp->force = 0;
        wp = wp->wndp;
    } // while


// Recompute the row and column number of the hardware
// cursor. This is the only update for simple moves.

// back up currow to account for long lines that might start back
// up off the screen, then count down lines.  account for the case
// where exactly at end of a screen line

// "curwp->toprow" is only really relevant when there is more than one
// window and the screen is split.  useless complication...

    lp = curwp->topp;
    currow = curwp->toprow - vline_offset(lp,curwp->topo) / term.ncol ;
    while( lp != curwp->dotp) {
        currow += ( vline_offset(lp,lp->used)/(term.ncol) + 1 );
        lp = lforw(lp);
    }
    curcol = 0;
    i = 0;
    while( i < curwp->doto ) {
        c = lgetc(lp, i++);
        if( c == '\t') {
            curcol = ((curcol / tabsize) + 1) * tabsize - 1;
        }
        ++curcol;
      if( curcol >= term.ncol) {
        if( currow < term.nrow ) {
          curcol -= term.ncol;
          currow++;
        }
      }
    }

// Special hacking if the screen is garbage. Clear the hardware
// screen, and update your copy to agree with it. Set all the
// virtual screen change bits, to force a full update.
    if( sgarbf ) {
        for( i=0; i<term.nrow; ++i) {
            cp1 = rscreen[i].v_text;
            for( j=0; j<term.ncol; ++j) {
                cp1[j] = ' ';
            }
        }
        movecursor(0, 0);       // Erase the screen
        (*term.eeop)();
        sgarbf = FALSE;         // Erase-page clears
        mpresf = FALSE;         // the message area
    }

// Make sure that the virtual and reference displays agree.
// Unlike before, the "updateline" code is only called with a
// line that has been updated for sure.
    for( i=0; i<term.nrow; ++i) {
        cp1 = vscreen[i].v_text;
        cp2 = rscreen[i].v_text;
        updateline(i, cp1, cp2);
    }


// Finally, update the hardware cursor and flush out buffers.
    movecursor(currow, curcol);
    (*term.flush)();
    return TRUE;
}

/*

Update a single line.  This does not know how to use insert or delete
character sequences; we are using VT52 functionality.  Update the physical
row and column variables.  It does try to exploit erase to end of line. 

*/
updateline(row, vline, pline)
int     row;
BYTE    vline[];
BYTE    pline[];
{
    BYTE   *cp1;
    BYTE   *cp2;
    BYTE   *cp3;
    BYTE   *cp4;
    BYTE   *cp5;
    int    nbflag;

    // Compute left match.

    cp1 = &vline[0];
    cp2 = &pline[0];
    while( (cp1 < (BYTE *)&vline[term.ncol]) && (cp1[0] == cp2[0]) ) {
        ++cp1;
        ++cp2;
    }
    
    // check that the virtual and physical line aren't identical
    // (This can still happen, even though we only call this routine
    // on changed lines. A hard update is always done when a line
    // splits, a massive change is done, or a buffer is displayed
    // twice. This optimizes out some of the excess updating.)

    if( cp1 == (BYTE *)&vline[term.ncol]) {
        return TRUE;
    }

    // Compute right match

    nbflag = FALSE;
    cp3 = &vline[term.ncol];
    cp4 = &pline[term.ncol];
    while( cp3[-1] == cp4[-1]) {
        --cp3;
        --cp4;
        if( cp3[0] != ' ' ) {   // Note if any nonblank
            nbflag = TRUE;      // in right match.
        }
    }
    cp5 = cp3;
    if( nbflag == FALSE) {      // Erase to EOL ? 
        while( (cp5 != cp1) && (cp5[-1] == ' ') ) {
            --cp5;
        }
        if( cp3-cp5 <= 3) {     // Use only if erase is
            cp5 = cp3;          // fewer characters
        }
    }

    // Go to start of line
    movecursor(row, cp1 - (BYTE *)&vline[0]);

    // spit out necessary updates to the physical device
    while( cp1 != cp5) {
        if( *cp1 < 0x20 ) {
            (*term.hilight)(*cp1 ^ 0x40);
        }
        else if( *cp1 == 0x7F || *cp1 == 0xff ) {
            (*term.hilight)('~');
        }
        else if( *cp1 > 0x80  ) {
            if( *cp1 ^ 0x80 < 0x20 ) (*term.hilight)( (*cp1 ^ 0x80) | 0x20 );
            else (*term.hilight)( *cp1 ^ 0x80 );
        }
        else {
            (*term.putchar)(*cp1);
        }
//        ++ttcol;
        *cp2++ = *cp1++;    // make "physical" screen match display
    }

    if( cp5 != cp3) {       // Erase to end of line
        (*term.eeol)();
        while( cp1 != cp3) {// update physical screen
            *cp2++ = *cp1++;
        }
    }
    return TRUE;
}


/*
 * Write a message into the message
 * line. Keep track of the physical cursor
 * position. A small class of printf like format
 * items is handled. Assumes the stack grows
 * down; this assumption is made by the "++"
 * in the argument scan loop. Set the "message
 * line" flag TRUE.
 */

void mlwrite(BYTE *fmt,...)
{
    va_list ap;
    int     c;
    BYTE    *cp;

    movecursor(term.nrow, 0);
    va_start(ap,fmt);
    while( (c = *fmt++) != 0) {
        if( c != '%') {
            (*term.putchar)(c);
        } else {
            c = *fmt++;
            switch (c) {
            case 'i':
            case 'd':
                mlputi(va_arg(ap,int), 10);
                break;

            case 'o':
                mlputi(va_arg(ap,int),  8);
                break;

            case 'x':
                mlputi(va_arg(ap,int), 16);
                break;

            case 'D':
                mlputli(va_arg(ap,long), 10);
                break;

            case 's':
                mlputs(va_arg(ap,char *));
                break;

            default:
                (*term.putchar)(c);
            }
        }
    }
    va_end(ap);
    (*term.eeol)();
    (*term.flush)();
    mpresf = TRUE;
//    return TRUE;
}


/*

Redisplay the mode line for the window pointed to by the "wp".  This is the
only routine that has any idea of how the modeline is formatted.  You can
change the modeline format by hacking at this routine.  Called by "update"
any time there is a dirty window. 

*/
modeline(wp)
register WINDOW *wp;
{
    BYTE   *cp;
    int    c;
    int    n;
    BUFFER *bp;

    n = wp->toprow+wp->ntrows;      // Location.
    vtmove(n, 0);                   // Seek to right line.
    vtputc('-');
    bp = wp->bufp;
    if( (bp->flag&BFCHG) != 0)      // "*" if changed.
        vtputc('*');
    else
        vtputc('-');
    n  = 2;
    cp = version;                   // Buffer name.
    while( (c = *cp++) != 0) {
        vtputc(c);
        ++n;
    }
    vtputc('-'); ++n;
    vtputc(' '); ++n;
    
    cp = &bp->bname[0];
    while( (c = *cp++) != 0) {
        vtputc(c);
        ++n;
    }
    vtputc(' ');
    ++n;
    if( bp->fname[0] != 0) {        /* File name.       */
        cp = (BYTE *)"- ";
        while( (c = *cp++) != 0) {
            vtputc(c);
            ++n;
        }
        cp = &bp->fname[0];
        while( (c = *cp++) != 0) {
            vtputc(c);
            ++n;
        }
        vtputc(' ');
        ++n;
    }
    // current character and column
    vtputc('-');
    vtputc(' ');
    c = lgetc(wp->dotp,wp->doto);
    vtputc(hexdigit[(c&0xf0)>>4]);
    vtputc(hexdigit[(c&0xf)]);
    vtputc(' ');
    vtputc('-');
    vtputc((curbp->dotp->flags & L_NL) ? 'N':'-');
    ++n;
//    vtputc((wp->flag&WFMODE)!=0  ? 'M' : '-');
//    vtputc((wp->flag&WFMOVE)!=0  ? 'V' : '-');
//    vtputc((wp->flag&WFFORCE)!=0 ? 'F' : '-');
//    n += 6;
    while( n < term.ncol) {       /* Pad to full width.   */
        vtputc('-');
        ++n;
    }
    return TRUE;
}

/*
 * Send a command to the terminal
 * to move the hardware cursor to row "row"
 * and column "col". The row and column arguments
 * are origin 0. Optimize out random calls.
 * Update "ttrow" and "ttcol".
 */
movecursor(row, col)
{
    if( row!=vtrow || col!=vtcol) {
        vtrow = row;
        vtcol = col;
//        (*term.move)(row, col);
    }
    (*term.move)(row, col);
    return TRUE;
}

/*
 * Erase the message line.
 * This is a special routine because
 * the message line is not considered to be
 * part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed
 * via a call to the flusher.
 */
mlerase()
{
    movecursor(term.nrow, 0);
    (*term.eeol)();
    (*term.flush)();
    mpresf = FALSE;
    return TRUE;
}

/*
 * Ask a yes or no question in
 * the message line. Return either TRUE,
 * FALSE, or ABORT. The ABORT status is returned
 * if the user bumps out of the question with
 * a ^G. Used any time a confirmation is
 * required.
 */
mlyesno(prompt)
BYTE    *prompt;
{
    int    s;
    BYTE  buf[64];

    for( ;;) {
        strcpy((char *)buf, (char *)prompt);
        strcat((char *)buf, " [y/n]? ");
        s = mlreply(buf, buf, sizeof(buf));
        if( s == ABORT)
            return (ABORT);
        if( s != FALSE) {
            if( buf[0]=='y' || buf[0]=='Y')
                return (TRUE);
            if( buf[0]=='n' || buf[0]=='N')
                return (FALSE);
        }
    }
}

/*
 * Write a prompt into the message
 * line, then read back a response. Keep
 * track of the physical position of the cursor.
 * If we are in a keyboard macro throw the prompt
 * away, and return the remembered response. This
 * lets macros run at full speed. The reply is
 * always terminated by a carriage return. Handle
 * erase, kill, and abort keys.
 */
mlreply(prompt,buf,nbuf)
BYTE *prompt;
BYTE *buf;
int  nbuf;
{
	return mlreply1(prompt,buf,nbuf,0);
}
mlgetpw(prompt,buf,nbuf)
BYTE *prompt;
BYTE *buf;
int  nbuf;
{
	return mlreply1(prompt,buf,nbuf,1);
}

mlreply1(prompt, buf, nbuf,passwd)
BYTE *prompt;
BYTE *buf;
int  nbuf;
int  passwd;
{
    register int    cpos;
    register int    i;
    register int    c;

    cpos = 0;

    // kb macro case
    if( kbdmop != NULL) {
        while( (c = *kbdmop++) != '\0')
            buf[cpos++] = c;
        buf[cpos] = 0;
        if( buf[0] == 0)
            return (FALSE);
        return (TRUE);
    }

    // normal case
    mlwrite("%s",prompt);
    for( ;;) {
        c = (*term.getchar)();
        switch (c) {
        case 0x0D:          // Return, end of line
        case 0x0A:          // Newline on UNIX
        case 0x1B:          // escape char
		// the "end of input" case
            if(  c == 0x1B ) pushkey(c);
            buf[cpos++] = 0;
//            if( ! (cpos = do_ml_escapes(buf)) ) return (FALSE );
            if( kbdmip != NULL) {
                if( kbdmip+cpos > &kbdm[NKBDM-3]) {
                    ctrlg(FALSE, 0);
                    (*term.flush)();
                    return (ABORT);
                }
                for( i=0; i<cpos; ++i)
                    *kbdmip++ = buf[i];
            }
            (*term.putchar)('\r');
            (*term.flush)();

            if( buf[0] == 0) return (FALSE);
            return (TRUE);
            
        case 0x11:          /* C-Q, quote       */
            c = (*term.getchar)();
            if( cpos < nbuf-1) {
                buf[cpos++] = c;
                if( c < ' ') {
                    (*term.putchar)('^');
                    c ^= 0x40;
                }
                (*term.putchar)(c);
                (*term.flush)();
            }
            break;
        case 0x07:          /* Bell, abort      */
            (*term.putchar)('^');
            (*term.putchar)('G');
            ctrlg(FALSE, 0);
            (*term.flush)();
            return (ABORT);

        case 0x7F:          /* Rubout, erase    */
        case 0x08:          /* Backspace, erase */
            if( cpos != 0) {
                (*term.putchar)('\b');
                (*term.putchar)(' ');
                (*term.putchar)('\b');
                if( buf[--cpos] < 0x20) {
                    (*term.putchar)('\b');
                    (*term.putchar)(' ');
                    (*term.putchar)('\b');
                }
                (*term.flush)();
            }
            break;

        case 0x15:          /* C-U, kill        */
            while( cpos != 0) {
                (*term.putchar)('\b');
                (*term.putchar)(' ');
                (*term.putchar)('\b');
                if( buf[--cpos] < 0x20) {
                    (*term.putchar)('\b');
                    (*term.putchar)(' ');
                    (*term.putchar)('\b');
                }
            }
            (*term.flush)();
            break;

        default:
            if( cpos < nbuf-1) {
                buf[cpos++] = c;
                if( c < ' ') {
                    (*term.putchar)('^');
                    c ^= 0x40;
                }
				if( passwd ) {
	                (*term.putchar)('*');
				}
				else {
	                (*term.putchar)(c);
				}
                (*term.flush)();
            }
        }
    }
}

int
do_ml_escapes(s)
BYTE *s;
{

    int i = 0;
    int j = 0;
    BYTE *t = s;
    int c0,c1,c2;

    while( *t = *s++ ) {
      if( *t == '\\' ) {
        switch( c0 = *s++ ) { 
        case '\\':   
                *t = '\\';
                break;
        case 'n': 
                *t = '\n';
                break;
        case 'r':
                *t = '\r';
                break;
        case 't':
                *t = '\t';
                break;
        case 'f':
                *t = '\f';
                break;
        case 'b':
                *t = '\b';
                break;
        case 'a':
                *t = '\a';
                break;
        case 'e':
                *t = '\e';
                break;
//        case '[':
//        case '.':
//        case '*':
//        case '$':
//        case ']':
                *t++ = '\\';
                *t = c0;
                break;
        case '0':
        case '1':
        case '2':
        case '3':
                c1 = *s++;
                c2 = *s++;
                if( c1 < '0' || c1 > '7' || c2 < '0' || c2 > '7' ) 
                    return 0;
                c0 = (c0-'0')*64 + (c1-'0')*8 + (c2-'0');
                if( c0 > 255 ) return 0;
                *t = c0;
                break;
        case 'x':
                c1 = *s++;
                c2 = *s++;
                for( i=0;i<16;i++ ) if( c1 == hexdigit[i] ) break;
                c1 = i;
                for( i=0;i<16;i++ ) if( c2 == hexdigit[i] ) break;
                c2 = i;

                if( c1 > 15 || c2 > 15 ) return 0;
                *t = (c1 * 16 + c2);
                break;
        default:
                *t = *s;
        }
      }
      t++;j++;
    }
    return ++j;
}

/*
 * Write out a string.
 * Update the physical cursor position.
 * This assumes that the characters in the
 * string all have width "1"; if this is
 * not the case things will get screwed up
 * a little.
 */
mlputs(s)
BYTE   *s;
{
    register int    c;

    while( (c = *s++) != 0) {
        (*term.putchar)(c);
    }
    return TRUE;
}

/*
 * Write out an integer, in
 * the specified radix. Update the physical
 * cursor position. This will not handle any
 * negative numbers; maybe it should.
 */
mlputi(i, r)
{
    register int    q;

    if( i < 0) {
        i = -i;
        (*term.putchar)('-');
    }
    q = i/r;
    if( q != 0)
        mlputi(q, r);
    (*term.putchar)(hexdigit[i%r]);
    return TRUE;
}

/*
 * do the same except as a long integer.
 */
mlputli(l, r)
long l;
{
    register long q;

    if( l < 0) {
        l = -l;
        (*term.putchar)('-');
    }
    q = l/r;
    if( q != 0)
        mlputli(q, r);
    (*term.putchar)((int)(l%r)+'0');
    return TRUE;
}






