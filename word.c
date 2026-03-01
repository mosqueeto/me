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
    int dotflag;           // was the last char a period?
    char wbuf[NSTRING];    // buffer for current word

    (void)defaultargs(f,n);

    if (rmarg <= lmarg) {     // no fill column set
        mlwrite("Fill column not set");
        return(FALSE);
    }

    if( isblankline( curwp->dotp ) ) return( TRUE );

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
    dotflag = FALSE;

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
            dotflag = (c == '.');   // was it a dot
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
                /* start a new line */
                lnewline();
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
            if (dotflag) {
                linsert(1, ' ');
                ++clength;
            }
            wordlen = 0;
        }
    }
    /* and add a last newline for the end of our new paragraph */
    lnewline();
    backchar(NULL,1);
    return(TRUE);
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
