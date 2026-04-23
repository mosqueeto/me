/*
The routines in this file handle the "file" abstraction.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ed.h"
#include "crypt.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

extern int set_mode(int);
#if CRYPT_S
extern BYTE *md5string(BYTE *, long);
extern BYTE *encrypt_buf(BYTE *, BYTE *, int *);
extern BYTE *decrypt_buf(BYTE *, BYTE *, long *);
#endif

static BYTE backupname[1024];

#define MAXFILES 256
struct stat statbuf;
struct stat *file_data[MAXFILES];
static int last_file = 0;


// module definitions for the putline buffer
#define IOBUFZ 8192
BYTE putline_buf[IOBUFZ+128];
BYTE * plbp = putline_buf;
int  plb_bytes = 0;

int io_error = 0;
BYTE curfn[NFILEN];

//
//  figure out a filename for a backup
//
int makebackupname(BYTE *fn)
{
    int nbytes,first;
    BYTE *p1,*p2;

    if( (nbytes=strlen((char *)fn)) > 1020 ){
        mlwrite("WARNING: backup filename too long");
        sleep(1);
        return( 0 );
    }
    // prepend ",," to the filename
    first = 1;
    p2 = fn + nbytes;
    p1 = backupname + nbytes + 2;
    while( p2 >= fn ){
        if( first && (*p2 == '/') ){
            first = 0;
            *p1-- = ',';
            *p1-- = ',';
        }
        *p1-- = *p2--;
    }
    if( first ){
        *p1-- = ',';
        *p1-- = ',';
    }
    return TRUE;
}

//
// Read an entire file
//
BYTE *
slurpfile(int fd, long *len)
{
    BYTE *file_buf;

    if( fstat(fd,&statbuf) != 0 ) {
        io_error = errno;
        mlwrite("Indefined file handle!!");
        sleep(1);
        mlwrite(strerror(io_error));
        sleep(1);
        return( FIOSUC );
    }

if( dbug ) {
    mlwrite("slurp: file size = %d",statbuf.st_size);
    sleep(1);
}

    // slurp the file
    file_buf = (BYTE *)malloc(statbuf.st_size + 128);
    // (allow a little space at end of file)
    if( !file_buf ) {
        mlwrite("memory allocation error in fileio.c");
        return (NULL);
    }
    if( read(fd,file_buf,statbuf.st_size) != statbuf.st_size ) {
        mlwrite("Couldn't read file");
        free(file_buf);
        return ( NULL );
    }
    *len = statbuf.st_size;
if( dbug ) {
    mlwrite("slurped %d bytes",*len);
    sleep(1);
}
    return file_buf;
}

/*
Read a file into the current buffer.  Find the name of the file, 
and call the standard "read a file into the current buffer" code.  
Bound to "C-X C-V". 
*/
int filevisit(int f, int n)
{
    int    s;
    BYTE   fname[NFILEN];
    (void)defaultargs(f,n);
    if ((s=mlreply("visit file: ", fname, NFILEN)) != TRUE)
        return (s);
    return (readin(curbp,fname));
}

/*
Read the next file into the current buffer.  Find the name of the file, 
and call the standard "read a file into the current buffer" code.  
Bound to "C-X C-N". 
*/
int nextfile(int f, int n)
{
    int    s;
    BYTE   fname[NFILEN];
    static int i=0;
    (void)defaultargs(f,n);
    if( ! fnames[i] ) {
        mlwrite("No more files");
        return( TRUE );
    }
    return (readin(curbp,fnames[i++]));
}

/*
Insert a file into the current buffer. Bound to "C-X C-I". 
*/
int insfile(int f, int n)
{
    int     s;
    BYTE    fname[NFILEN];

    if ((s=mlreply("insert file: ", fname, NFILEN)) != TRUE)
         return(s);
    return(ifile(fname));
}



/*
Select a file for editing.  See if you can find the file in
another buffer; if you find it switch to the buffer.  If not
create a new buffer, read in the text, and switch to the new
buffer.  
Bound to c-x c-f. 
*/
int fileread(int f, int n)
{
    BYTE   fname[NFILEN];
    int    status;

   (void) defaultargs(f,n);
    if( (status = mlreply("Read file: ", fname, NFILEN)) != TRUE )
        return (status);   
logit("fname: ");
logit(fname);

//    return readin(curbp,fname);
    // fileread_1 handles dealing with preexisting buffers and so on
    return fileread_1(f,n,fname);
}


int fileread_1(int f, int n, char *fname)
{
    BUFFER *bp;
    BUFFER *obp;
    WINDOW *wp;
    LINE   *lp;
    int    i;
    int    status;
    BYTE   bname[NBUFN];

logit("\nin fileread_1\n");
    // file already in a buffer?
    for( bp=bheadp; bp!=NULL; bp=bp->bufp ) {
        if( (bp->flag&BFTEMP)==0 && 
            strcmp((char *)bp->fname, (char *)fname)==0 ) {
            if( f != TRUE ){
                old_buffer(bp);
                return( TRUE );
            }
        }
    }

    // ok, need to really read something
    makename(bname, fname);   // new buffer name in cannonical form
logit("\nbname = ");
logit(bname);
logit("\n");

    // if buffer exists, new name, or use name and clobber contents
    while ((bp=bfind(bname, FALSE, 0)) != NULL) {  
        status = mlreply("buffer name: ", bname, NBUFN);
        if (status == ABORT)             // ^g to just quit
            return (status);
        if (status == FALSE) {           // cr to clobber it
            makename(bname, fname);
            break;
        }
    }
    
    // if no existing buffer, make a new one.
    if( bp==NULL ) {
        if( (bp=bfind(bname, TRUE, 0))==NULL) {
            mlwrite("cannot create buffer");
            return (FALSE);
        }
logit("\nnew buffer made: ");
logit(bname);
    }

    i = strlen((char *)fname);
    if( (fname[i-1] == 'c' || fname[i-1] == 'h') && fname[i-2] == '.' ){
        set_mode('C');
        bp->mode = 'C';
    }

    if (--curbp->nwnd == 0) {       // undisplay.
        curbp->dotp = curwp->dotp;
        curbp->doto = curwp->doto;
        curbp->markp = curwp->markp;
        curbp->marko = curwp->marko;
    }

    curbp = bp;         // switch to new buffer.
    curwp->bufp = bp;
    curbp->nwnd++;

    status = readin(curbp,fname);         // read in the file.
logit("\njust called readin\n");
logit("for buffer: ");
logit(curbp->bname);

if( dbug ) {
    sleep(1);
    mlwrite("file %s buffer %s status %d",curbp->bname,fname,status);
    sleep(1);
}
    return status;
} // fileread_1


//////////////////////////////////////////////////////////////

/*
Read file "fname" into the buffer, blowing away any text found there. 
Called by both the read and visit commands.  return the final status of the
read.
 */
int readin(BUFFER *bufp, BYTE fname[])
{
    WINDOW *wp;
    int  status = TRUE;
    long  nline;
    long fz;

logit("\nreadin: ");
logit(fname);
logit("\n ");
logit(bufp->bname);
    // prepare buffer
    if( ! ( status = bclear(bufp) ) )    // might be old.
        return (FALSE);

    bufp->flag &= ~(BFTEMP|BFCHG);
    strcpy((char *)bufp->fname, (char *)fname);

    // actual mechanics of the read into current buffer
    status = do_read(bufp,fname,&nline,&fz);

logint("\ndo_read status: ",status);
    
    if ( status == FIOERR ) {
       strcpy((char *)bufp->fname, "");
       return (FALSE);
    }

    for ( wp=wheadp; wp!=NULL; wp=wp->wndp ) {
        if ( wp->bufp == bufp ) {
            wp->topp = lforw(bufp->lines);
            wp->dotp  = lforw(bufp->lines);
            wp->doto  = 0;
            wp->markp = NULL;
            wp->marko = 0;
            wp->flag |= WFMODE;
        }
    }

    if( status == FIONEW ) {
        mlwrite( "new file" );
        return ( TRUE );
    }
    if ( status == FIOSUC ) {
        mlwrite("[read %d lines (%d bytes)]",nline,fz);
        return ( TRUE );
    }
    else {
        mlwrite("i/o error: %d",status);
        return ( FALSE );
    }
} // readin


/*
Insert file "fname" into the current buffer, Called by insert file command.
Return the final status of the read.
 */
int ifile(BYTE fname[])
{
    int  status = TRUE;
    long nline = 0;
    long fz = 0;
    
    struct LINE *save_dotp;

    // prepare buffer
    curbp->flag |= BFCHG; // we have changed
    text_chg_flag++;      // disable dot macro

if( dbug ) {                
    mlwrite("[inserting file %s]",fname);
    sleep(1);
}

    // back up a line and save the mark here
    curwp->dotp = lback(curwp->dotp);
    save_dotp = curwp->dotp;
    curwp->doto = 0;
    curwp->markp = curwp->dotp;
    curwp->marko = 0;

    status = do_read(curbp,fname,&nline,&fz);

    // advance to the next line and mark the window for changes
    curwp->dotp = save_dotp;
    curwp->dotp = lforw(curwp->dotp);
    curwp->flag |= WFMODE;
        
    // copy window parameters back to the buffer structure
    curbp->dotp = curwp->dotp;
    curbp->doto = curwp->doto;
    curbp->markp = curwp->markp;
    curbp->marko = curwp->marko;

    if( status == FIOERR ){
        return (FALSE);
    }
    mlwrite("[read %d lines (%d bytes)]",nline,fz);
    return (TRUE);

} // ifile

/*
The basic strategy is to read the entire file into a temporary binary buffer
(not an emacs "buffer"), then parse out individual lines and populate an 
emacs "buffer".  This is so we can decrypt (or otherwise preprocess the 
file, though the only processing so far is decrypting.)
*/

int do_read(BUFFER *bp, BYTE fname[], long *nlines, long *filesize)
{
    LINE   *lp;
    int  status = TRUE;
    int  i,j;
    int  nline;
    BYTE *fbp;
    BYTE *file_buf;
    BYTE *decrypted_buf;
    BYTE *nlp;
    BYTE *p;
    long fz;
    long bz = 0;
    long rlen,len;
    int fd;
    int nl_flag = 0;

// raw read the file into a buffer
// decrypt if necessary
// then create an me buffer with pointers and lengths that
// point into the raw buffer
    
if( dbug ) {
    mlwrite("do_read for %s",fname);
    sleep(1);
}
    // open file
    if( ( fd = ffropen(fname) ) < 0 ) {
        if( io_error != ENOENT )
        return FIOERR; // ffropen leaves message in status line
    }

    if(  io_error == ENOENT ) {         // file not found.
        mlwrite("[new file]");
        return ( FIONEW );
    }

    status = FIOERR;  // assume it failed
logit("\ndo_read: ");
logit(fname);


    // read the file    
    mlwrite("[reading file %s]",fname);
    if( ! ( file_buf = slurpfile(fd,filesize) ) ) {
        mlwrite("read error");
        sleep(1);
        return status;
    }
    close(fd); // Ignore close errors.

    bz = fz = *filesize;  // assume buffer size is same as file size, to start
    
    // if it is an encrypted file, try to decrypt it
    if( fz >= 48 && ! strncmp( (char *)file_buf,ME_MAGIC,strlen(ME_MAGIC) ) ) {
#if CRYPT_S
        p = NULL;
        p = getpw(0);
        if( ! p ) return status;
        strncpy((char *)bp->passwd,(char *)p,MAXPW);
        decrypted_buf = decrypt_buf(p,file_buf,&bz);
        free(file_buf);

        if( !decrypted_buf || bz <= 0 ) {
            mlwrite("file cannot be decrypted");
            return status;
        }
        file_buf = decrypted_buf;
#else
    mlwrite("encrypted file; decryption not supported on this platform");
    goto out;
#endif
    }

    // tracking vars
    rlen = bz;
    fbp = file_buf;
    // go through buf, pulling out lines and putting them
    // in the BUFFER
    len = 0;
    nline = 0;
    while( rlen > 0 ) {
        // find a lines worth of stuff
logit("\ngetting lines:");
logit(fbp);
        len = rlen;
        nl_flag = L_NL;
        nlp = (BYTE *)memchr((char *)fbp,'\n',len);
        if( nlp == fbp ) {
            len = 0;
        } 
        else if( nlp != (BYTE *)NULL ) {
            len = nlp-fbp;
        } 
        else {
            nl_flag = 0;
        }
        
        // add the line into the BUFFER
//        if ((lp=ladd(curwp->dotp->fp,-1)) == NULL) {
        if ((lp=ladd(bp->dotp->fp,-1)) == NULL) {
            status = FIOERR; // Keep message on the display.
            return status;
        }
//        curwp->dotp = lp;
        bp->dotp = lp;

        lp->text = fbp;
        lp->flags |= (L_EXT|nl_flag);
        lp->size = lp->used = len;
logit("lp->text:");
logit(lp->text);

        
        
        // and update some indices, counters, and flags
        fbp  += len;
        rlen -= len;
        if( nlp != (BYTE *)NULL ) {
            fbp++;  // skip the NL
            rlen--;
            nline++;
        }
    }
    
    *nlines = nline;

logint("\nnline: ",nline);

    status = FIOSUC;
    return status;

}

/*
Take a file name, and from it fabricate a buffer name.
*/
int makename(BYTE bname[], BYTE fname[])
{
    BYTE   *cp1;
    BYTE   *cp2;

    cp1 = &fname[0];
    while (*cp1 != 0)
        ++cp1;
    while (cp1!=&fname[0] && cp1[-1]!='/')
        --cp1;
    cp2 = &bname[0];
    while (cp2!=&bname[NBUFN-1] && *cp1!=0 && *cp1!=';')
        *cp2++ = *cp1++;
    *cp2 = 0;
    return TRUE;
}

int reread(int f, int n)
{
    (void)defaultargs(f,n);
    fileread(TRUE,n);
}

int old_buffer(BUFFER *bp)
{
    register WINDOW *wp;
    register LINE   *lp;
    register int    i;

    if (--curbp->nwnd == 0) {
        curbp->dotp  = curwp->dotp;
        curbp->doto  = curwp->doto;
        curbp->markp = curwp->markp;
        curbp->marko = curwp->marko;
    }
    curbp = bp;
    curwp->bufp  = bp;
    if (bp->nwnd++ == 0) {
        curwp->dotp  = bp->dotp;
        curwp->doto  = bp->doto;
        curwp->markp = bp->markp;
        curwp->marko = bp->marko;
    } else {
        wp = wheadp;
        while (wp != NULL) {
            if (wp!=curwp && wp->bufp==bp) {
                curwp->dotp  = wp->dotp;
                curwp->doto  = wp->doto;
                curwp->markp = wp->markp;
                curwp->marko = wp->marko;
                break;
            }
            wp = wp->wndp;
        }
    }
    lp = curwp->dotp;
    i = curwp->ntrows/2;
    while (i-- && lback(lp)!=curbp->lines)
        lp = lback(lp);
    curwp->topp = lp;
    curwp->flag |= WFMODE;
    mlwrite("[old buffer]");
    return (TRUE);
}



/*
Ask for a file name, and write the contents of the current buffer to that
file.  Update the remembered file name and clear the buffer changed flag. 
Bound to "c-x c-w". 
*/
int filewrite(int f, int n)
{
    WINDOW *wp;
    int    s;
    BYTE   fname[NFILEN];

    (void)defaultargs(f,n);
    if ((s=mlreply("write file: ", fname, NFILEN)) != TRUE)
        return (s);
    if( (s = writeout(fname,1)) == TRUE ) {
        strcpy((char *)curbp->fname, (char *)fname);
        curbp->flag &= ~BFCHG;
        wp = wheadp;            // Update mode lines.
        while (wp != NULL) {
            if (wp->bufp == curbp)
                wp->flag |= WFMODE;
            wp = wp->wndp;
        }
    }
    return (s);
}

/*
Save the contents of the current buffer in its associated file.  No nothing
if nothing has changed (this may be a bug, not a feature).  Error if there is
no remembered file name for the buffer.  Bound to "C-X C-S". 
*/
int filesave(int f, int n)
{
    register WINDOW *wp;
    register int    s;

    (void)defaultargs(f,n);
    if ((curbp->flag&BFCHG) == 0)   // Return, no changes.
        return (TRUE);
    if (curbp->fname[0] == 0) {     // Must have a name.
        mlwrite("no file name");
        return (FALSE);
    }

    if( (s = writeout(curbp->fname,1)) == TRUE ) {
        curbp->flag &= ~BFCHG;
        wp = wheadp;                // Update mode lines.
        while (wp != NULL) {
            if (wp->bufp == curbp)
                wp->flag |= WFMODE;
            wp = wp->wndp;
        }
    }
    return (s);
}

/*

This complicated function performs the details of file writing.  

When we open the to-be-written file, and it exists, and it's changed from 
when we opened it before, then we return an error.

The algorithm is more complicate than one might expect because it supports 
two modes: update in place (which overwrites the original file datablocks, 
and thus preserves the inode), and the "new inode" mode, where a new file is 
written, and the old file is renamed to be the backup.  Update mode is the 
default, and so far there is no interface to activate "new inode" mode.  
Update mode is better in many ways, but messes up efficient backup schemes 
using hard links.

There are 3 cases: "create" (new file); "update" (no change in inode); and
"save" (new inode).  update is still the default; need to add commands to
control this.

1. create: this is simple, because there are no backup files to create.  in
this case, use the default permission bits 0600.

2. update: make a backupname, unlink it if it exists, create a file by that 
name, copy the old contents to that file; then open the old file for writing 
and copy the new data into it.  the backup should have the same 
permissions/ownership as the original.

3. save: make a backup name, unlink it if it exists, rename the old file to 
the backup name.  create the file using the old name, and write the data. 
the newly created file should have the same permissions/ownership as the 
original. 

To enhance this for versioned backups, only need to change makebackupname to 
be more clever about it's choice of name.

Note that in cases 2 and 3, the backup is always in the same directory, and 
hence in the same filesystem, as the original, so hard links always work. 
(what about nfs?)

*/

int writeout(BYTE *fn, int update)
{
    int     s;
    LINE   *lp;
    int     nline;
    int     i;
    int     fd;
    int     nfd;
    int     ofd;
    int     nch;
    BYTE    *buf;
    BYTE    *newbuf;
    BYTE    *bp;
    BYTE    *t;
    mode_t  mode;
    mode_t  o_umask;

    //  global
    statbuf.st_ino = 0;

    // start by assuming it's a new file.
    // if the create fails, then it is not a new file...
//
//  NOT SO! -- could just be a permission problem in the directory.  
//  Need to actually  check the error, and give a proper error message 
//  in that case.
//  Another problem: open a new file, then c-x c-i to insert a non-existent
//  file gets a "Indefined file handle!!" error
//
    if( (nfd = open((char *)fn,O_WRONLY|O_CREAT|O_EXCL,0600)) < 0 ) {
        if( ! (errno == EEXIST ) ) {
            mlwrite("Error: %s",strerror(errno));
            return FIOSUC;            
        }
        // OK, the file exists, so we are rewriting it
        if( ! makebackupname(fn) ) {
            return FIOSUC;
        }

        // now really open the file
        nfd = open( (char *)fn, O_RDWR);
        if( nfd < 0 ) {
            io_error = errno;
            mlwrite("Error opening original file");
            sleep(1);
            mlwrite(strerror(io_error));
            close( ofd );
            return( FIOSUC );
        }

        // and get the metadata
        if( ffget_stat(nfd,1) == FIOERR ) return FIOERR;

        if( ! (S_ISREG(statbuf.st_mode)) ) {
            mlwrite("Not a regular file!"); // we can only update regular
            return( FIOSUC );               // files
        }
        mode = statbuf.st_mode & 
            (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO);
        
        // update mode preserve the inode?
        if( update ) {
            unlink((char *)backupname); // delete previous backup, if any
            o_umask = umask(0);  // must be able to recreate any modes
            ofd = open( (char *)backupname, O_WRONLY|O_CREAT|O_EXCL, mode );
            o_umask = umask(o_umask);

            // if we can't create backup, don't write the file; return
            if( ofd < 0 ) {
                io_error = errno;
                mlwrite("Error creating backup file");
                sleep(1);
                mlwrite(strerror(io_error));
                return( FIOSUC ); // don't write the file
            }

            // duplicate the metadata for the backup file
            chown( (char *)backupname, statbuf.st_uid, statbuf.st_gid );

            if( fdcopy( nfd,ofd,statbuf.st_size ) < 0 ) {
                io_error = errno;
                mlwrite("Error copying to backup file");
                sleep(1);
                mlwrite(strerror(io_error));
                close( nfd );
                close( ofd );
                return( FIOSUC );
            }
            close(nfd);

            // ok, backup created and successfully written
            // now reopen the file and copy changed data over it
            // note that the code for the two modes after this open
            // is the same
            nfd = open( (char *)fn, O_WRONLY|O_TRUNC);
            if( nfd < 0 ) {
                io_error = errno;
                mlwrite("Error rewriting original file");
                sleep(1);
                mlwrite(strerror(io_error));
                close( ofd );
                return( FIOSUC );
            }
        } // end update mode code


        // save mode (don't preserve the inode)
        else { 
            s = rename((char *)fn, (char *)backupname);
            if( s < 0 ) {
                io_error = errno;
                mlwrite("Error renaming file");
                sleep(1);
                mlwrite(strerror(io_error));
                return( FIOSUC );
            }
            o_umask = umask(0);  // must be able to recreate any modes
            close(nfd);
            nfd = open((char *)fn,O_WRONLY|O_CREAT|O_EXCL,mode);
            o_umask = umask(o_umask);
            if( nfd < 0 ) {
                io_error = errno;
                mlwrite("Error creating backup file");
                sleep(1);
                mlwrite(strerror(io_error));
                return( FIOSUC );
            }
            chown((char *)fn, statbuf.st_uid,statbuf.st_gid);
        }
    }

    // at this point, nfd is the file descriptor of the 
    // file to receive the new data.

    // if the buffer has a password, it's an encrypted file.
    // encrypt into a new buffer, and use that one to write out. 
    // this isn't military grade encryption; no effort to clear
    // memory or anything like that.
    if( curbp->passwd[0] ) {
#if CRYPT_S
        // for an encrypted file, encrypt the file into a new
        // buffer, and write out that buffer.

        // count lines/chars in the file
        lp = lforw(curbp->lines);
        nch = 0;
        while( lp != curbp->lines ) {    
            nch += llength(lp);
            nch++; // count newline
            lp = lforw(lp);
        }

        buf = (BYTE *)malloc(nch+16);
        bp = buf;
        lp = lforw(curbp->lines);
        while( lp != curbp->lines ) {
            t = lp->text;
            for( i=0;i<llength(lp);i++ ) {
                *bp++ = *t++;
            }
            *bp++ = '\n';
            lp = lforw(lp);
        }
        newbuf = encrypt_buf(curbp->passwd,buf,&nch);
        free(buf);
        putline(nfd,newbuf,nch,0);  // don't add newline -- it's one
                                    // long binary blob
        putline(nfd,NULL,0,0);  // flushes the buffer
        ffget_stat(nfd,2); // update the saved status
        close(nfd);
        free(newbuf);
        mlwrite("file written");
#else
        mlwrite("can't encrypt file on this platform");
        return FALSE;
#endif
    }
    else { // not an encrypted file, so we just write line after line
        // be sure we are at beginning of file...
        if( lseek(nfd,0,SEEK_SET) < 0 ) {
            mlwrite("Seek error during save");
            sleep(1);
            mlwrite(strerror(io_error));
            close( nfd );
             return( FIOSUC );
        }
        lp = lforw(curbp->lines);       // First line.
        nline = 0;                      // Number of lines.
        while (lp != curbp->lines) {
            int has_nl = lp->flags & L_NL;
            int snl    = lp->flags & L_SNL;
            s = putline(nfd, &lp->text[0], llength(lp), has_nl && !snl);
            if (has_nl && snl) s = putline(nfd, " ", 1, 0);
            if (has_nl && !snl) ++nline;
            lp = lforw(lp);
        }
        s = putline(nfd,NULL,0,0);  // flushes the buffer
            // someday should check status
        ffget_stat(nfd,2); // update the saved status
        close(nfd);
        if (nline == 1) {
            mlwrite("[wrote 1 line]");
        }
        else {
            mlwrite("[wrote %d lines]", nline);
        }
    }

//    if(! auto_backup ) unlink((char *)backupname);
    return (TRUE);
}

// copy from one fd to another as fast as you can...
int
fdcopy(int fdsrc, int fdsnk, int nbytes)
{
    char buf[8192];
    int s;
    lseek(fdsrc,0,SEEK_SET);
    lseek(fdsnk,0,SEEK_SET);
    while( (s = read(fdsrc,buf,8192)) > 0 ) {
        s = write(fdsnk,buf,s);
    }
    return( s );
}

/*
 putline: output a line.
      fd - the file discriptor of the file to write to
      s  - the string to put
      z  - number of bytes in s
      nl - 1: add a newline; 0: don't add a newline
 putting 0 bytes guarantees last buffer is flushed
 function has a recursive call; tested with a 600 MB binary
 file, so shouldn't be a problem in normal cases.
 complicated by the "nl" flag.
*/
int
putline(int fd, char *s, int z, int nl)
{
    int ws;
//    putline_buf[IOBUFZ+1] = 0;
    if( z == 0 && nl == 0 ) { // z == 0 isn't sufficient; there might
        if( plb_bytes ) {     // be a single newline ready to write
          ws = write(fd,putline_buf,plb_bytes);
        }
        plbp = putline_buf;
        plb_bytes = 0;
        return FIOSUC;
    }
        
    if( nl ) nl = 1;
    if( z + plb_bytes + nl <= IOBUFZ ) { // string fits
        if( z > 0 ) memcpy(plbp,s,z);
        plbp += z;
        if( nl ) { *plbp++ = '\n'; }
        plb_bytes += z + nl;
        if( putline_buf[IOBUFZ+1] != 0 ) die("putline buf overwritten");
        return FIOSUC;
    }
    else {              // string doesn't fit
        memcpy(plbp,s,(IOBUFZ-plb_bytes));
        s += IOBUFZ-plb_bytes;
        z -= IOBUFZ-plb_bytes;
        ws = write(fd,putline_buf,IOBUFZ);
        plb_bytes = 0;
        plbp = putline_buf;
//        if( putline_buf[IOBUFZ+1] != 0 ) die("putline buf overwritten 1");
        return putline(fd,s,z,nl);
        // this recursion could get quite deep w/ a very long line...
    }
    return FIOSUC ; 
}

/*
The command allows the user to modify the file name associated with the
current buffer.  It is like the "f" command in UNIX "ed".  The operation is
simple; just zap the name in the BUFFER structure, and mark the windows as
needing an update.  You can type a blank line at the prompt if you wish. 
*/
int filename(int f, int n)
{
    WINDOW *wp;
    int    s;
    BYTE   fname[NFILEN];

    (void)defaultargs(f,n);
    if ((s=mlreply("name: ", fname, NFILEN)) == ABORT)
        return (s);
    if (s == FALSE)
        strcpy((char *)curbp->fname, "");
    else
        strcpy((char *)curbp->fname, (char *)fname);
    wp = wheadp;                // Update mode lines.
    while (wp != NULL) {
        if (wp->bufp == curbp)
            wp->flag |= WFMODE;
        wp = wp->wndp;
    }
    return (TRUE);
}
// 
//  Open a file for reading.
//
int
ffropen(BYTE *fn)
{
    int fd;

    mlwrite("[opening (%s)]",fn);
    io_error = 0;
    if( (fd = open( (char *)fn,O_RDONLY ) ) < 0 ) {
        io_error = errno;
        if( io_error == ENOENT ) return FIOERR;
        mlwrite("[file %s unreadable...]",fn);
        sleep(1);
        mlwrite(strerror(io_error));
        sleep(1);
        return FIOERR;
    }

    // status checks, for meaningful errors...
    if( ffget_stat(fd,0) == FIOERR ) return FIOERR;

    if( S_ISDIR(statbuf.st_mode) ) {
        mlwrite("%s: directory",fn);
        return FIOERR;
    }
    if( ! (S_ISREG(statbuf.st_mode)) ) {
        mlwrite("Not a regular file!"); // we can only edit regular files
        return FIOERR;
    }
    strcpy((char *)curfn,(char *)fn);
if( dbug ) {
    mlwrite("%s opened, fd = %d",curfn,fd);
    sleep(1);
}
    return (fd);
}

int ffget_stat(int fd, int flag)
{
    int i;
    long file_sz;       // size of file
    long file_x;        // current position in file

    if ( fstat(fd,&statbuf) != 0 ) {
        mlwrite("stat error: %d, %s",errno,strerror(errno));
        sleep(1);
        return (FIOFNF);
    }
    file_sz = statbuf.st_size;
    file_x = 0;
if( dbug ) {
    mlwrite("stat.. file_sz: %d",file_sz);
    sleep(1);
}
    for( i=0; i<last_file; i++ ) {
        if( statbuf.st_ino == file_data[i]->st_ino ) {
            if( flag == 0 ) {  // reading
                mlwrite("Warning: file (inode %d) already open",
                    statbuf.st_ino);
                sleep(1);
                // update earlier data
                memcpy(file_data[i],&statbuf,sizeof(statbuf));
                return FIOSUC;
            }
            if( flag == 1) {  // writing
                if( statbuf.st_mtime != file_data[i]->st_mtime )  {
                   mlwrite("WARNING: file has been modified externally");
                   memcpy(file_data[i],&statbuf,sizeof(statbuf));
                   return FIOERR;
                }
                return FIOSUC;
            }
            if( flag == 2 ) {  // just update
                memcpy(file_data[i],&statbuf,sizeof(statbuf));
                return FIOSUC;
            }
        }
    }
    if( last_file >= MAXFILES ) {
        mlwrite("Too many files open");
        return FIOERR;
    }
    file_data[last_file] = (struct stat *)malloc(sizeof(statbuf) + 8);
    if( ! file_data[last_file] ) {
        mlwrite("WARNING: memory allocation failure");
        return FIOERR;
    }
    memcpy(file_data[last_file],&statbuf,sizeof(statbuf));
    last_file++;
    return FIOSUC;
}

