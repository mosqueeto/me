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
BYTE backupname[1024];
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

