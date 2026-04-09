/*
 * The routines in this file
 * implement commands that work word at
 * a time. There are all sorts of word mode
 * commands. If I do any sentence and/or paragraph
 * mode commands, they are likely to be put in
 * this file.
 */
#include    <stdio.h>
#include    "ed.h"
#define WORDPRO 1
#define NSTRING 1024

/* Word wrap on n-spaces.
 * Back-over whatever precedes the point on the current line and
 * stop on the first word-break or the beginning of the line.
 * If we reach the beginning of the line, jump back to the end of the
 * word and start a new line.  Otherwise, break the line at the
 * word-break, eat it, and jump back to the end of the word.
 *  NOTE:  This function may leaving trailing blanks.
 * Returns TRUE on success, FALSE on errors.
 */
int wrapword(int f, int n)
{
    int i;

    (void)defaultargs(f,n);
    i = 0;
    while( ! inword() ){    // back over trailing space
        i++; 
        if (! backchar(TRUE, 1)) return(FALSE);
    }

    while( inword() ){  // back over a word
        i++;
        if (backchar(FALSE, 1) == FALSE) return (FALSE);
        if( curwp->doto >= curwp->dotp->used )
            i--;    // don't count EOLs
    }
    forwchar(FALSE,1);
    --i;

    if( i < rmarg ){  /* otherwise the word is the whole line... */
        if (! newline(NULL, 1))
            return(FALSE);
        if (curbp->mode & MDWRAP)
            curwp->dotp->bp->flags |= L_SNL;
    }
    if( lmarg > 0 ) linsert(lmarg,' ');
/*  while( inword()) forwchar(FALSE,1);*/
    return( forwchar(FALSE,i) ) ;

}
                
/*
 * Move the cursor backward by
 * "n" words. All of the details of motion
 * are performed by the "backchar" and "forwchar"
 * routines. Error if you try to move beyond
 * the buffers.
 */
int backword(int f, int n)
{
    if (n < 0)
        return (forwword(f, -n));
    if (backchar(FALSE, 1) == FALSE)
        return (FALSE);
    while (n--) {
        while( ! inword() ){
            if (backchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
        while( inword() ){
            if (backchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
    }
    return (forwchar(FALSE, 1));
}

/*
 * Move the cursor forward by
 * the specified number of words. All of the
 * motion is done by "forwchar". Error if you
 * try and move beyond the buffer's end.
 */
int forwword(int f, int n)
{
    (void)defaultargs(f,n);
    if (n < 0)
        return (backword(f, -n));
    while (n--) {
        while (inword() == FALSE) {
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
        while (inword() != FALSE) {
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
    }
    return (TRUE);
}

/*
 * Move the cursor forward by
 * the specified number of words. As you move,
 * convert any characters to upper case. Error
 * if you try and move beyond the end of the
 * buffer. Bound to "M-U".
 */
int upperword(int f, int n)
{
    register int    c;

    (void)defaultargs(f,n);
    if (n < 0)
        return (FALSE);
    while (n--) {
        while (inword() == FALSE) {
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
        while (inword() != FALSE) {
            c = lgetc(curwp->dotp, curwp->doto);
            if (c>='a' && c<='z') {
                c -= 'a'-'A';
                lputc(curwp->dotp, curwp->doto, c);
                lchange();
            }
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
    }
    return (TRUE);
}

/*
 * Move the cursor forward by
 * the specified number of words. As you move
 * convert characters to lower case. Error if you
 * try and move over the end of the buffer.
 * Bound to "M-L".
 */
int lowerword(int f, int n)
{
    register int    c;

    (void)defaultargs(f,n);
    if (n < 0)
        return (FALSE);
    while (n--) {
        while (inword() == FALSE) {
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
        while (inword() != FALSE) {
            c = lgetc(curwp->dotp, curwp->doto);
            if (c>='A' && c<='Z') {
                c += 'a'-'A';
                lputc(curwp->dotp, curwp->doto, c);
                lchange();
            }
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
    }
    return (TRUE);
}

/*
 * Move the cursor forward by
 * the specified number of words. As you move
 * convert the first character of the word to upper
 * case, and subsequent characters to lower case. Error
 * if you try and move past the end of the buffer.
 * Bound to "M-C".
 */
int capword(int f, int n)
{
    register int    c;

    (void)defaultargs(f,n);
    if (n < 0)
        return (FALSE);
    while (n--) {
        while (inword() == FALSE) {
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        }
        if (inword() != FALSE) {
            c = lgetc(curwp->dotp, curwp->doto);
            if (c>='a' && c<='z') {
                c -= 'a'-'A';
                lputc(curwp->dotp, curwp->doto, c);
                lchange();
            }
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
            while (inword() != FALSE) {
                c = lgetc(curwp->dotp, curwp->doto);
                if (c>='A' && c<='Z') {
                    c += 'a'-'A';
                    lputc(curwp->dotp, curwp->doto, c);
                    lchange();
                }
                if (forwchar(FALSE, 1) == FALSE)
                    return (FALSE);
            }
        }
    }
    return (TRUE);
}

/*
 * Kill forward by "n" words.
 * Remember the location of dot. Move forward
 * by the right number of words. Put dot back where
 * it was and issue the kill command for the
 * right number of characters. Bound to "M-D".
 */
int delfword(int f, int n)
{
    register int    size;
    register LINE   *dotp;
    register int    doto;

    (void)defaultargs(f,n);
    if (n < 0)
        return (FALSE);
    dotp = curwp->dotp;
    doto = curwp->doto;
    size = 0;
    while (n--) {
        while (inword() == FALSE) {
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
            ++size;
        }
        while (inword() != FALSE) {
            if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
            ++size;
        }
    }
    curwp->dotp = dotp;
    curwp->doto = doto;
    return (ldelete(size, TRUE));
}

/*
 * Kill backwards by "n" words.
 * Move backwards by the desired number of
 * words, counting the characters. When dot is
 * finally moved to its resting place, fire off
 * the kill command. Bound to "M-Rubout" and
 * to "M-Backspace".
 */
int delbword(int f, int n)
{
    register int    size;

    (void)defaultargs(f,n);
    if (n < 0)
        return (FALSE);
    if (backchar(FALSE, 1) == FALSE)
        return (FALSE);
    size = 0;
    while (n--) {
        while (inword() == FALSE) {
            if (backchar(FALSE, 1) == FALSE)
                return (FALSE);
            ++size;
        }
        while (inword() != FALSE) {
            if (backchar(FALSE, 1) == FALSE)
                return (FALSE);
            ++size;
        }
    }
    if (forwchar(FALSE, 1) == FALSE)
        return (FALSE);
    return (ldelete(size, TRUE));
}

/*

Return TRUE if the character at dot is a character that is considered to be
part of a word.  The word character list is hard coded.  Should be setable. 

*/
inword()
{
    int c;
    LINE *lp;
    int o;

    lp = curwp->dotp;
    o  = curwp->doto;
    if( o >= llength(lp) )  return (FALSE);
    c = lgetc(lp, o);
#if WORDPRO
    if( isspace(c) ) return ( FALSE );
    else return ( TRUE );
#else
    if (c>='a' && c<='z')
        return (TRUE);
    if (c>='A' && c<='Z')
        return (TRUE);
    if (c>='0' && c<='9')
        return (TRUE);
    if (c=='$' || c=='_')           /* For identifiers  */
        return (TRUE);
    return (FALSE);
#endif
}


#if     WORDPRO
/*
 * Fill the current paragraph according to the left and right margin
 * settings.
 */
int fillpara(int f, int n)       // deFault flag and Numeric argument
{
    int c;                 // current char during scan
    int wordlen;           // length of current word
    int clength;           // position on line during fill
    int i;                 // index during word copy
    int newlength;         // tentative new line length
    int eopflag;           // Are we at the End-Of-Paragraph?
    int firstflag;         // first word? (needs no space)
    LINE *eopline;         // pointer to line just past EOP
    char wbuf[NSTRING];    // buffer for current word
    LINE *save_dotp;       // cursor line before fill
    int   save_doto;       // cursor offset before fill
    int   cursor_nonws;    // non-ws chars from BOP to cursor

    (void)defaultargs(f,n);

    if (rmarg <= lmarg) {     // no fill column set
        mlwrite("Fill column not set");
        return(FALSE);
    }

    if( isblankline( curwp->dotp ) ) return( TRUE );

    // Save cursor position as count of non-ws chars from paragraph start.
    // Used to restore after reflow rather than leaving cursor at EOP.
    save_dotp = curwp->dotp;
    save_doto  = curwp->doto;
    gotobop(FALSE, 1);
    {
        LINE *lp = curwp->dotp;
        int   off = 0;      // start from col 0 of BOP line
        cursor_nonws = 0;
        while (lp != save_dotp || off < save_doto) {
            if (lp == curbp->lines) break;
            if (off >= llength(lp)) { lp = lforw(lp); off = 0; continue; }
            if (lgetc(lp, off) != ' ' && lgetc(lp, off) != '\t') cursor_nonws++;
            off++;
        }
    }
    curwp->dotp = save_dotp;    // restore so fill navigation starts correctly
    curwp->doto = save_doto;

    // record the pointer to the line just past the EOP
    gotobol(0,1);
    gotoeop(FALSE, 1);

    if( lforw(curwp->dotp) == curbp->lines ) { // insert a line
        eopline = curwp->dotp;
        curwp->dotp = lforw(curwp->dotp);
        lnewline();
        curwp->dotp = eopline;
    }
    eopline = lforw(curwp->dotp);

    /* and back top the beginning of the paragraph */
    gotobop(FALSE, 1);

    /* initialize various info */
    curwp->doto = 0;
    clength = curwp->doto;
    wordlen = 0;
    /* scan through lines, filling words */
    firstflag = TRUE;
    eopflag = FALSE;
    while (!eopflag) {
        /* get the next character in the paragraph */
        if( curwp->doto == llength(curwp->dotp) ){
            c = ' ';
            if( (lforw(curwp->dotp) == eopline) ){
                eopflag = TRUE;
            }
            if( curwp->dotp == eopline ) {
                eopflag = TRUE;
                mlwrite("overstepped paragraph end");
            }
            if( curwp->dotp == curbp->lines ) {
                eopflag = TRUE;
                mlwrite("overstepped buffer end");
            }
        }
        else {
            c = lgetc(curwp->dotp, curwp->doto);
        }

        /* and then delete it */
        ldelete(1, FALSE);

        /* if not a separator, just add it in */
        if (c != ' ' && c != '\t') {
            if( wordlen >= NSTRING-1 ) { // word too big -- output it and reset
                lnewline();              // always a new line -- it's BIG
                clength = 0;
                if( lmarg > 0 ) {
                    linsert((lmarg),' ');
                    clength = lmarg;
                }

                for (i=0; i<wordlen; i++) {
                    linsert(1, wbuf[i]);
                    ++clength;
                }
                mlwrite("Unbreakable big word");
                wordlen = 0;
            }
            wbuf[wordlen++] = c;
            continue;
        }

       if( wordlen ){
            // at a word break with a word waiting
            // calculate tentative new length with word added
            newlength = clength + 1 + wordlen;
            if (newlength <= rmarg) {
                // add word to current line
                if( firstflag ) {
                    if( lmarg > 0 ) {
                        linsert(lmarg,' ');
                        clength = lmarg;
                    }
                    firstflag = FALSE;
                } 
                else {
                    linsert(1, ' ');
                    clength++;
                }
            }
            else {
                lnewline();
                if (curbp->mode & MDWRAP)
                    curwp->dotp->bp->flags |= L_SNL;
                clength = 0;
                if( lmarg > 0 ) {
                    linsert((lmarg),' ');
                    clength = lmarg;
                }
            }

            /* and add the word in in either case */
            for (i=0; i<wordlen; i++) {
                linsert(1, wbuf[i]);
                ++clength;
            }
            wordlen = 0;
        }
    }
    /* and add a last newline for the end of our new paragraph */
    lnewline();

    // Restore cursor: go to BOP, walk forward past cursor_nonws non-ws chars
    gotobop(FALSE, 1);
    curwp->doto = 0;
    {
        int rem = cursor_nonws;
        while (rem > 0) {
            if (curwp->doto >= llength(curwp->dotp)) {
                LINE *nxt = lforw(curwp->dotp);
                if (nxt == curbp->lines || isblankline(nxt)) break;
                curwp->dotp = nxt;
                curwp->doto = 0;
                continue;
            }
            if (lgetc(curwp->dotp, curwp->doto) != ' ' &&
                lgetc(curwp->dotp, curwp->doto) != '\t')
                rem--;
            curwp->doto++;     // advance past the character (including the nth non-ws)
            if (rem == 0) break;
        }
    }
    curwp->flag |= WFHARD;
    return(TRUE);
}

/* Fill every paragraph in the buffer.  Useful after opening a file that was
   saved in wrap mode (long lines) and setting the right margin. */
int fillbuf(int f, int n)
{
    int   nfilled = 0;
    int   cursor_nonws = 0;

    (void)defaultargs(f, n);

    if (rmarg <= lmarg) {
        mlwrite("Fill column not set");
        return FALSE;
    }

    // Count non-ws chars from BOB to cursor.  fillpara frees all lines it
    // processes, so a saved line pointer would dangle.  Track position as a
    // character count instead (same technique fillpara uses within a paragraph).
    {
        LINE *lp  = lforw(curbp->lines);
        LINE *cdp = curwp->dotp;
        int   cdo = curwp->doto;
        int   off = 0;
        while (lp != curbp->lines && (lp != cdp || off < cdo)) {
            if (off >= llength(lp)) { lp = lforw(lp); off = 0; continue; }
            if (lgetc(lp, off) != ' ' && lgetc(lp, off) != '\t') cursor_nonws++;
            off++;
        }
    }

    // fill every paragraph from the top
    curwp->dotp = lforw(curbp->lines);
    curwp->doto = 0;

    while (curwp->dotp != curbp->lines) {
        while (curwp->dotp != curbp->lines && isblankline(curwp->dotp))
            curwp->dotp = lforw(curwp->dotp);
        if (curwp->dotp == curbp->lines) break;

        fillpara(FALSE, 1);
        nfilled++;

        gotoeop(FALSE, 1);
        if (curwp->dotp != curbp->lines)
            curwp->dotp = lforw(curwp->dotp);
        curwp->doto = 0;
    }

    // Restore cursor: walk from BOB counting cursor_nonws non-ws chars
    curwp->dotp = lforw(curbp->lines);
    curwp->doto = 0;
    {
        int rem = cursor_nonws;
        while (rem > 0 && curwp->dotp != curbp->lines) {
            if (curwp->doto >= llength(curwp->dotp)) {
                curwp->dotp = lforw(curwp->dotp);
                curwp->doto = 0;
                continue;
            }
            if (lgetc(curwp->dotp, curwp->doto) != ' ' &&
                lgetc(curwp->dotp, curwp->doto) != '\t')
                rem--;
            curwp->doto++;
            if (rem == 0) break;
        }
    }

    curwp->flag |= WFHARD;
    mlwrite("[filled %d paragraph%s]", nfilled, nfilled == 1 ? "" : "s");
    return TRUE;
}

int toggle_ww(int f, int n)     // toggle word-wrap (WP) mode for current buffer
{
    (void)defaultargs(f, n);
    if (curbp->mode & MDWRAP) {
        curbp->mode &= ~MDWRAP;
        mlwrite("[wrap off]");
    } else {
        curbp->mode |= MDWRAP;
        mlwrite("[wrap on]");
    }
    curwp->flag |= WFMODE;
    return TRUE;
}

/* Split lp at position brk (a space).  The space is consumed and lp is
   trimmed to [0..brk-1].  The tail (chars after brk) is either prepended
   to the existing continuation line (when lp had L_SNL) or used to create
   a new continuation line (when lp was a hard-ended line).  Saves and
   restores the window cursor. */
static void wrap_split(LINE *lp, int brk)
{
    int len      = llength(lp);
    int tail_len = len - brk - 1;
    int had_snl  = lp->flags & L_SNL;
    int i;

    // Copy tail before modifying lp
    char tail[NSTRING];
    for (i = 0; i < tail_len && i < NSTRING-1; i++)
        tail[i] = lp->text[brk + 1 + i];

    LINE *save_dotp = curwp->dotp;
    int   save_doto = curwp->doto;

    // Trim lp: delete the space at brk plus everything after it
    curwp->dotp = lp;
    curwp->doto = brk;
    ldelete(len - brk, FALSE);
    if (had_snl || tail_len > 0)
        lp->flags |= L_SNL;

    if (had_snl) {
        // Prepend tail to the existing continuation line
        LINE *next    = lforw(lp);
        int next_orig = llength(next);
        curwp->dotp   = next;
        curwp->doto   = 0;
        for (i = 0; i < tail_len; i++)
            linsert(1, tail[i]);
        if (tail_len > 0 && next_orig > 0)
            linsert(1, ' ');    // separator between tail and existing content
    } else if (tail_len > 0) {
        // Create a new continuation line with the tail
        curwp->dotp = lp;
        curwp->doto = brk;     // EOL of lp (lp->used == brk now)
        lnewline();            // new empty line; cursor on it at offset 0
        for (i = 0; i < tail_len; i++)
            linsert(1, tail[i]);
        // new line flags: L_NL only (no L_SNL — it's the new paragraph end)
    }

    curwp->dotp = save_dotp;
    curwp->doto = save_doto;
}

/* After inserting a space that pushes the line over rmarg in MDWRAP mode,
   split the line at the rightmost space at or after the cursor so the cursor
   never moves.  Cascades into continuation lines if needed. */
void wrap_insert(void)
{
    LINE *lp     = curwp->dotp;
    int   cursor = curwp->doto;
    int   len    = llength(lp);
    int   end    = (rmarg < len - 1) ? rmarg : len - 1;
    int   brk    = -1;
    int   i;

    for (i = end; i >= cursor; i--) {
        if (lgetc(lp, i) == ' ') { brk = i; break; }
    }
    if (brk < 0) return;   // no valid break at or after cursor; leave over-long

    wrap_split(lp, brk);

    // Cascade: if the continuation line now overflows, split it too.
    // No cursor constraint on lines below.
    LINE *cont = lforw(lp);
    while (cont != curbp->lines && llength(cont) > rmarg) {
        int len2 = llength(cont);
        int end2 = (rmarg < len2 - 1) ? rmarg : len2 - 1;
        int brk2 = -1;
        for (i = end2; i >= 0; i--) {
            if (lgetc(cont, i) == ' ') { brk2 = i; break; }
        }
        if (brk2 < 0) break;
        wrap_split(cont, brk2);
        cont = lforw(cont);
    }

    curwp->flag |= WFHARD;
}

/* After deleting a character in MDWRAP mode, pull words up from continuation
   lines as space permits, then cascade to do the same for subsequent lines.
   Cursor never moves. */
void wrap_delete(void)
{
    if (!((curbp->mode & MDWRAP) && rmarg > 0)) return;

    LINE *cur    = curwp->dotp;
    int   cursor = curwp->doto;

    // Walk continuation lines: fill cur from nxt, then advance to the next pair.
    while (cur->flags & L_SNL) {
        LINE *nxt = lforw(cur);
        if (nxt == curbp->lines) break;

        // Pull words from nxt into cur as long as they fit.
        for (;;) {
            int cur_len = llength(cur);
            int nxt_len = llength(nxt);

            // Find first word on nxt (skip any leading spaces)
            int ws = 0;
            while (ws < nxt_len && lgetc(nxt, ws) == ' ') ws++;
            int we = ws;
            while (we < nxt_len && lgetc(nxt, we) != ' ') we++;
            int word_len = we - ws;
            if (word_len == 0) break;

            if (cur_len + 1 + word_len > rmarg) break;

            // Pull: append ' ' + word to cur
            LINE *save_dotp = curwp->dotp;

            curwp->dotp = cur;
            curwp->doto = cur_len;
            linsert(1, ' ');
            for (int k = ws; k < we; k++)
                linsert(1, lgetc(nxt, k));

            // Remove word + leading spaces + trailing separator from nxt
            int del = we;
            if (del < nxt_len && lgetc(nxt, del) == ' ')
                del++;
            curwp->dotp = nxt;
            curwp->doto = 0;
            ldelete(del, FALSE);

            curwp->dotp = save_dotp;
            curwp->doto = cursor;

            if (llength(nxt) == 0) {
                int snl_nxt = nxt->flags & L_SNL;
                lfree(nxt);
                cur->flags = (cur->flags & ~L_SNL) | snl_nxt;
                break;  // nxt gone; outer loop will advance cur
            }
        }

        cur = lforw(cur);   // cascade: now fill cur (old nxt) from its continuation
    }

    curwp->flag |= WFHARD;
}

int killpara(int f, int n)  /* delete n paragraphs starting with the current one */
{
        register int status;    /* returned status of functions */

          (void)defaultargs(f,n);
        while (n--) {           /* for each paragraph to delete */

                /* mark out the end and beginning of the para to delete */
                gotoeop(FALSE, 1);

                /* set the mark here */
                curwp->markp = curwp->dotp;
                curwp->marko = curwp->doto;

                /* go to the beginning of the paragraph */
                gotobop(FALSE, 1);
                curwp->doto = 0;      /* force us to the beginning of line */
        
                /* and delete it */
                if ((status = killregion(FALSE, 1)) != TRUE)
                        return(status);

                /* and clean up the 2 extra lines */
                ldelete(2L, TRUE);
        }
        return(TRUE);
}


/*      wordcount:      count the # of words in the marked region,
                        along with average word sizes, # of chars, etc,
                        and report on them.                     */

int wordcount(int f, int n)       /* ignored numeric arguments */
{
        register LINE *lp;      /* current line to scan */
        register int offset;    /* current char to scan */
        long size;              /* size of region left to count */
        register int ch;        /* current character to scan */
        register int wordflag;  /* are we in a word now? */
        register int lastword;  /* were we just in a word? */
        long nwords;            /* total # of words */
        long nchars;            /* total number of chars */
        int nlines;             /* total number of lines in region */
        int avgch;              /* average number of chars/word */
        int status;             /* status return code */
          REGION region;          /* region to look at */

        (void)defaultargs(f,n);

        /* make sure we have a region to count */
        if ((status = getregion(&region)) != TRUE)
                return(status);
        lp = region.lines;
        offset = region.offset;
        size = region.size;

        /* count up things */
        lastword = FALSE;
        nchars = 0L;
        nwords = 0L;
        nlines = 0;
        while (size--) {

                /* get the current character */
                if (offset == llength(lp)) {    /* end of line */
                        ch = '\n';
                        lp = lforw(lp);
                        offset = 0;
                        ++nlines;
                } else {
                        ch = lgetc(lp, offset);
                        ++offset;
                }

                /* and tabulate it */
                wordflag = ((ch >= 'a' && ch <= 'z') ||
                            (ch >= 'A' && ch <= 'Z') ||
                            (ch >= '0' && ch <= '9') ||
                            (ch == '$' || ch == '_'));
                if (wordflag == TRUE && lastword == FALSE)
                        ++nwords;
                lastword = wordflag;
                ++nchars;
        }

        /* and report on the info */
        if (nwords > 0L)
                avgch = (int)((100L * nchars) / nwords);
        else
                avgch = 0;

        mlwrite("Words %D Chars %D Lines %d Avg chars/word %f",
                nwords, nchars, nlines + 1, avgch);
        return(TRUE);
}
#endif
