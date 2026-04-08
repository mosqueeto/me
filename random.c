/*
 * This file contains the
 * command processing functions for
 * a number of random commands. There is
 * no functional grouping here, for
 * sure.
 */
#include    <stdio.h>
#include    <string.h>
#include    <stdlib.h>
#include    "ed.h"

EVAR    vtab[MAXVARS];
int lastvar = -1;

// trim leading and trailing whitespace from a string; shift
// the string to the beginning of the line
BYTE *
trim(BYTE *s, int flag)
{
    BYTE *s1 = s;
    BYTE *start = s;
    BYTE *end = s+strlen((char *)s);
    while( isspace(*start) ) start++;
    while( isspace(*end) ) end--;
    *end = '\0';
    while( *s1++ = *start++ );
}

void definevar(BYTE *name, int flag, void *val)
{
    if( lastvar >= MAXVARS ) {
        mlwrite("Too many variables");
        return;
    }
    vtab[++lastvar].name = (BYTE *)malloc(strlen((char *)name));
    strcpy((char *)vtab[lastvar].name,(char *)name);
    vtab[lastvar].flag = flag;
    vtab[lastvar].val = val;
    return;
}
        
int lookupvar(BYTE *name)
{
    int i;
    
    for( i=0; i<=lastvar; i++ ) {
        if( strcmp((char *)name,(char *)vtab[i].name) == 0 ) return i;
    }
    return -1;
}
    
    
/*
 * Set a variable.
 */
int setvar(int f, int n)
{
    BYTE valbuf[81];
    int v;
    BYTE *p,*p1;

    (void)defaultargs(f,n);
        
    for( v=0;v<81;v++ ) valbuf[v] = 0;
        
    if( lastvar<0 ) {       /* define the built-ins     */
        definevar("rmarg", BUILTIN | INTVAL,(void *)&rmarg);
        definevar("lmarg", BUILTIN | INTVAL,(void *)&lmarg);
        definevar("rm",    BUILTIN | INTVAL,(void *)&rmarg);
        definevar("lm",    BUILTIN | INTVAL,(void *)&lmarg);
        definevar("t",     BUILTIN | INTVAL,(void *)&tabsize);
        definevar("T",     BUILTIN | INTVAL,(void *)&softtabs);
    }
    mlreply("Var: ",valbuf,80);
    p = valbuf;
    while( isspace(*p) ) p++;
    p1 = p;
    while( (! isspace(*p)) && (*p != 0) ) p++;
    if( isspace(*p) ) *p++ = '\0';
    v = lookupvar(p1);
    if( v<0 ) {
        mlwrite("Undefined variable: <%s>",p1);
        return FALSE;
    }
    if( vtab[v].flag & RDONLY ) {
        mlwrite("Variable <%s> is read-only",p1);
        return FALSE;
    }
    if( *p == '\0' ) {
        mlreply("Val: ",valbuf,80);
        p = valbuf;
    }
    while( isspace(*p) ) p++;
    if( vtab[v].flag & INTVAL ) {
        if( *p == '.' ) {
            *((int *)vtab[v].val) = getccol();
        } else {
            *((int *)vtab[v].val) = atoi((char *)p);
        }
    } else
    if( vtab[v].flag & STRVAL ) {
        vtab[v].val = (void *)malloc(strlen((char *)valbuf));
        strcpy((char *)(vtab[v].val),(char *)valbuf);
    }
    update();
    return TRUE;
}

BYTE *
getpw(int confirm)
{
    static BYTE pwbuf[MAXPW];
    BYTE pwbuf1[MAXPW];
    BYTE *p;
    BYTE *p1;

    // get pw       
    mlgetpw("Passwd: ",pwbuf,MAXPW);
    p = pwbuf;
    while( isspace(*p) ) 
        p++;
    p1 = &pwbuf[strlen((char *)pwbuf)];
    while( (isspace(*p1) || ! *p1) && p1 > p ) 
        *p1-- = '\0';

    // get confirmation
    if( confirm ) {
        mlgetpw("Again: ",pwbuf1,MAXPW);
        p = pwbuf1;
        while( isspace(*p) ) 
            p++;
        p1 = &pwbuf1[strlen((char *)pwbuf1)];
        while( (isspace(*p1) || ! *p1) && p1 > p ) 
            *p1-- = '\0';
    
        if( strcmp((char *)pwbuf1,(char *)pwbuf) ) {
            mlwrite("Sorry, they don't match","");
            return NULL;
        }
    }
    return pwbuf;
}

int
encryptb(int f, int n)
{
    int i;
    BYTE *p,*p1;

    (void)defaultargs(f,n);
#if CRYPT_S
    p = getpw(1);
    if( !p ) return TRUE;
    p1 = curbp->passwd;
    while( *p ) *p1++ = *p++;
    filewrite(f,n);
#else
    mlwrite("encryption not supported on this platform");
#endif
    return TRUE;
}
int
decryptb(int f, int n)
{
    int i;
    BYTE  *p,*p1;

    (void)defaultargs(f,n);
#if CRYPT_S
    curbp->passwd[0] = '\0';
    filewrite(f,n);
#else
    mlwrite("decryption not supported on this platform");
#endif
    return TRUE;
}

/*
 * Set indent
 */
int setindent(int f, int n)
{
    int s,i;
    BYTE numbuf[16];

    (void)defaultargs(f,n);
        
    if( (s = mlreply("Indent: ",numbuf,16)) != TRUE ) {
        return s;
    } 
    (void)trim(numbuf,0);
    if( !strcmp( (char *)numbuf,"+" ) || !strcmp( (char *)numbuf,"=" ) ) {
        lmarg += tabsize;
        if( lmarg > rmarg ) lmarg = rmarg-1;
        return( TRUE );
    }
    if( !strcmp( (char *)numbuf,"-" ) ) {
        lmarg -= tabsize;
        if( lmarg < 0 ) lmarg = 0;
        return( TRUE );
    }
    i = atoi((char *)numbuf);
    if( i==0 ) {
        lmarg = 0;
        return( TRUE );
    }
    lmarg += i;
    if ( lmarg < 0 ) lmarg = 0;
    if( lmarg > rmarg ) lmarg = rmarg-1;

    return(tab(TRUE,i));
}
/*
 * Set right margin
 */
int setrmarg(int f, int n)
{
    int s,i;
    BYTE numbuf[16];

    (void)defaultargs(f,n);
        
    if( (s = mlreply("Fill column[<n> or .]: ",numbuf,16)) != TRUE ) {
        return s;
    } 
    if( numbuf[0] == '.' ) {
        rmarg = getccol(FALSE);
    } else {
        i = atoi((char *)numbuf);
        if( i<0 ) return( FALSE );
        rmarg = i;
    }
    return(TRUE);
}


/*
 * Display the current position of the cursor,
 * in origin 1 X-Y coordinates, the character that is
 * under the cursor (in hex), and the fraction of the
 * text that is before the cursor. The displayed column
 * is not the current column, but the column that would
 * be used on an infinite width display. Normally this
 * is bound to "C-X =".
 */
int showcpos(int f, int n)
{
    register LINE   *clp;
    register long   nch;
    register int    cbo;
    register long   nbc;
    register int    cac;
    register int    ratio;
    register int    col;
    register int    vcol;
    register int    i;
    register int    c;
    register int    ln;
    register int    cln;
    register int    cl;
    register int    ccl;
    BYTE mbuf[80];

    (void)defaultargs(f,n);
    
    // start at beginning of buffer, all offsets and counters = 0
    // count all the way to the end, to get total size, but remember
    // where we were when we got to current position
    clp = lforw(curbp->lines);
    cbo = 0;
    nch = 0;
    nbc = 0;
    cac = 0;
    cln = 0;
    ccl = 0;
    ln  = 0;
    cl  = 0;
    while( clp != curbp->lines ) { // go through all lines
        if( clp==curwp->dotp ) {  // at the line with the cursor
            nbc = nch + curwp->doto + 1;
            cln = ln;
            ccl = cl;
//            if( curwp->doto == llength(clp) ) {
//                cac = '\n';
//            } else {
                cac = lgetc(clp, curwp->doto);
//            }
        }
        ln++;
        cl = 0;
        if( clp->flags & L_NL ) nch++; // count newline? 
        nch += llength(clp);
        clp = lforw(clp);
    }
#if 0
    for( ;; ) {  // count the characters.
        if( clp==curwp->dotp && cbo==curwp->doto ) {  // remember this
            nbc = nch;
            cln = ln;
            ccl = cl;
            if( cbo == llength(clp) ) {
                cac = '\n';
            } else {
                cac = lgetc(clp, cbo);
            }
        }
        if( cbo == llength(clp) ){
            if( clp == curbp->lines ) {  // we are at end
                break;
            }
            ln++;
            cl = 0;
            clp = lforw(clp);
            cbo = 0;
        } else {
            ++cbo;
        }
        ++nch;
    }
    ++nbc;
#endif
    vcol= getvcol()+1;          /* get virtual column   */
    if( cac == '\n' ) vcol--;       /* don't count eol  */
    ratio = 0;              /* Ratio before dot.    */
    if( nch != 0 ) ratio = (100L*nbc) / nch;
    sprintf((char *)mbuf,
    "line=%d.%d/%d vcol=%d CH=0x%02x .=%ld/%ld (%d%%%%)",
    cln+1, ccl+1, ln, vcol,  cac, nbc, nch, ratio);
logchr(cac);
    mlwrite(mbuf);
    
    return (TRUE);
}

int bufchars(int f, int n)
{
    register LINE   *lp;
    register long   nch;
    BYTE mbuf[80];

    (void)defaultargs(f,n);
    
    lp = lforw(curbp->lines);
    nch = 0;
    while( lp != curbp->lines ) {
        nch += llength(lp);
        nch++; // count newline??
        lp = lforw(lp);
    }
    sprintf((char *)mbuf,"nbytes = %ld",nch);
    mlwrite(mbuf);
    
    return (TRUE);
}



/*
 * Return virtual column.
 */
getvcol()
{
    int col = 0;
    int i;
    int c;

    for (i=0; i<curwp->doto; ++i) {
        c = lgetc(curwp->dotp, i);
        if (c == '\t') {
            col = (col / tabsize + 1) * tabsize;
//            col |= 0x07;
        } 
//        else if (c<0x20 || c==0x7F) {
//            ++col;
//        }
        ++col;
    }
    return(col);
}

/*
 * Return current column in current segment of line.
 */
getccol()
{
    return( getvcol() );
}

/* 

Twiddle the two characters on either side of dot.  Return with an error if
dot is at the beginning or end of line; it seems to be a bit pointless to
make this work.  This fixes up a very common typo with a single stroke. 
Normally bound to "C-T". 

 */
int twiddle(int f, int n)
{
    register LINE   *dotp;
    register int    doto;
    register int    cl;
    register int    cr;

    (void)defaultargs(f,n);
    
    doto = curwp->doto;
    if( doto == 0 ) 
        return (FALSE);
    dotp = curwp->dotp;
    if( doto==llength(dotp) )
        return (FALSE);

    cr = lgetc(dotp, doto);
    cl = lgetc(dotp, --doto);
    lputc(dotp, doto+0, cr);
    lputc(dotp, doto+1, cl);
    lchange();
    return (TRUE);
}

/*
 * Quote the next character, and
 * insert it into the buffer. All the characters
 * are taken literally, with the exception of the newline,
 * which always has its line splitting meaning. The character
 * is always read, even if it is inserted 0 times, for
 * regularity.
 */
int quote(int f, int n)
{
    register int    s;
    register int    c;

    (void)defaultargs(f,n);
    
    if( kbdmop == NULL ) {
        c = (*term.getchar)();
    } else {
        c = *kbdmop++;
    }
    if (n < 0)
        return (FALSE);
    if (n == 0)
        return (TRUE);
    if (c == '\n') {
        do {
            s = lnewline();
        } while (s==TRUE && --n);
        return (s);
    }
    if( kbdmip != NULL ) {
        *kbdmip++ = c;
    }
    return (linsert(n, c));
}

/*
 * Set tab size if given non-default argument (n <> 1).  Otherwise, insert a
 * tab into file.  
 * If given argument, n, of zero, change to true tabs.
 * If n > 1, simulate tab stop every n-characters using spaces.
 * This has to be done in this slightly funny way because the
 * tab (in ASCII) has been turned into "C-I" (in 10
 * bit code) already. 
 * Bound to "C-I".
 */
int tab(int f, int n)
{
    int status;
    (void)defaultargs(f,n);
    
    if( n < 0 ) return (FALSE);
    
    if( n == 0 || n > 1 ){
        tabsize = n;
        update();
        return(TRUE);
    }
    if( ! softtabs ){
        status =linsert(1, '\t');
    } else {
        status =linsert(tabsize - (getccol() % tabsize), ' ');
    }
    if( (rmarg > 0) && (getvcol() > rmarg ) ) wrapword();
    return status;
}

/* Open up some blank space after the current line.
Normally this is bound to "C-O". 
 */
int openline(int f, int n)
{
    LINE *lp;

    (void)defaultargs(f,n);
    
    if (n < 0)
        return (FALSE);
    if (n == 0)
        return (TRUE);
    lp = curwp->dotp;
    while( n > 0 ) {
        lp = ladd(lp->fp,0);
        lp->used = 0;
        n--;
    }
    // were we at end of buffer?
    if( curwp->dotp == curbp->lines ) {
        curbp->lines = lp;
    }
    lchange();
    update();
    return (TRUE);
}

/*
 * Insert a newline. Bound to "C-M".
 */
int newline(int f, int n)
{
//    int nicol;
    register LINE   *lp;
    register int    s;

    (void)defaultargs(f,n);
    
    if (n < 0)
        return (FALSE);
    while (n--) {
        if ((s=lnewline()) != TRUE)
            return (s);
    }
    return (TRUE);
}

/*
 * Delete blank lines around dot.
 * What this command does depends if dot is
 * sitting on a blank line. If dot is sitting on a
 * blank line, this command deletes all the blank lines
 * above and below the current line. If it is sitting
 * on a non blank line then it deletes all of the
 * blank lines after the line. Normally this command
 * is bound to "C-X C-O". Any argument is ignored.
 */
int deblank(int f, int n)
{
    register LINE   *lp1;
    register LINE   *lp2;
    register int    nld;

    (void)defaultargs(f,n);
    
    lp1 = curwp->dotp;
/*  while (llength(lp1)==0 && (lp2=lback(lp1))!=curbp->lines)
        lp1 = lp2;*/
    while (isblankline(lp1) && (lp2=lback(lp1))!=curbp->lines)
        lp1 = lp2;
    lp2 = lp1;
    nld = 0;
/*  while ((lp2=lforw(lp2))!=curbp->lines && llength(lp2)==0)
        ++nld;*/
    while ((lp2=lforw(lp2))!=curbp->lines && isblankline(lp2))
        nld += llength(lp2) + 1; /* allow for newline */
    if (nld == 0)
        return (TRUE);
    curwp->dotp = lforw(lp1);
    curwp->doto = 0;
    return (ldelete(nld,TRUE));
}

/*
 * Insert a newline, then enough
 * tabs and spaces to duplicate the indentation
 * of the previous line. Assumes tabs are every eight
 * characters. Quite simple. Figure out the indentation
 * of the current line. Insert a newline by calling
 * the standard routine. Insert the indentation by
 * inserting the right number of tabs and spaces.
 * Return TRUE if all ok. Return FALSE if one
 * of the subcomands failed. Normally bound
 * to "C-J".
 */
int indent(int f, int n)
{
    register int    nicol;
    register int    c;
    register int    i;

    (void)defaultargs(f,n);
    
    if (n < 0) return (FALSE);
    while (n--) {
        nicol = 0;
        for (i=0; i<llength(curwp->dotp); ++i) {
            c = lgetc(curwp->dotp, i);
	        if( c == ' ' ) {
		       ++nicol;
		    }
		    else if( c == '\t' ) {
		       nicol += tabsize;
		    }
		    else {
		        break;
			}
        }
        if (lnewline() == FALSE || (linsert(nicol,  ' ')==FALSE) )
            return (FALSE);
    }
    return (TRUE);
}

/*
 * Delete forward. This is real
 * easy, because the basic delete routine does
 * all of the work. Watches for negative arguments,
 * and does the right thing. If any argument is
 * present, it kills rather than deletes, to prevent
 * loss of text if typed with a big argument.
 * Normally bound to "C-D".
 */
/* In MDWRAP mode, reflow the paragraph after a deletion if the cursor is
   on or adjacent to a soft-wrapped line. */
static void wrap_reflow_after_del(void)
{
    LINE *cur = curwp->dotp;
    int   pos = curwp->doto;

    if (!((curbp->mode & MDWRAP) && rmarg > 0)) return;
    if (!((cur->flags & L_SNL) || (lback(cur)->flags & L_SNL))) return;

    /* Deleting the first char of a word creates a double space at the cursor.
       Skip fillpara to avoid the extra cursor jump; the double space will be
       normalized on the next auto-wrap or M-q. */
    if (pos > 0 && pos < llength(cur) &&
        lgetc(cur, pos-1) == ' ' && lgetc(cur, pos) == ' ')
        return;

    fillpara(FALSE, 1);
}

int forwdel(int f, int n)
{
    int s;
    (void)defaultargs(f,n);

    if (n < 0)
        return (backdel(f, -n));
    if (f != FALSE) {           /* Really a kill.   */
        if ((lastflag&CFKILL) == 0)
            kdelete();
        thisflag |= CFKILL;
    }
    s = ldelete(n, f);
    wrap_reflow_after_del();
    return (s);
}

/*
 * Delete backwards. This is quite easy too,
 * because it's all done with other functions. Just
 * move the cursor back, and delete forwards.
 * Like delete forward, this actually does a kill
 * if presented with an argument. Bound to both
 * "RUBOUT" and "C-H".
 */
int backdel(int f, int n)
{
    register int    s;

    (void)defaultargs(f,n);

    if (n < 0)
        return (forwdel(f, -n));
    if (f != FALSE) {           /* Really a kill.   */
        if ((lastflag&CFKILL) == 0)
            kdelete();
        thisflag |= CFKILL;
    }
    if ((s=backchar(f, n)) == TRUE)
        s = ldelete(n, f);
    wrap_reflow_after_del();
    return (s);
}

/*
 * Kill text. If called without an argument,
 * it kills from dot to the end of the line, unless it
 * is at the end of the line, when it kills the newline.
 * If called with an argument of 0, it kills from the
 * start of the line to dot. If called with a positive
 * argument, it kills from dot forward over that number
 * of newlines. If called with a negative argument it
 * kills backwards that number of newlines. Normally
 * bound to "C-K".
 */
int killfw(int f, int n)
{
    register int    chunk;  // how many characters to delete
    register LINE   *nextp;

    (void)defaultargs(f,n);
    
    if( (lastflag & CFKILL) == 0 ){ // Clear kill buffer if
        kdelete();                  // last command wasn't a kill
    }
    thisflag |= CFKILL;
    if( f == FALSE ){  // no argument
        chunk = llength(curwp->dotp) - curwp->doto;
        if( chunk == 0 ){  // empty line
            if( curwp->dotp->flags & L_NL ) {
                chunk = 1;  // delete the logical newline
            }
            else {
                return TRUE;
            }
        }
    } 
    else if( n == 0 ){
        chunk = curwp->doto;
        curwp->doto = 0;
    } 
    else if( n > 0 ){
        chunk = 0;
        nextp = curwp->dotp;
        while( n > 0 ){
            if( nextp == curbp->lines ){
                return( FALSE );
            }
            chunk += llength(nextp);
            nextp = lforw(nextp);
        }
        chunk -= curwp->doto;
    } else {
        mlwrite("neg kill");
        return (FALSE);
    }
    return( ldelete(chunk, TRUE) );
}

/*
 * Yank text back from the kill buffer. This
 * is really easy. All of the work is done by the
 * standard insert routines. All you do is run the loop,
 * and check for errors. Bound to "C-Y". The blank
 * lines are inserted with a call to "newline"
 * instead of a call to "lnewline" so that the magic
 * stuff that happens when you type a carriage
 * return also happens when a carriage return is
 * yanked back from the kill buffer.
 */
int yank(int f, int n)
{
    register int    c;
    register int    i;
    extern   int    kused;

    (void)defaultargs(f,n);
    
    if (n < 0)
        return (FALSE);
    while (n--) {
        i = 0;
        while ((c=kremove(i)) >= 0) {
            if (c == '\n') {
                if (newline(FALSE, 1) == FALSE)
                    return (FALSE);
            } else {
                if (linsert(1, c) == FALSE)
                    return (FALSE);
            }
            ++i;
        }
    }
    return (TRUE);
}
