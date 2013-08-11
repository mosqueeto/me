/* pre.c -- perl regular expression support
 */

#include <stdio.h>
#include <string.h>
#include "ed.h"
#include "search.h"
#include "pcre.h"


#define MAXRE 255
#define OVECCOUNT 30

pcre *re;
const char *error;
char *pattern;
char *subject;
unsigned char *name_table;
int erroffset;
int find_all;
int namecount;
int name_entry_size;
int ovector[OVECCOUNT];
int subject_length;
int rc, i;

char old_pattern[MAXRE];



/*

pre_search -- search using perl regular expressions -- only in the forward
direction.  get the re from the user, and search, beginning at ".", for a
match.  If found, reset the "." to be just after the match string, and
(perhaps) repaint the display. 

 */

int
re_search(f, n)
{
    register int status = TRUE;

    if( n == 0 ) n = 1;
    

    /* Ask the user for the text of a pattern.  If the
     * response is TRUE (responses other than FALSE are
     * possible), search for the pattern.
     */
    if( (status = read_re("re:", &pat[0], TRUE)) == TRUE ){
        do {
//            if( (magical && curwp->bufp->mode & MDMAGIC) != 0 ){
//                //
//            }
//            else {
//                //
//            }
        } while( (--n > 0) && status );

        /* ...and complain if not there.
         */
        if( status == FALSE ) {
            mlwrite("Not found");
        }
    }
    return( status );
}

int
read_re( char * prompt )
{
    char tre[MAXRE+20];
    char pstring[256];
    char tpat[MAXRE+20];
    int status;
 
    strcpy(pstring, prompt);
    strcat(pstring, " [");
    strcat(pstring, old_pattern);
    strcat(pstring, "]<META>: ");
    if( (status = mlreply(pstring, tpat, NPAT)) == TRUE ){
    }
    else {
        // copy in old pattern
    }
    re = pcre_compile(
    pattern,                 /* the pattern */
    0,                    /* default options */
    &error,               /* for error message */
    &erroffset,           /* for error offset */
    NULL);                /* use default character tables */

    mlwrite("got a pattern");    
}

