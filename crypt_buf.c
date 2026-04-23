/*
The routines in this file handle the "file" abstraction.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "ed.h"
#include "crypt.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#if CRYPT_S

#define     BLOCKZ 8

extern BYTE *md5string(BYTE *, long);

//blowfish ecb mode
//
// file format: 8 byte blocks
//
//  0       8     16    24    32    40         n-16     n-8      n
// +--------+-----+-----+-----+-----+--- ~~ ---+--------+--------+
// |#ME1.42$|    md5    |flags| IV  |ddd ~~ ddd|dddddd00|0000000n|
// +--------+-----+-----+-----+-----+--- ~~ ---+--------+--------+
//
// last block is all zeros except last byte (n), which is count of pad 
// bytes in previous block (which contains last real data).  in the above,
// there are 6 data bytes (d's) and two pad bytes (0's), and the pad byte
// count (n) would be 2.  The md5 is calculated over the unencrypted data 
// bytes, not counting the pad (not to scale).
//
// there are 40 bytes of header info, and 8 bytes in the final block 
// that are added to the data, plus the pad bytes.


BYTE *
encrypt_buf(BYTE *pw, BYTE *buf, int *bz)
{
    BYTE *nb;
    int  nbz;
    BYTE *nbp;
    BYTE *md5;
    BYTE *md5p;
    int i,j,k,l,m,o,t,extra,pad_size;
    BYTE tmp[16];
    union {
        long t;
        BYTE s[4];
    } iv; 
    Blowfish_Key kk;

    // new, bigger, better, buffer
    nbz = (*bz/8 + 1 ) * 8 + BLOCKZ * 6; 
    nb = (BYTE *)malloc(nbz);
    nbp = nb;

    // set up header info
    strncpy((char *)nbp,ME_MAGIC,BLOCKZ );  // 1 block for magic
    nbp += BLOCKZ;
    md5 = md5string(buf,*bz);
    for( i=0;i<16;i++ ) {           // 2 blocks for md5
        *nbp++ = md5[i];
    }

    // flags, unused for now
    for( i=0;i<8;i++ )              // 1 block
        *nbp++ = '\0';
    
    // set up random IV from /dev/urandom
    {
        int rfd = open("/dev/urandom", O_RDONLY);
        if (rfd < 0 || read(rfd, nbp, BLOCKZ) != BLOCKZ) {
            iv.t = (long)time(0);
            md5 = md5string(iv.s, 4);
            for (i = 0; i < BLOCKZ; i++) nbp[i] = md5[i % 16];
        }
        if (rfd >= 0) close(rfd);
    }
    nbp += BLOCKZ;

    // trailing unaligned bytes out to block boundary
    extra = (*bz)%8;        // bytes past last block boundary
    m = ((*bz) - extra);    // m marks last aligned byte in data
    pad_size = (8 - extra) % 8; // number of zeros to pad with

    Blowfish_ExpandUserKey((BYTE *)pw,strlen((char *)pw),kk);

    // basic cbc

    o = 4*BLOCKZ;   // output buffer starting offset
                    // account for magic, md5, flags, start at iv
    i = 0;          // input buffer offset
    while( i < m ) {
        // xor current iv and input buf into tmp
        for(j=0;j<BLOCKZ;j++) tmp[j] = buf[i++] ^ nb[o++];
        // encrypt temp into new buf
        Blowfish_Encrypt(tmp,&nb[o],kk);
    }

    // now deal with residue.  put the trailing data, 
    // padding, and final block into tmp.  there may be
    // either one or two blocks to encrypt

    k = 0;
    for( j=0;j<extra;j++ ) tmp[k++] = buf[i++]; // might be 0
    for( j=0;j<pad_size;j++) tmp[k++] = '\0';   // might be 0
    for( j=0;j<7;j++ ) tmp[k++] = '\0';
    tmp[k++] = pad_size; // record pad size

    // xor and encrypt first block
    for( j=0;j<BLOCKZ;j++ ) tmp[j] ^= nb[o++];
    Blowfish_Encrypt(tmp,&nb[o],kk);

    // if there was a second block, do it, too
    if( k > 8 ) {
        for( j=0;j<BLOCKZ;j++ ) tmp[j+BLOCKZ] ^= nb[o++];
        Blowfish_Encrypt(&tmp[BLOCKZ],&nb[o],kk);
    }

    *bz = o+BLOCKZ;

    return nb;
}


BYTE *
decrypt_buf(BYTE *pw, BYTE *buf, long *bz)
{
    BYTE *nb;
    BYTE *computed_md5;
    BYTE stored_md5[16];
    BYTE tmp[2*BLOCKZ];
    int nbx, i, j, k, pad_size;
    Blowfish_Key kk;

    nb = (BYTE *)malloc(*bz);
    if (!nb) return NULL;

    memcpy(stored_md5, &buf[8], 16);

    Blowfish_ExpandUserKey((BYTE *)pw, strlen((char *)pw), kk);

    nbx = 0;
    for (i = 40; i < *bz; i += BLOCKZ) {
        Blowfish_Decrypt(&buf[i], &nb[nbx], kk);
        for (j = 0, k = i - 8; j < BLOCKZ; j++, k++)
            nb[nbx++] ^= buf[k];
    }

    pad_size = nb[nbx - 1];
    if (pad_size >= BLOCKZ) {   /* valid range is 0..BLOCKZ-1 */
        free(nb);
        return NULL;
    }
    *bz = (nbx - BLOCKZ - pad_size);
    if (*bz <= 0) {
        free(nb);
        return NULL;
    }

    computed_md5 = md5string(nb, *bz);
    if (memcmp(computed_md5, stored_md5, 16) != 0) {
        free(nb);
        return NULL;
    }
    return nb;
}

#endif

