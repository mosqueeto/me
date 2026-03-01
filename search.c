/* 
 * The functions in this file implement commands that search in the forward
 * and backward directions.
 */

#include    <stdio.h>
#include    <string.h>
#include    <stdlib.h>
#include    "ed.h"
#include    <pcre.h>

#define FORWARD 1
#define BACKWARD 0

static int readpattern(char *, char []);
static int replaces(int, int, int);

static int lastmatch_os;

/*
 * forwsearch -- Search forward.  Get a search string from the user, and
 *      search, beginning at ".", for the string.  If found, reset the "."
 *      to be just after the match string, and (perhaps) repaint the display.
 */

int
forwsearch(int f, int n)
{
    register int status = TRUE;
    const char *error;
    int erroffset;
    pcre *re;
    int pcre_options = 0;

    if( n == 0 ) n = 1;
    if( n < 0 ) return ( backsearch(f, -n) );

    /* Ask the user for the text of a pattern.  If the
     * response is TRUE (responses other than FALSE are
     * possible), search for the pattern.
     */
    if( (status = readpattern("search", &pat[0])) == TRUE ){
        pcre_options = check_pattern(pat);
        re = pcre_compile(pat,pcre_options,&error,&erroffset,NULL);
        do {
            status = scanfw(re);
        } while( (--n > 0) && status );

        /* ...and complain if not there.
         */
        if( status == FALSE ) {
            mlwrite("Not found");
        }
    }
    return( status );
}

int check_pattern(BYTE p[])
{
    int result = PCRE_CASELESS;

    int i;
    for( i=0;i<MAXPAT && pat[i];i++ ) {
        if ( pat[i] >= 'A' && pat[i] <= 'Z' ) {
            result &= ~PCRE_CASELESS;  // clear flag
            break;
        }
    }
    return result;
}
/*
 * backsearch -- Reverse search.  Get a search string from the user, and
 *      search, starting at "." and proceeding toward the front of the buffer
 *      If found "." is left pointing at the first character of the pattern
 *      (the last character that was matched).
 */
int
backsearch(int f, int n)
{
    register int status = TRUE;
    const char *error;
    int erroffset;
    pcre *re;
    int pcre_options = 0;

    if( n == 0 ) n = 1;
    if( n < 0 ) return ( forwsearch(f, -n) );

    /* Ask the user for the text of a pattern.  If the
     * response is TRUE (responses other than FALSE are
     * possible), search for the pattern.
     */
    if( (status = readpattern("search", &pat[0])) == TRUE ){
        pcre_options = check_pattern(pat);
        re = pcre_compile(pat,0,&error,&erroffset,NULL);
        do {
            status = scanbw(re);
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
 * scanning -- Search for a pattern in either direction.
pcre only goes forward, going back is complicated.
forward:
    while not at eob
    if match in current line, move to there and return
    else next line; goto loop
back:
    while not at bob
      if stored matches in this line, go to next previous else
        back up one line, match forward recording position of last n
        match(es), go to last match.
 */
int
scanfw(pcre *re)
{
    LINE *curline;     // current line during scan
    int curoff;        // position within current line
    int curused;       // characters in current line
    int ovector[30];   // pcre substring vector
    int count;         // number of matches

    // Setup local scan pointers to global "."
    curline = curwp->dotp;
    curoff  = curwp->doto;
    curused = curline->used;

    // Scan each character until we hit the head link record.
    while (!boundry(curline, curoff, FORWARD)) {
        count = pcre_exec(re,NULL,curline->text,curused,curoff,0,ovector,30);
        if( count  >= 0) {  // a match
            curwp->dotp = curline;
            curwp->doto = ovector[1];
            curwp->flag |= WFMOVE; // flag that we have moved
            return TRUE;
        }
        // Advance the cursor.
        curline = curline->fp;
        curoff  = 0;
        curused = curline->used;
    }
    return FALSE;   // No match
}

int
scanbw(pcre *re)
{
    LINE *curline;     // current line during scan
    int curoff;        // position within current line
    int curused;       // characters in current line
    int ovector[30];   // pcre substring vector
    int count;         // number of matches

    // Setup local scan pointers
    if( curwp->doto == 0 ) { // beginning of line, so need to back up
        if( curwp->dotp->bp == curwp->bufp->lines ) {
            return FALSE;
        }
        curline = curwp->dotp->bp;
        curused = curline->used;
    }
    else {
        curline = curwp->dotp;
        curused = curwp->doto;
    }
    curoff  = 0; // always start at beginning of line

    while (!boundry(curline, curoff, BACKWARD)) {
        // skip down the line finding matches
        lastmatch_os = -1;
        while ( count = 
          pcre_exec(re,NULL,curline->text,curused,curoff,0,ovector,30) > 0 ) 
        {
            if( count  >= 0) {  // a match
                lastmatch_os = ovector[0];
                curoff = ovector[0] + 1;
            }
        }
        if( lastmatch_os >= 0 ) {
            curwp->dotp = curline;
            curwp->doto = lastmatch_os;
            curwp->flag |= WFMOVE; // flag that we have moved
            return TRUE;
        }
        // Advance the cursor.
        curline = curline->bp;
        curoff  = 0;
        curused = curline->used;
    }
    return FALSE;   // No match
}


/*
 * readpattern -- Read a pattern.  Stash it in apat.  
 *      Apat is not updated if the user types in an empty line.  If
 *      the user typed an empty line, and there is no old pattern, it is
 *      an error.  Display the old pattern, in the style of Jeff Lomicka.
 *      There is some do-it-yourself control expansion.  Change to using
 *      <META> to delimit the end-of-pattern to allow <NL>s in the search
 *      string. 
 */
static int
readpattern(char *prompt, char apat[])
{
    int status;
    char tpat[MAXPAT+20];

    strcpy(tpat, prompt);   // copy prompt to output string
    strcat(tpat, " [");     // build new prompt string

    strncpy(&tpat[strlen(tpat)],apat,MAXPAT);
    strcat(tpat, "]<META>: ");

    /* Read a pattern.  Either we get one,
     * or we just get the META charater, and use the previous pattern.
     * Then, if it's the search string, make a reversed pattern.
     * *Then*, make the meta-pattern, if we are defined that way.
     */
    if( (status = mlreply(tpat, tpat, MAXPAT)) == TRUE ){
        strcpy(apat, tpat);
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
sreplace(int f, int n)
{
    return( replaces(FALSE, f, n) );
}

/*
 * qreplace -- search and replace with query.
 */
int
qreplace(int f, int n)
{
    return( replaces(TRUE, f, n) );
}

/*
 * replaces -- Search for a string and replace it with another
 *      string.  Query might be enabled (according to kind).
 */
static int
replaces(int kind, int f, int n)
{

    return(TRUE);

}

/*
 * expandp -- Expand control key sequences for output.
 */
int
expandp(char *srcstr, char *deststr, int maxlength)
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
 *      in future.  At the moment, just return TRUE  or
 *      FALSE depending on if a boundry is hit (ouch).
 */
int
boundry(LINE *curline, int curoff, int dir)
{
    int    border;

    if( dir == FORWARD ) {
        border = (curline == curbp->lines);
    }
    else  {
        border = (curline == curbp->lines);
    }
    return (border);
}


