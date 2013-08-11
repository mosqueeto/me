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

#define     BLOCKZ 8

extern int set_mode();
#if CRYPT_S
extern BYTE *md5string();
extern BYTE *encrypt_buf();
extern BYTE *decrypt_buf();
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
makebackupname(fn)
BYTE     *fn;
{
    int nbytes,first;
    BYTE *p1,*p2;

    if( (nbytes=strlen((char *)fn)) > 1020 ){
        mlwrite("WARNING: backup filename too long");
        sleep(2);
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
slurpfile(fd,len)
int fd;
long *len;
{
    BYTE *file_buf;

    if( fstat(fd,&statbuf) != 0 ) {
        io_error = errno;
        mlwrite("Indefined file handle!!");
        sleep(2);
        mlwrite(strerror(io_error));
        return( FIOSUC );
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
    return file_buf;
}

/*
Read a file into the current buffer.  Find the name of the file, 
and call the standard "read a file into the current buffer" code.  
Bound to "C-X C-R". 
*/
filevisit(f, n)
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
Bound to "C-X C-N" ?. 
*/
nextfile(f, n)
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
insfile(f, n)
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
fileread(f, n) 
{
    BYTE   fname[NFILEN];
    int    status;

   (void) defaultargs(f,n);
    if( (status = mlreply("Read file: ", fname, NFILEN)) != TRUE )
        return (status);
    return fileread_1(f,n,fname);

}
/*
Helper function for fileread that does all the real work.
*/
fileread_1(f, n, fname)
int f;
int n;
char * fname;
{
    BUFFER *bp;
    BUFFER *obp;
    WINDOW *wp;
    LINE   *lp;
    int    i;
    int    status;

    BYTE   bname[NBUFN];

    for( bp=bheadp; bp!=NULL; bp=bp->bufp ) {
        if( (bp->flag&BFTEMP)==0 && 
            strcmp((char *)bp->fname, (char *)fname)==0 ) {
            if( f != TRUE ){
                old_buffer(bp);
                return( TRUE );
            }
        }
    }

    makename(bname, fname);   // new buffer name in cannonical form

    // look for existing buf
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
    }

    i=strlen((char *)fname);
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
    //mlwrite("file %s read %d",fname,status);

    return status;
}

/*
Read file "fname" into the buffer, blowing away any text found there. 
Called by both the read and visit commands.  return the final status of the
read.
 */
readin(bufp,fname)
BUFFER *bufp;
BYTE fname[];
{
    LINE   *lp;
    WINDOW *wp;
    BUFFER *bp;
    int  status = TRUE;
    int  i,j;
    int  nbytes;
    int  nline;
    BYTE line[NLINE];
    int  hdrz;
    BYTE *fbp;
    BYTE *file_buf;
    BYTE *decrypted_buf;
    BYTE *nlp;
    BYTE *p;
    BYTE md5[16];
    BYTE chk[16];
    BYTE *d;
    long fz;
    long bz = 0;
    long rlen,len;
    int fd;
    int newfile = 0;
    int nl_flag = 0;

    // prepare buffer
    bp = bufp;
    if( ! ( status = bclear(bp) ) )    // might be old.
        return (FALSE);

    bp->flag &= ~(BFTEMP|BFCHG);
    strcpy((char *)bp->fname, (char *)fname);

    // open file
    if( ( fd = ffropen(fname) ) < 0 ) {
        if( io_error != ENOENT )
        return FIOERR; // ffropen leaves message in status line
    }

    if(  io_error == ENOENT ) {         // file not found.
        mlwrite("[new file]");
        newfile++;
        goto out;
    }

    status = FIOERR;  // assume it failed

    // read the file    
    mlwrite("[reading file]");
    if( ! ( file_buf = slurpfile(fd,&fz) ) ) {
        mlwrite("read error");
        goto out;
    }

    if( fd == 0 ) die("fd=stdin");
    close(fd); // Ignore close errors.
    
    // if it is an encrypted file, try to decrypt it
    bz = fz;  // assume buffer size is same as file size, to start
    if( fz >= 48 && ! strncmp( (char *)file_buf,ME_MAGIC,strlen(ME_MAGIC) ) ) {
#if CRYPT_S
        for( i=0,j=8;i<16;i++,j++ ) md5[i] = file_buf[j];
        p = NULL;
        p = getpw(0);
        if( ! p ) goto out;
        strncpy((char *)bp->passwd,(char *)p,MAXPW);
        decrypted_buf = decrypt_buf(p,file_buf,&bz);
        free(file_buf);

        if( bz <= 0 ) {
            mlwrite("file cannot be decrypted");
            goto out;
        }

        d = md5string(decrypted_buf,bz);
        for( i=0;i<16;i++ ) {
            if( d[i] != md5[i] ) {
                mlwrite("file cannot be decrypted.");
                goto out;
            }
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
        len = rlen;
        nl_flag = L_NL;
        nlp = (BYTE *)memchr((char *)fbp,'\n',len);

        if( nlp == fbp ) { len = 0; } 
        else if( nlp != (BYTE *)NULL ) { len = nlp-fbp; } 
        else { nl_flag = 0; }
        
        // add the line into the BUFFER
        if ((lp=ladd(bp->lines,-1)) == NULL) {
            status = FIOERR; // Keep message on the display.
            goto out;
        }
        lp->text = fbp;
        lp->flags |= (L_EXT|nl_flag);
        lp->size = lp->used = len;
        
        // and update some indices, counters, and flags
        fbp  += len;
        rlen -= len;
        if( nlp != (BYTE *)NULL ) {
            fbp++;  // skip the NL
            rlen--;
            nline++;
        }
    }

    status = TRUE;
out:

    
    if (status == FIOERR) {
       strcpy((char *)bp->fname, "");
       return (FALSE);
    }

    for (wp=wheadp; wp!=NULL; wp=wp->wndp) {
        if (wp->bufp == bp) {
            wp->topp = lforw(bp->lines);
            wp->dotp  = lforw(bp->lines);
            wp->doto  = 0;
            wp->markp = NULL;
            wp->marko = 0;
            wp->flag |= WFMODE;
        }
    }
    if( ! newfile ) mlwrite("[read %d lines (%d bytes)]",nline,fz);
    return (TRUE);
}


/*
Insert file "fname" into the current buffer, Called by insert file command.
Return the final status of the read.
 */
ifile(fname)
BYTE fname[];
{
    LINE   *lp;
    WINDOW *wp;
    BUFFER *bp;
    int  status = TRUE;
    int  i,j;
    int  nbytes;
    int  nline;
    BYTE line[NLINE];
    int  hdrz;
    BYTE *fbp;
    BYTE *file_buf;
    BYTE *decrypted_buf;
    BYTE *nlp;
    BYTE *p;
    BYTE md5[16];
    BYTE chk[16];
    BYTE *d;
    long fz;
    long bz = 0;
    long rlen,len;
    int fd;
    int newfile = 0;
    int nl_flag = 0;

    // prepare buffer
    bp = curbp;
    bp->flag |= BFCHG; // we have changed
    text_chg_flag++;   // disable dot macro

    // open file
    if( ( fd = ffropen(fname) ) < 0 ) {
        if( io_error != ENOENT )
        return FIOERR; // ffropen leaves message in status line
    }

                
    mlwrite("[inserting file]");

    // back up a line and save the mark here
    curwp->dotp = lback(curwp->dotp);
    curwp->doto = 0;
    curwp->markp = curwp->dotp;
    curwp->marko = 0;

@@@@    
    // advance to the next line and mark the window for changes
    curwp->dotp = lforw(curwp->dotp);
    curwp->flag |= WFMODE;
        
    // copy window parameters back to the buffer structure
    curbp->dotp = curwp->dotp;
    curbp->doto = curwp->doto;
    curbp->markp = curwp->markp;
    curbp->marko = curwp->marko;

    close(fd);     
    if( status == FIOERR ){
        return (FALSE);
    }
     mlwrite("[read %d lines (%d bytes)]",nline,fz);
    return (TRUE);

}

int do_read()
{

    status = FIOERR;  // assume it failed

    // read the file    
    mlwrite("[reading file]");
    if( ! ( file_buf = slurpfile(fd,&fz) ) ) {
        mlwrite("read error");
        goto out;
    }
    close(fd); // Ignore close errors.
    
    // if it is an encrypted file, try to decrypt it
    bz = fz;  // assume buffer size is same as file size, to start
    if( fz >= 48 && ! strncmp( (char *)file_buf,ME_MAGIC,strlen(ME_MAGIC) ) ) {
#if CRYPT_S
        for( i=0,j=8;i<16;i++,j++ ) md5[i] = file_buf[j];
        p = NULL;
        p = getpw(0);
        if( ! p ) goto out;
        strncpy((char *)bp->passwd,(char *)p,MAXPW);
        decrypted_buf = decrypt_buf(p,file_buf,&bz);
        free(file_buf);

        if( bz <= 0 ) {
            mlwrite("file cannot be decrypted");
            goto out;
        }

        d = md5string(decrypted_buf,bz);
        for( i=0;i<16;i++ ) {
            if( d[i] != md5[i] ) {
                mlwrite("file cannot be decrypted.");
                goto out;
            }
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
        if ((lp=ladd(curwp->dotp->fp,-1)) == NULL) {
            status = FIOERR; // Keep message on the display.
            goto out;
        }
        curwp->dotp = lp;

        lp->text = fbp;
        lp->flags |= (L_EXT|nl_flag);
        lp->size = lp->used = len;

        
        
        // and update some indices, counters, and flags
        fbp  += len;
        rlen -= len;
        if( nlp != (BYTE *)NULL ) {
            fbp++;  // skip the NL
            rlen--;
            nline++;
        }
    }

    status = TRUE;
out:


}

/*
Take a file name, and from it fabricate a buffer name.
*/
makename(bname, fname)
BYTE    bname[];
BYTE    fname[];
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

int reread(f,n)
{
    (void)defaultargs(f,n);
    fileread(TRUE,n);
}

int old_buffer(bp)
BUFFER  *bp;
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
filewrite(f, n)
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
filesave(f, n)
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

This compliated function performs the details of file writing.  The algorithm
is more complicate than it should be, because I would like a modified file to
be able to have a different inode.  This is to ensure that backups made by
hard links will discover new files. 

There are 3 cases: "create" (new file); "update" (no change in inode); and
"save" (change inode).

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

writeout(fn,update)
BYTE    *fn;
int     update;
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

        // and need the mode bits from the original file...
        ffget_stat(nfd,1);

        if( ! (S_ISREG(statbuf.st_mode)) ) {
            mlwrite("Not a regular file!"); // we can only update regular
            return( FIOSUC );               // files
        }
        mode = statbuf.st_mode & 
            (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO);
        
        // preserve the inode?
        if( update ) {
            unlink((char *)backupname);
            o_umask = umask(0);  // must be able to recreate any modes
            ofd = open( (char *)backupname, O_WRONLY|O_CREAT|O_EXCL, mode );
            o_umask = umask(o_umask);
            if( ofd < 0 ) {
                io_error = errno;
                mlwrite("Error creating backup file");
                sleep(2);
                mlwrite(strerror(io_error));
                return( FIOSUC );
            }
            chown( (char *)backupname, statbuf.st_uid, statbuf.st_gid );
            nfd = open( (char *)fn, O_RDWR);
            if( nfd < 0 ) {
                io_error = errno;
                mlwrite("Error opening original file");
                sleep(2);
                mlwrite(strerror(io_error));
                close( ofd );
                return( FIOSUC );
            }
            if( fdcopy( nfd,ofd,statbuf.st_size ) < 0 ) {
                io_error = errno;
                mlwrite("Error copying to backup file");
                sleep(2);
                mlwrite(strerror(io_error));
                close( nfd );
                close( ofd );
                return( FIOSUC );
            }
            close(nfd);
            // now reopen the file and rewrite it
            nfd = open( (char *)fn, O_WRONLY|O_TRUNC);
            if( nfd < 0 ) {
                io_error = errno;
                mlwrite("Error rewriting original file");
                sleep(2);
                mlwrite(strerror(io_error));
                close( ofd );
                return( FIOSUC );
            }
        }
        // save (don't preserve the inode)
        else { 
            s = rename((char *)fn, (char *)backupname);
            if( s < 0 ) {
                io_error = errno;
                mlwrite("Error renaming file");
                sleep(2);
                mlwrite(strerror(io_error));
                return( FIOSUC );
            }
            o_umask = umask(0);  // must be able to recreate any modes
            nfd = open((char *)fn,O_WRONLY|O_CREAT|O_EXCL,mode);
            o_umask = umask(o_umask);
            if( nfd < 0 ) {
                io_error = errno;
                mlwrite("Error creating backup file");
                sleep(2);
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
        putline(nfd,newbuf,nch,0);  // don't add newline -- it's binary
        putline(nfd,NULL,0,0);  // flushes the buffer
        close(nfd);
        free(newbuf);
        mlwrite("file written");
#else
        mlwrite("can't encrypt file on this platform");
        return FALSE;
#endif
    }
    else { // not an encrypted file
        // be sure we are at beginning of file...
        if( lseek(nfd,0,SEEK_SET) < 0 ) {
            mlwrite("Seek error during save");
            sleep(2);
            mlwrite(strerror(io_error));
            close( nfd );
             return( FIOSUC );
        }
        lp = lforw(curbp->lines);       // First line.
        nline = 0;                      // Number of lines.
        while (lp != curbp->lines) {
            s = putline(nfd,&lp->text[0], llength(lp),(lp->flags&L_NL));
            ++nline;
            lp = lforw(lp);
        }
        s = putline(nfd,NULL,0,0);  // flushes the buffer
            // someday should check status
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
fdcopy(fdsrc,fdsnk,nbytes)
int fdsrc;
int fdsnk;
int nbytes;
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
putline(fd,s,z,nl) 
int fd;
char *s;
int z;
int nl;
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
filename(f, n)
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
ffropen(fn)
BYTE    *fn;
{
    int fd;

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
    ffget_stat(fd,0);

    if( S_ISDIR(statbuf.st_mode) ) {
        mlwrite("%s: directory",fn);
        return -1;
    }
    if( ! (S_ISREG(statbuf.st_mode)) ) {
        mlwrite("Not a regular file!"); // we can only edit regular files
        return( -1 );
    }
    strcpy((char *)curfn,(char *)fn);
    return (fd);
}

int ffget_stat(fd,flag)
int fd;
int flag;
{
    int i;
    long file_sz;       // size of file
    long file_x;        // current position in file

    if ( fstat(fd,&statbuf) != 0 ) return (FIOFNF);
    file_sz = statbuf.st_size;
    file_x = 0;
    for( i=0; i<last_file; i++ ) {
        if( statbuf.st_ino == file_data[i]->st_ino ) {
            if( flag == 0 ) {  // reading
                mlwrite("Warning: file (inode %d) already open",
                    statbuf.st_ino);
                // update earlier data
                memcpy(file_data[i],&statbuf,sizeof(statbuf));
                return i;
            }
            if( flag == 1) {  // writing
                if( statbuf.st_mtime != file_data[i]->st_mtime )  {
                   mlwrite("WARNING: file has been modified externally");
                   memcpy(file_data[i],&statbuf,sizeof(statbuf));
                   return FIOERR;
                }
                return i;
            }
            if( flag == 2 ) {  // just update
                memcpy(file_data[i],&statbuf,sizeof(statbuf));
                return i;
            }
        }
    }
    if( i >= MAXFILES ) {
        mlwrite("Too many files open");
        return -1;
    }
    file_data[i] = (struct stat *)malloc(sizeof(statbuf) + 8);
    if( ! file_data[i] ) {
        mlwrite("WARNING: memory allocation failure");
        return -1;
    }
    memcpy(file_data[i],&statbuf,sizeof(statbuf));
    last_file = i;
    return 0;
}

