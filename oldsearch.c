/* @(#)search.c	1.2
 * The functions in this file implement commands that search in the forward
 * and backward directions.  There are no special characters in the search
 * strings.  Probably should have a regular expression search, or something
 * like that.
 *
 * Aug. 1986 John M. Gamble:
 *      Made forward and reverse search use the same scan routine.
 *
 *      Added a limited number of regular expressions - 'any',
 *      'character class', 'closure', 'beginning of line', and
 *      'end of line'.
 *
 *      Replacement metacharacters will have to wait for a re-write of
 *      the replaces function, and a new variation of ldelete().
 *
 *      For those curious as to my references, i made use of
 *      Kernighan & Plauger's "Software Tools."
 *      I deliberately did not look at any published grep or editor
 *      source (aside from this one) for inspiration.  I did make use of
 *      Allen Hollub's bitmap routines as published in Doctor Dobb's Journal,
 *      June, 1985 and modified them for the limited needs of character class
 *      matching.  Any inefficiences, bugs, stupid coding examples, etc.,
 *      are therefore my own responsibility.
 */

#include    <stdio.h>
#include    <string.h>
#include    <stdlib.h>
#include    "ed.h"
#include    "search.h"

static int amatch();
static int readpattern();
static int replaces();
static int nextch();
static int mcstr();
static int mceq();
static int cclmake();
static int biteq();
static BITMAP clearbits();
static int setbit();
static int freebits();


/*
 * Reversed pattern array.
 */
char    tap[NPAT];

unsigned char	rpat[NPAT];
/*
 * The variable magical determines if there are actual
 * metacharacters in the string - if not, then we don't
 * have to use the slower MAGIC mode search functions.
 *
 * The variable mclen holds the length of the matched
 * string - used by the replace functions.
 *
 * The arrays mcpat and tapcm hold the MC and reversed
 * MC search structures.
 */
short int   magical = FALSE;
int         mclen = 0;
MC          mcpat[NPAT];
MC          tapcm[NPAT];

/*
 * forwsearch -- Search forward.  Get a search string from the user, and
 *      search, beginning at ".", for the string.  If found, reset the "."
 *      to be just after the match string, and (perhaps) repaint the display.
 */

int
forwsearch(f, n)
{
    register int status = TRUE;

    if( n == 0 ) n = 1;
    if( n < 0 ) return ( backsearch(f, -n) );

    /* Ask the user for the text of a pattern.  If the
     * response is TRUE (responses other than FALSE are
     * possible), search for the pattern.
     */
    if( (status = readpattern("search", &pat[0], TRUE)) == TRUE ){
        do {
            if( (magical && curwp->bufp->mode & MDMAGIC) != 0 ){
                status = mcscanner(&mcpat[0], FORWARD, PTEND);
            }
            else {
                status = scanner(&pat[0], FORWARD, PTEND);
            }
        } while( (--n > 0) && status );

        /* ...and complain if not there.
         */
        if( status == FALSE ) {
            mlwrite("Not found");
        }
    }
    return( status );
}

/*
 * forwhunt -- Search forward for a previously acquired search string,
 *      beginning at ".".  If found, reset the "." to be just after
 *      the match string, and (perhaps) repaint the display.
 */
int
forwhunt(f, n)
{
    register int status = TRUE;

    if( n == 0 ) n = 1;
    if( n < 0 )  return( backhunt(f, -n) );

    /* Make sure a pattern exists, or that we didn't switch
     * into MAGIC mode until after we entered the pattern.
     */
    if( pat[0] == '\0' ){
        mlwrite("No pattern set");
        return FALSE;
    }

    if( (curwp->bufp->mode & MDMAGIC) != 0 && mcpat[0].mc_type == MCNIL ){
        if( !mcstr() ){
            return FALSE;
        }
    }

    /* Search for the pattern...
     */
    do {
        if( (magical && curwp->bufp->mode & MDMAGIC) != 0 ){
            status = mcscanner(&mcpat[0], FORWARD, PTEND);
        }
        else {
            status = scanner(&pat[0], FORWARD, PTEND);
        }
    } while( (--n > 0) && status );

    /* ...and complain if not there.
     */
    if( status == FALSE ) {
        mlwrite("Not found");
    }

    return( status );
}

/*
 * backsearch -- Reverse search.  Get a search string from the user, and
 *      search, starting at "." and proceeding toward the front of the buffer.
 *      If found "." is left pointing at the first character of the pattern
 *      (the last character that was matched).
 */
int
backsearch(f, n)
{
    register int status = TRUE;

    if( n == 0 ) n = 1;
    if( n < 0 ) return( forwsearch(f, -n) );

    /* Ask the user for the text of a pattern.  If the
     * response is TRUE (responses other than FALSE are
     * possible), search for the pattern.
     */
    if( (status = readpattern("reverse search",&pat[0],TRUE)) == TRUE ){
        do {
            if( (magical && curwp->bufp->mode & MDMAGIC) != 0 ){
                status = mcscanner(&tapcm[0], REVERSE, PTBEG);
            }
            else {
                status = scanner(&tap[0], REVERSE, PTBEG);
            }
        } while( (--n > 0) && status );

        /* ...and complain if not there.
         */
        if( status == FALSE ){
            mlwrite("Not found");
        }
    }
    return( status );
}

/*
 * backhunt -- Reverse search for a previously acquired search string,
 *      starting at "." and proceeding toward the front of the buffer.
 *      If found "." is left pointing at the first character of the pattern
 *      (the last character that was matched).
 */
int
backhunt(f, n)
{
    register int    status = TRUE;

    if( n == 0 ) n = 1;
    if( n < 0 )  return( forwhunt(f, -n) );

    /* Make sure a pattern exists, or that we didn't switch
     * into MAGIC mode until after we entered the pattern.
     */
    if( tap[0] == '\0' ) {
        mlwrite("No pattern set");
        return FALSE;
    }

    if( (curwp->bufp->mode & MDMAGIC) != 0 && tapcm[0].mc_type == MCNIL ){
        if( !mcstr() ){
            return FALSE;
        }
    }

    // search for it...
    do {
        if( (magical && curwp->bufp->mode & MDMAGIC) != 0 ){
            status = mcscanner(&tapcm[0], REVERSE, PTBEG);
        }
        else {
            status = scanner(&tap[0], REVERSE, PTBEG);
        }
    } while( (--n > 0) && status );

    /* ...and complain if not there.
     */
    if( status == FALSE ){
        mlwrite("Not found");
    }

    return( status );
}

/*
 * mcscanner -- Search for a meta-pattern in either direction.
 */
int
mcscanner(mcpatrn, direct, beg_or_end)
MC      *mcpatrn;   // pointer into pattern
int     direct;     // which way to go.
int     beg_or_end; // put point at beginning or end of pattern.
{
    LINE *lastline;    // last line position during scan
    int lastoff;       // position within last line
    LINE *curline;     // current line during scan
    int curoff;        // position within current line
    int c;             // (dummy) char at current position

    // If we are going in reverse, then the 'end' is actually
    // the beginning of the pattern.
    beg_or_end ^= direct;

    // Setup local scan pointers to global ".".
    curline = curwp->dotp;
    curoff  = curwp->doto;

    // Scan each character until we hit the head link record.
    while (!boundry(curline, curoff, direct))
    {
        /* Save the current position in case we need to
         * restore it on a match, and initialize mclen to
         * zero in case we are doing a search for replacement.
         */
        lastline = curline;
        lastoff = curoff;
        mclen = 0;

        if (amatch(mcpatrn, direct, &curline, &curoff)) {
            // SUCCESSFULL MATCH
            // reset the global "." pointers.
            if( beg_or_end == PTEND ){  // at end of string
                curwp->dotp = curline;
                curwp->doto = curoff;
            }
            else {  // at beginning of string
                curwp->dotp = lastline;
                curwp->doto = lastoff;
            }

            curwp->flag |= WFMOVE; // flag that we have moved
            return TRUE;
        }

        // Advance the cursor.
        (void) nextch(&curline, &curoff, direct);
    }
    return FALSE;   // No match
}

/*
 * amatch -- Search for a meta-pattern in either direction.  Based on the
 *      recursive routine amatch() (for "anchored match") in
 *      Kernighan & Plauger's "Software Tools".
 */
static int
amatch(mcptr, direct, pcwline, pcwoff)
register MC *mcptr;     // string to scan for
int         direct;     // which way to go.
LINE        **pcwline;  // current line during scan
int         *pcwoff;    // position within current line
{

    int     c;          //character at current position
    LINE    *curline;   // current line during scan
    int     curoff;     // position within current line
    int     nchars;

    /* Set up local scan pointers to ".", and get
     * the current character.  Then loop around
     * the pattern pointer until success or failure.
     */
    curline = *pcwline;
    curoff = *pcwoff;

    /* The beginning-of-line and end-of-line metacharacters
     * do not compare against characters, they compare
     * against positions.
     * BOL is guaranteed to be at the start of the pattern
     * for forward searches, and at the end of the pattern
     * for reverse searches.  The reverse is true for EOL.
     * So, for a start, we check for them on entry.
     */
    if( mcptr->mc_type == BOL ) {
        if (curoff != 0) return FALSE;
        mcptr++;
    }

    if( mcptr->mc_type == EOL ) {
        if( curoff != llength(curline) ) return FALSE;
        mcptr++;
    }

    while( mcptr->mc_type != MCNIL ) {
        c = nextch(&curline, &curoff, direct);

        if( mcptr->mc_type & CLOSURE ) {
            /* Try to match as many characters as possible
             * against the current meta-character.  A
             * newline never matches a closure.
             */
            nchars = 0;
            while( c != '\n' && mceq(c, mcptr) ) {
                c = nextch(&curline, &curoff, direct);
                nchars++;
            }

            /* We are now at the character that made us
             * fail.  Try to match the rest of the pattern.
             * Shrink the closure by one for each failure.
             * Since closure matches *zero* or more occurences
             * of a pattern, a match may start even if the
             * previous loop matched no characters.
             */
            mcptr++;

            for( ;; ){
                c = nextch(&curline, &curoff, direct ^ REVERSE);
                if( amatch(mcptr, direct, &curline, &curoff) ){
                    mclen += nchars;
                    goto success;
                }
                if( nchars-- == 0) return FALSE;
            }
        }
        else {  // Not a closure.
            /* The only way we'd get a BOL metacharacter
             * at this point is at the end of the reversed pattern.
             * The only way we'd get an EOL metacharacter
             * here is at the end of a regular pattern.
             * So if we match one or the other, and are at
             * the appropriate position, we are guaranteed success
             * (since the next pattern character has to be MCNIL).
             * Before we report success, however, we back up by
             * one character, so as to leave the cursor in the
             * correct position.  For example, a search for ")$"
             * will leave the cursor at the end of the line, while
             * a search for ")<NL>" will leave the cursor at the
             * beginning of the next line.  This follows the
             * notion that the meta-character '$' (and likewise
             * '^') match positions, not characters.
             */
            if( mcptr->mc_type == BOL ){
                if( curoff == llength(curline) ){
                    c = nextch(&curline, &curoff,  direct ^ REVERSE);
                    goto success;
                }
                else {
                    return FALSE;
                }
            }

            if( mcptr->mc_type == EOL ){
                if( curoff == 0 ){
                    c = nextch(&curline, &curoff, direct ^ REVERSE);
                    goto success;
                }
                else {
                    return FALSE;
                }
            }

            /* Neither BOL nor EOL, so go through
             * the meta-character equal function.
             */
            if( !mceq(c, mcptr) ) return FALSE;
        }

        /* Increment the length counter and
         * advance the pattern pointer.
         */
        mclen++;
        mcptr++;
    } // End of mcptr loop.

success:    // Reset the "." pointers.
    *pcwline = curline;
    *pcwoff  = curoff;

    return TRUE;
}

/*
 * scanner -- Search for a pattern in either direction.
 */
int
scanner(patrn, direct, beg_or_end)
char    *patrn;     // string to scan for
int     direct;     // which way to go
int     beg_or_end; // put point at beginning or end of pattern
{
    int  c;         // character at current position
    char *patptr;   // pointer into pattern
    LINE *lastline; // last line position during scan
    int  lastoff;   // position within last line
    LINE *curline;  // current line during scan
    int  curoff;    // position within current line
    LINE *matchline;// current line during matching
    int  matchoff;  // position in matching line

    /* If we are going in reverse, then the 'end' is actually
     * the beginning of the pattern.  Toggle it.
     */
    beg_or_end ^= direct;

    /* Setup local scan pointers to global ".".
     */
    curline = curwp->dotp;
    curoff = curwp->doto;

    // Scan each character until we hit the head link record.

    while( !boundry(curline, curoff, direct) ) {
        // Save the current position in case we need to
        // restore it on a match.
        lastline = curline;
        lastoff = curoff;

        // Get the character resolving newlines, and
        // test it against first char in pattern.

        c = nextch(&curline, &curoff, direct);

        if( eq(c, patrn[0]) ) {    // if we find it..
            matchline = curline;
            matchoff = curoff;
            patptr = &patrn[0];

            // Scan through the pattern for a match.
            while( *++patptr != '\0' ) {
                c = nextch(&matchline, &matchoff, direct);

                if( !eq(c, *patptr))
                    goto fail;
            }

            // SUCCESSFULL MATCH
            // reset the global "." pointers

            if( beg_or_end == PTEND ) {    // at end of string
                curwp->dotp = matchline;
                curwp->doto = matchoff;
            }
            else {  // at beginning of string
                curwp->dotp = lastline;
                curwp->doto = lastoff;
            }

            curwp->flag |= WFMOVE; // cursor has moved.
            return TRUE;
        }
fail:;   // continue to search
    }
    return FALSE;   // NO MATCH
}

/*
 * eq -- Compare two characters.  The "bc" comes from the buffer, "pc"
 *      from the pattern.  If we are not in EXACT mode, fold out the case.
 */
int
eq(bc, pc)
register int    bc;
register int    pc;
{
    if( (curwp->bufp->mode & MDEXACT) == 0 ){
        if( islower(bc)) bc ^= 0x20;
        if( islower(pc)) pc ^= 0x20;
    }
    return( (bc & 0xff) == (pc & 0xff) );
}


/*
 * rvstrcpy -- Reverse string copy.
 */
void
rvstrcpy(rvstr, str)
register char   *rvstr, *str;
{
    register int i;
    str += (i = strlen(str));
    while( i-- > 0) *rvstr++ = *--str;
    *rvstr = '\0';
}

/*
 * readpattern -- Read a pattern.  Stash it in apat.  If it is the
 *      search string, create the reverse pattern and the magic
 *      pattern, assuming we are in MAGIC mode (and defined that way).
 *      Apat is not updated if the user types in an empty line.  If
 *      the user typed an empty line, and there is no old pattern, it is
 *      an error.  Display the old pattern, in the style of Jeff Lomicka.
 *      There is some do-it-yourself control expansion.  Change to using
 *      <META> to delimit the end-of-pattern to allow <NL>s in the search
 *      string. 
 */
static int
readpattern(prompt, apat, srch)
char    *prompt;
char    apat[];
int     srch;
{
    int status;
    char tpat[NPAT+20];

    strcpy(tpat, prompt);   // copy prompt to output string
    strcat(tpat, " [");     // build new prompt string

    expandp(&apat[0], &tpat[strlen(tpat)], NPAT/2); // add old pattern
    strcat(tpat, "]<META>: ");

    /* Read a pattern.  Either we get one,
     * or we just get the META charater, and use the previous pattern.
     * Then, if it's the search string, make a reversed pattern.
     * *Then*, make the meta-pattern, if we are defined that way.
     */
    if( (status = mlreply(tpat, tpat, NPAT)) == TRUE ){
        strcpy(apat, tpat);
        if( srch ) { // If we are doing the search string
            rvstrcpy(tap, apat);
            /* Only make the meta-pattern if in magic mode,
             * since the pattern in question might have an
             * invalid meta combination.
             */
            if( (curwp->bufp->mode & MDMAGIC) == 0 ) {
                mcpat[0].mc_type = tapcm[0].mc_type = MCNIL;
            }
            else {
                status = mcstr();
            }
        }
    }
    else if( status == FALSE && apat[0] != 0 ){  // Old pattern
        status = TRUE;
    }

    return(status);
}

/*
 * sreplace -- Search and replace.
 */
int
sreplace(f, n)
int f;
int n;
{
    return( replaces(FALSE, f, n) );
}

/*
 * qreplace -- search and replace with query.
 */
int
qreplace(f, n)
int f;
int n;
{
    return( replaces(TRUE, f, n) );
}

/*
 * replaces -- Search for a string and replace it with another
 *      string.  Query might be enabled (according to kind).
 */
static int
replaces(kind, f, n)
int     kind;   // Query enabled flag
int     f;      // default flag
int     n;      // # of repetitions wanted
{
    int i;
    int status;
    int slength;    // length of search and replace strings
    int rlength;   
    register int numsub;    // number of substitutions
    register int nummatch;  // number of found matches
    int nlflag;         // last char of search string a <NL>?
    int nlrepl;         // was a replace done on the last line?
    char tmpc;          // temporary character
    char c;             // input char for query
    char tpat[NPAT];    // temporary to hold search pattern
    LINE *origline;     // original "." position
    int origoff;        // and offset (for . query option)
    LINE *lastline;     // position of last replace and
    int lastoff;        // offset (for 'u' query option)
    char linebuf[1024];	// for mlwriting

    /* Check for negative repetitions.
     */
    if( f && n < 0 ) {
        return(FALSE);
    }

    /* Ask the user for the text of a pattern.
     */
    if( (status = readpattern(
        (kind == FALSE ? "Replace" : "Query replace"), &pat[0], TRUE))
                                != TRUE )
    {
        return(status);
    }

    // get replacement string.
    if( (status = readpattern("with", &rpat[0], FALSE)) == ABORT ) {
        return(status);
    }

    // Find the lengths of the strings.
    slength = strlen((char *)&pat[0]);
    rlength = strlen((char *)&rpat[0]);

    /* Set up flags so we can make sure not to do a recursive
     * replace on the last line.
     */
    nlflag = (pat[slength - 1] == '\n');
    nlrepl = FALSE;

    if( kind ){
        // Build query replace question string.
        strcpy(tpat, "Replace '");
        expandp(&pat[0], &tpat[strlen(tpat)], NPAT/3);
        strcat(tpat, "' with '");
        expandp(&rpat[0], &tpat[strlen(tpat)], NPAT/3);
        strcat(tpat, "'? ");

        // Initialize last replaced pointers.
        lastline = NULL;
        lastoff = 0;
    }

    /* Save original . position, init the number of matches and
     * substitutions, and scan through the file.
     */
    origline = curwp->dotp;
    origoff = curwp->doto;
    numsub = 0;
    nummatch = 0;

    while(  (f == FALSE || n > nummatch) &&
        (nlflag == FALSE || nlrepl == FALSE) )
    {
        /* Search for the pattern.
         * If we search with a regular expression, also save
         * the true length of matched string.
         */
        if( (magical && curwp->bufp->mode & MDMAGIC) != 0 ) {
            if( !mcscanner(&mcpat[0], FORWARD, PTBEG))
                break;
            slength = mclen;
        }
        else {
            if( !scanner(&pat[0], FORWARD, PTBEG))
                break;      /* all done */
        }

        ++nummatch;     /* Increment # of matches */

        // last line?
        nlrepl = (lforw(curwp->dotp) == curwp->bufp->lines);

        // a query?
        if( kind ) {
            // Get the query.
pprompt:    sprintf(linebuf,&tpat[0], &pat[0], &rpat[0]);
			mlwrite(linebuf);
qprompt:
            update();      // show the proposed change
			c = (*term.getchar)();
            mlwrite("");   // and clear it...

            switch ( c )
            {
                case 'y':       // yes, do substitute
                case ' ':
                    break;

                case 'n':       // no, onward
                    forwchar(FALSE, 1);
                    continue;

                case '!':       // yes/stop asking
                    kind = FALSE;
                    break;

                case 'u':       // undo last and re-prompt
                    // Restore old position.
                    if( lastline == NULL ) {
                        // There is nothing to undo
						(*term.beep)();
                        goto qprompt;
                    }
                    curwp->dotp = lastline;
                    curwp->doto = lastoff;
                    lastline = NULL;
                    lastoff = 0;

                    // Delete the new string
                    backchar(FALSE, rlength);
                    if( !ldelete(rlength, FALSE) ) {
                        mlwrite("%%ERROR while deleting");
						(*term.beep)();
                        return(FALSE);
                    }

                    // put in the old one.
                    for( i = 0; i < slength; i++ ) {
                        tmpc = pat[i];
                        status = (tmpc == '\n'?
                            lnewline():
                            linsert(1, tmpc));

                        // Insertion error?
                        if( !status ) {
                            mlwrite("%%Out of memory while inserting");
							(*term.beep)();
                            return(FALSE);
                        }
                    }

                    // Record one less substitution,
                    // backup, and reprompt.

                    --numsub;
                    backchar(FALSE, slength);
                    goto pprompt;

                case '.':       // abort! and return
                    // restore old position
                    curwp->dotp = origline;
                    curwp->doto = origoff;
                    curwp->flag |= WFMOVE;

                case 0x07:      // abort! and stay
                    mlwrite("Aborted!");
                    return(FALSE);

                default:    // bitch and beep
					(*term.beep)();

                case '?':       // help
                    mlwrite(
"(Y)es, (N)o, (!)Do rest, (U)ndo last, (^G)Abort, (.)Abort back, (?)Help: ");
                    goto qprompt;

            } // switch
        }  // "if kind"

         // Delete the sucker.
        if( !ldelete(slength, FALSE) ) {
            mlwrite("%%ERROR while deleteing");
            return(FALSE);
        }

        // insert its replacement
        for( i = 0; i < rlength; i++ ) {
            tmpc = rpat[i];
            status = (tmpc == '\n'? lnewline(): linsert(1, tmpc));
            // Insertion error?
            if( !status ) {
                mlwrite("%%Out of memory while inserting");
                return(FALSE);
            }
         }

        // Save position for undo
        if( kind ) {
            lastline = curwp->dotp;
            lastoff = curwp->doto;
        }
        numsub++;       // increment # of substitutions
    }

    // report the results.
    mlwrite("%d substitutions", numsub);
    return(TRUE);
}

/*
 * expandp -- Expand control key sequences for output.
 */
int
expandp(srcstr, deststr, maxlength)
char *srcstr;   // string to expand
char *deststr;  // destination of expanded string
int maxlength;  // maximum chars in destination
{
    char c;     // current char to translate

    while( (c = *srcstr++) != 0 ) {
        if( c == '\n') {
            *deststr++ = '<';
            *deststr++ = 'N';
            *deststr++ = 'L';
            *deststr++ = '>';
            maxlength -= 4;
        }
        else if( c < 0x20 || c == 0x7f) { // control character
            *deststr++ = '^';
            *deststr++ = c ^ 0x40;
            maxlength -= 2;
        }
        else if( c == '%' ) {
            *deststr++ = '%';
            *deststr++ = '%';
            maxlength -= 2;
        }
        else {
            *deststr++ = c;
            maxlength--;
        }

        if( maxlength < 4 ) {
            *deststr++ = '$';
            *deststr = '\0';
            return(FALSE);
        }
    }
    *deststr = '\0';
    return(TRUE);
}

/*
 * boundry -- Return information depending on whether we may search no
 *      further.  Beginning of file and end of file are the obvious
 *      cases, but we may want to add further optional boundry restrictions
 *      in future.  At the moment, just return TRUE or
 *      FALSE depending on if a boundry is hit (ouch).
 */
int
boundry(curline, curoff, dir)
LINE    *curline;
int     curoff, dir;
{
    int    border;

    if( dir == FORWARD ) {
        border = (curoff == llength(curline)) &&
            (lforw(curline) == curbp->lines);
    }
    else  {
        border = (curoff == 0) &&
            (lback(curline) == curbp->lines);
    }
    return (border);
}

/*
 * nextch -- retrieve the next/previous character in the buffer,
 *      and advance/retreat the point.
 *      The order in which this is done is significant, and depends
 *      upon the direction of the search.  Forward searches look at
 *      the current character and move, reverse searches move and
 *      look at the character.
 */
static int
nextch(pcurline, pcuroff, dir)
LINE    **pcurline;
int     *pcuroff;
int     dir;
{
    LINE   *curline;
    int    curoff;
    int    c;

    curline = *pcurline;
    curoff = *pcuroff;

    if( dir == FORWARD ) {
        if( curoff == llength(curline)) {   // EOL?
            curline = lforw(curline);       // skip to next line
            curoff = 0;
            c = '\n';                       // and return a <NL>
        }
        else {
            c = lgetc(curline, curoff++);   // get the char
        }
    }
    else {   // Reverse.
        if( curoff == 0 ) {
            curline = lback(curline);
            curoff = llength(curline);
            c = '\n';
        }
        else {
            c = lgetc(curline, --curoff);
        }
    }
    *pcurline = curline;
    *pcuroff = curoff;

    return (c);
}

/*
 * mcstr -- Set up the 'magic' array.  The closure symbol is taken as
 *      a literal character when (1) it is the first character in the
 *      pattern, and (2) when preceded by a symbol that does not allow
 *      closure, such as a newline, beginning of line symbol, or another
 *      closure symbol.
 *
 *      Coding comment (jmg):  yes, i know i have gotos that are, strictly
 *      speaking, unnecessary.  But right now we are so cramped for
 *      code space that i will grab what i can in order to remain
 *      within the 64K limit.  C compilers actually do very little
 *      in the way of optimizing - they expect you to do that.
 */
static int
mcstr( )
{
    MC      *mcptr, *rtpcm;
    BYTE    *patptr;
    int     mj;
    int     pchr;
    int     status = TRUE;
    int     does_closure = FALSE;

    // If we had metacharacters in the MC array previously,
    // free up any bitmaps that may have been allocated.

    if( magical) freebits();

    magical = FALSE;
    mj = 0;
    mcptr = &mcpat[0];
    patptr = &pat[0];

    while( (pchr = *patptr) && status ) {
        switch ( pchr )
        {
            case MC_CCL:
                status = cclmake(&patptr, mcptr);
                magical = TRUE;
                does_closure = TRUE;
                break;
            case MC_BOL:
                if( mj != 0) goto litcase;
                mcptr->mc_type = BOL;
                magical = TRUE;
                does_closure = FALSE;
                break;
            case MC_EOL:
                if( *(patptr + 1) != '\0') goto litcase;
                mcptr->mc_type = EOL;
                magical = TRUE;
                does_closure = FALSE;
                break;
            case MC_ANY:
                mcptr->mc_type = ANY;
                magical = TRUE;
                does_closure = TRUE;
                break;
            case MC_CLOSURE:
                // Does the closure symbol mean closure here?
                // If so, back up to the previous element
                // and indicate it is enclosed.
                if( !does_closure) goto litcase;
                mj--;
                mcptr--;
                mcptr->mc_type |= CLOSURE;
                magical = TRUE;
                does_closure = FALSE;
                break;

            // Note: no break between MC_ESC case and the default.
            case MC_ESC:
                magical = TRUE;
                pchr = *++patptr;
                if( pchr == '\0' ) {
                    pchr = '\\';
                    --patptr;
                }
            default:
litcase:        mcptr->mc_type = LITCHAR;
                mcptr->u.lchar = pchr;
                does_closure = (pchr != '\n');
                break;
        } // switch

        mcptr++;
        patptr++;
        mj++;
    } // while

    // Close off the meta-string.
    mcptr->mc_type = MCNIL;

    /* Set up the reverse array, if the status is good.  Please note the
     * structure assignment - your compiler may not like that.
     * If the status is not good, nil out the meta-pattern.
     * The only way the status would be bad is from the cclmake()
     * routine, and the bitmap for that member is guarenteed to be
     * freed.  So we stomp a MCNIL value there, call freebits() to
     * free any other bitmaps, and set the zeroth array to MCNIL.
     */
    if( status ) {
        rtpcm = &tapcm[0];
        while( --mj >= 0 ) {
            *rtpcm++ = *--mcptr;
        }
        rtpcm->mc_type = MCNIL;
    }
    else {
        (--mcptr)->mc_type = MCNIL;
        freebits();
        mcpat[0].mc_type = tapcm[0].mc_type = MCNIL;
    }

    return(status);
}

/*
 * mceq -- meta-character equality with a character.  In Kernighan & Plauger's
 *      Software Tools, this is the function omatch()
 */
static int
mceq(bc, mt)
int     bc;
MC      *mt;
{
    int result;

    switch ( mt->mc_type & MASKCL ) {
        case LITCHAR:
            result = eq(bc, mt->u.lchar);
            break;

        case ANY:
            result = (bc != '\n');
            break;

        case CCL:
            if( !(result = biteq(bc, mt->u.cclmap)) ) {
                if( (curwp->bufp->mode & MDEXACT) == 0 && (isletter(bc)) ) {
                    result = biteq(bc ^ 0x20, mt->u.cclmap);
                }
            }
            break;

        case NCCL:
            result = !biteq(bc, mt->u.cclmap);
            if( (curwp->bufp->mode & MDEXACT) == 0 && (isletter(bc)) ) {
                result &= !biteq(bc^0x20, mt->u.cclmap);
            }
            break;

        default:
            mlwrite("mceq: what is %d?", mt->mc_type);
            result = FALSE;
            break;

    } //switch

    return (result);
}

/*
 * cclmake -- create the bitmap for the character class.
 *      ppatptr is left pointing to the end-of-character-class character,
 *      so that a loop may automatically increment with safety.
 */
static int
cclmake(ppatptr, mcptr)
char    **ppatptr;
MC      *mcptr;
{
    BITMAP      clearbits();
    BITMAP      bmap;
    register char   *patptr;
    register int    pchr, ochr;

    if( (bmap = clearbits()) == NULL ) {
        mlwrite("%%Out of memory");
        return FALSE;
    }

    mcptr->u.cclmap = bmap;
    patptr = *ppatptr;

    /*
     * Test the initial character(s) in ccl for
     * special cases - negate ccl, or an end ccl
     * character as a first character.  Anything
     * else gets set in the bitmap.
     */
    if( *++patptr == MC_NCCL ) {
        patptr++;
        mcptr->mc_type = NCCL;
    }
    else {
        mcptr->mc_type = CCL;
    }

    if( (ochr = *patptr) == MC_ECCL ) {
        mlwrite("%%No characters in character class");
        return (FALSE);
    }
    else {
        if( ochr == MC_ESC) ochr = *++patptr;
        setbit(ochr, bmap);
        patptr++;
    }

    while( ochr != '\0' && (pchr = *patptr) != MC_ECCL ) {
        switch ( pchr ) {
            /* Range character loses its meaning
             * if it is the last character in
             * the class.
             */
            case MC_RCCL:
                if( *(patptr + 1) == MC_ECCL) {
                    setbit(pchr, bmap);
                }
                else {
                    pchr = *++patptr;
                    while( ++ochr <= pchr) setbit(ochr, bmap);
                }
                break;

            // Note: no break between case MC_ESC and the default.
            case MC_ESC:
                pchr = *++patptr;
            default:
                setbit(pchr, bmap);
                break;
        }
        patptr++;
        ochr = pchr;
    }

    *ppatptr = patptr;

    if( ochr == '\0' ) {
        mlwrite("%%Character class not ended");
        free(bmap);
        return FALSE;
    }
    return TRUE;
}

/*
 * biteq -- is the character in the bitmap?
 */
static int
biteq(bc, cclmap)
int     bc;
BITMAP  cclmap;
{
    if( bc >= HICHAR) return FALSE;
    return( (*(cclmap + (bc >> 3)) & BIT(bc & 7))? TRUE: FALSE );
}

/*
 * clearbits -- Allocate and zero out a CCL bitmap.
 */
static BITMAP 
clearbits( )
{
    BITMAP cclstart;
    BITMAP cclmap;
    int    j;

    if( (cclmap = cclstart = (BITMAP) malloc(HIBYTE)) != NULL) {
        for (j = 0; j < HIBYTE; j++) {
            *cclmap++ = 0;
        }
    }
    return (cclstart);
}

/*
 * freebits -- Free up any CCL bitmaps.
 */
static int
freebits( )
{
    MC     *mcptr;

    mcptr = &mcpat[0];
    while( mcptr->mc_type != MCNIL ) {
        if( (mcptr->mc_type & MASKCL) == CCL ||
            (mcptr->mc_type & MASKCL) == NCCL )
        {
            free(mcptr->u.cclmap);
        }
        mcptr++;
    }
	return 0;
}

/*
 * setbit -- Set a bit (ON only) in the bitmap.
 */
static int
setbit(bc, cclmap)
int     bc;
BITMAP  cclmap;
{
    if( bc < HICHAR)
        *(cclmap + (bc >> 3)) |= BIT(bc & 7);
	return 0;
}
