#include    <stdio.h>
#include    <unistd.h>
#include    <stdlib.h>
#include    <string.h>
#include    <sys/types.h>
#include    <sys/stat.h>
#include    "ed.h"

#define MAXBACKUPS 8
#define BUFZ 65536
#define MAXFILES 256

// never has more than one file open at a time
// ffp is used by all functions

FILE *ffp = (FILE *)NULL;
long file_sz;       // size of file
long file_x;        // current position in file
BYTE *file_buf;     // whole-file buffer
BYTE buf[BUFZ];     // partial file buffer
BYTE *curbuf;
int buf_used;
BYTE curfn[NFILEN];
BYTE oldbackup[128];
int curbackup = 0;

// keep data about all files ever opened
struct stat statbuf;
struct stat *file_data[MAXFILES];
static int last_file = 0;

int ffget_stat(fn,flag)
BYTE * fn;
int flag;
{
    int i;
    if ( stat((char *)fn,&statbuf) != 0 ) return (FIOFNF);
    file_sz = statbuf.st_size;
    file_x = 0;
    for( i=0; i<last_file; i++ ) {
        if( statbuf.st_ino == file_data[i]->st_ino ) {
            if( flag == 0 ) {  // reading
                mlwrite("Warning: file %s (inode %d) already open",
                    fn,statbuf.st_ino);
                // update earlier data
                memcpy(file_data[i],&statbuf,sizeof(statbuf));
                return i;
            }
            if( flag == 1) {  // writing
                if( statbuf.st_mtime != file_data[i]->st_mtime )  {
                   mlwrite("WARNING: file %s has been modified externally",fn);
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
    return last_file;
}

//
//  Open a file for reading.
//
ffropen(fn)
BYTE    *fn;
{
    int f;
    if( ffp != (FILE *)NULL ) {
        fprintf(stderr,"previous file %s not closed\n",curfn);
        return (FIOSUC);
    }
    
    // status checks...
    if( access((char *)fn,F_OK) ) {
        mlwrite("%s: File not readable",fn);
        return FIOFNF;
    }
    if( access((char *)fn,R_OK) ) {
        mlwrite("%s: File not readable",fn);
        return FIOERR;
    }
    if ( f = ffget_stat(fn,0) < 0 ) {
        mlwrite("%s: Can't get file status",fn);
        return (FIOFNF);
    }
    if( S_ISDIR(file_data[f]->st_mode) ) {
        mlwrite("%s: directory",fn,f);
        return FIOERR;
    }
    if( !(S_ISREG(file_data[f]->st_mode)) ) {
        mlwrite("%s: Not a regular file",fn);
        return FIOERR;
    }

    strcpy((char *)curfn,(char *)fn);
    if ((ffp=fopen((char *)fn, "r")) == NULL)
        return (FIOFNF);
    return (FIOSUC);
}
//
//Back up a file.  First figure out a name for the backup, rename the old file
//
ffbackup(fn)
BYTE     *fn;
{
    int nbytes,first;
    FILE *ifp,*ofp;
    BYTE backupname[1024];
    BYTE *p1,*p2;

    if( (nbytes=strlen((char *)fn)) > 1020 ){
        mlwrite("WARNING: backup filename too long");
        sleep(2);
        return( FIOSUC );
    }
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
    if( !strcmp((char *)backupname,(char *)oldbackup) ) return( FIOSUC );

    strcpy((char *)oldbackup,(char *)backupname);
    if( link((char *)fn, (char *)backupname ) {
        mlwrite("WARNING: problem creating backup file");
        sleep(2);
        mlwrite(backupname);
        sleep(4);  // strange error, catch users attention
        return( FIOSUC );
    }
    ifp = fopen((char *)fn,"r");
    if( ifp == (FILE *)NULL ) {
        return( FIOSUC );
    }
    ofp = fopen((char *)backupname,"w");
    if( ofp == (FILE *)NULL ) {
    }

    while( 1 ) {
        nbytes = fread(buf,1,BUFZ,ifp);
        if( nbytes == 0 ) break;
        if( nbytes < 0 ) {
            mlwrite("WARNING: problem writing backup file");
            sleep(4);
            break;
        }
        fwrite(buf,1,nbytes,ofp);
        if( nbytes < BUFZ ) break;
    }
    fclose(ifp);
    fclose(ofp);
}

//
// Open a file for writing.
// Return TRUE if all is well, and
// FALSE on error (cannot create).
//
ffwopen(fn)
BYTE    *fn;
{
    int s;
    if( ffp != (FILE *)NULL ) {
        fprintf(stderr,"previous file %s not closed\n",curfn);
        
        return (FIOERR);
    }
    s = ffget_stat(curfn,1);
    if( s != FIOSUC && s != FIOFNF ) return -1;
    strcpy((char *)curfn,(char *)fn);
    if ((ffp=fopen((char *)fn, "w")) == NULL) {
        mlwrite("Cannot open file for writing");
        return (FIOERR);
    }
    return (FIOSUC);
}

//
// Close a file.
// Should look at the status in all systems.
//
ffclose()
{
    if (fclose(ffp) != FALSE) {
        mlwrite("Error closing file");
        return(FIOERR);
    }
    ffget_stat(curfn,2);
    if( curbuf != (BYTE *)NULL ) {
        free(curbuf);
        curbuf = (BYTE *)NULL;
    }
    ffp = (FILE *)NULL;
    return (FIOSUC);
}

/*
 * Write a line to the already
 * opened file. The "buf" points to the
 * buffer, and the "nbuf" is its length, less
 * the free newline. Return the status.
 * Check only at the newline.
 */
ffputline(buf, nbuf, nl)
BYTE   buf[];
int nbuf;
int nl;
{
    register int    i;

    for (i=0; i<nbuf; ++i)
        putc(buf[i]&0xFF, ffp);
    if( nl ) putc('\n', ffp);
    if (ferror(ffp) != FALSE) {
        mlwrite("Write I/O error");
        return (FIOERR);
    }
    return (FIOSUC);
}

//
// Read a buffer full of stuff from the current file. 
//
int ffreadfile(buf,len)
BYTE *buf;
int len;
{
    int rlen;
    if( file_x == 0L ) { 
        // slurp the file
        file_buf = (BYTE *)malloc(file_sz);
        if( !file_buf ) {
            mlwrite("memory allocation error in fileio.c");
            return (FIOERR);
        }  
        if( fread(file_buf,1,file_sz,ffp) != file_sz ) {
            mlwrite("Couldn't read file");
            return ( FIOERR );
        }
    }
    rlen = file_sz - file_x;
    if( len > rlen ) len = rlen;
    memcpy((void *)buf,&file_buf[file_x],len);
    file_x += len;
    return len;
}
//
// Read an entire file
//
BYTE *
ffslurpfile(len)
long *len;
{
	BYTE *file_buf;
    // slurp the file
    file_buf = (BYTE *)malloc(file_sz);
    if( !file_buf ) {
        mlwrite("memory allocation error in fileio.c");
        return (NULL);
    }  
    if( fread(file_buf,1,file_sz,ffp) != file_sz ) {
        mlwrite("Couldn't read file");
		free(file_buf);
        return ( NULL );
    }
	*len = file_sz;
    return file_buf;
}



#if 0


/*
 * Read a chunk of a file into a buffer, and return the buffer.
 * A null buffer means an error, a zero return value means the
 * buffer is zero long.
 */
int 
ffgetfile(bufp)
BYTE **bufp;
{
    int c;
    int i;
    FILE *fd;

    
    if( curbuf != (BYTE *)NULL ) {
        fprintf(stderr,"buffer not deallocated\n");
        return -1;
    }
    *bufp = (BYTE *)malloc(statbuf.st_size);
    if( *bufp == NULL ) return -1;
    curbuf = *bufp;
    
    i = fread(*bufp,1,statbuf.st_size,ffp);

    while( 1 ) {
        sprintf(backupname,"%%%s.%d",fn,curbackup++);
        mlwrite(backupname);
        sleep(2);
        if( access(backupname,F_OK) == 0 ){
            continue;
        }
    }
    fd = fopen(backupname,"w");
    if( fd == (FILE *)NULL ) {
        mlwrite("WARNING: problem creating backup file");
        sleep(2);
    }

    fwrite((char *)curfn,1,strlen((char *)curfn),fd);
    fwrite("\n",1,strlen("\n"),fd);
    fwrite(*bufp,1,i,fd);
    fclose(fd);
    curbackup = curbackup % MAXBACKUPS;
    return i;
}
#endif
