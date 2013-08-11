#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "ed.h"
#include "crypt.h"


// must be a multiple of 8 bytes -- 32 is about the minimum that will
// work at all (useful for testing).
//#define BZ 32 
#define BZ 32768

int
die(char *s)
{
	fprintf(stderr,"crypt: %s\n",s	);
	exit(-1);
}


// The following two routines use blowfish in CBC mode.
// The padding at the end of the file adds some complexity
// to the buffer bookkeeping:  the final block of the plaintext
// is padded out with 0's, and another block is added, all 0's
// except the last byte, which contains the number of 0's added to
// the final block.
// 

int
encrypt(char *pw,int in,int out)
{
	Blowfish_Key kk;
	unsigned char buf[BZ+16];
	unsigned char t[8];
	int r,i,j,k,m,n,o,pad_size,done;

	Blowfish_ExpandUserKey(pw, strlen(pw), kk);

	// set up random IV
	r = open("/dev/urandom",O_RDONLY);
	n = read(r,buf,8);
	if( n<8 ) die("couldn't get IV");
	close(r);

	done = 0;
	o = 0;
	while( ! done ) {
		n = read(in,&buf[8],BZ);
		m = n + 8; // account for IV offset

		// if at end, pad to 8 byte boundary
		if( n < BZ ) {
			pad_size = (8 - (n%8))%8;
			for( ;m<n+8+pad_size+7;m++ ) buf[m]='\0';
			buf[m++] = pad_size;
			done = 1;
		}

		// (m always ends at an 8-byte boundary)

		// core CBC
		for(i=8;i<m;i+=8) {  // account for offset
			for(j=i,k=i-8;j<i+8;j++,k++) buf[j] ^= buf[k];
			Blowfish_Encrypt(&buf[i],t,kk);
			for( j=i,k=0;k<8;j++,k++ ) buf[j] = t[k];
		}

		n = write(out,&buf[o],m - o);
		if( n != m - o ) die("write error 1");

		// copy last enc block to be IV for next buffer
		for(j=0,k=i-8;j<8;j++,k++ ) buf[j] = buf[k];
		o = 8; // don't write out the new IV again
	}	
}

int
decrypt(char *pw,int in,int out)
{
	Blowfish_Key kk;
	unsigned char buf[BZ];
	unsigned char t[8];
	int i,j,k,l,m,n,o,pad_size,done,first;

	Blowfish_ExpandUserKey(pw, strlen(pw), kk);

	// the first time through we read from the very beginning
	// later we offset by 8 bytes to account for the IV carry
	// from the previous read
	l = BZ;
	n = read(in,buf,l);
	o = 8; // just past IV
	m = n; // EOI
	while( ! done ) {
		if( n < l ) done = 1;
		for(i=o;i<m;i+=8) {
			Blowfish_Decrypt(&buf[i],t,kk);
			for(j=0,k=i-8;k<i;j++,k++) buf[k] ^= t[j];
		}
		// entire buffer shifted back 1 block over
		// the IV block
		if( done ) {
			i -= 8; // back up from the last ciphertext block
			i -= 1; // back up 1 byte to get pad_size
			pad_size = buf[i]; 
			i -= 7; // back up rest of block of the padsize block
			i -= pad_size; // back up over padding
			m = write(out,buf,i);
			if(m != i ) die("write error 2");
			break;
		}

		// why 24 bytes (3 blocks)?
		// need last encrypted block to serve as IV
		// need last 2 decrypted blocks, in case we
		// need to calculate pad_size from them, because
		// we won't know if this is the end of file until
		// after we attempt another read.
		m = write(out,buf,i-24);
		if(m != i-24 ) die("write error 3");

		//save IV and potential pad blocks
		for( k=0,j=i-24;k<24;j++,k++ ) buf[k] = buf[j];
		l = BZ - 24;
		n = read(in,&buf[24],l);
		o = 24;
		m = n + o; // BZ, in full buffer case
	}
}

int getopt(int argc, char * const argv[],
                  const char *optstring);
extern char *optarg;
extern int optind,opterr, optopt;

int main(argc,argv)
int argc;
char *argv[];
{
	char c;
	char *p;
	char pw1[128];
	char pw2[128];
	char ofname[256];
	int i,in,out,d=0;

	pw1[0] = 0;

	while( (c = getopt(argc, argv, "p:d")) > -1 ) {
		switch (c) {
		case 'p':
			p = optarg;
			strncpy(pw1, p, 127);
			break;
		case 'd':
			d++;
			break;
		default:
			printf("char is %c\n",c);
			usage();
			exit(-1);
		}
	}
	if( argc < 1 ) {
		usage();
		exit(-1);
	}

	if( argc-optind > 2 ) usage();

	if( pw1[0] == 0 ) {
		if( argc-optind < 2 ) 
			die("must supply -p option when using io redirection");
		for(i=0;i<128;i++ ) pw1[i] = '\0';
		for(i=0;i<128;i++ ) pw2[i] = '\0';
		printf("  pw: ");
		// turn off echo
		fgets(pw1,127,stdin);
		if( ! d ) {
			printf("again: ");
			// turn off echo
			fgets(pw2,127,stdin);
			for(i=0;i<128;i++ ) {
				if( pw1[i] != pw2[i] ) die("passwords don't match");
			}
		}
	}

	if( argc-optind == 2 ) {
		if( strlen(argv[optind+1]) > 250 ) 
			die("output file name too long");
		out = open(argv[optind+1],O_WRONLY|O_TRUNC|O_CREAT,0600);
		if( out<0 ) {
			perror("couldn't create output file");
			exit(-1);
		}
	}
	else {
		out = 1; // stdout
	}

	if( argc-optind > 0 ) {
		if( strlen(argv[optind]) > 250 ) 
			die("input file name too long");
		in = open(argv[optind],O_RDONLY);
		if( in < 0 ) {
			perror("couldn't open input file");
			exit(-1);
		}
	}
	else {
		in = 0; // stdin
	}

	if( d ) {
		decrypt(pw1,in,out);
	}
	else {
		encrypt(pw1,in,out);
	}
}

int
usage ()
{
	printf("crypt [-d] [-p password] infile outfile\n");
	exit(-1);
}
