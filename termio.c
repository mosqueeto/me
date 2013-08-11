/*
 * The functions in this file
 * negotiate with the operating system
 * for characters, and write characters in
 * a barely buffered fashion on the display.
 * All operating systems.
 */
#include	<stdio.h>
#include	"ed.h"
#ifdef __linux__
#define TERMIOS
#endif
#ifdef _CRAY_
#define TERMIOS
#endif
#ifdef _HPUX_SOURCE
#define TERMIOS
#endif
#include <errno.h>

#ifdef TERMIOS
# include <termios.h>
  static struct termios ostate;
  static struct termios nstate;
#else
#include    <sgtty.h>       /* for stty/gtty functions */
    struct  sgttyb  ostate; /* saved tty state */
    struct  sgttyb  nstate; /* values for editor mode */
#endif

/*
 * This function is called once
 * to set up the terminal device streams.
 */
ttopen()
{
	register int i;
#if 0
/*
 * Redefine cursor keys (as described in DOS Technical Reference Manual
 * p. 2-11, DOS BASIC Manual p. G-6) to mean what the user might expect.
 */
	static char *control[] = {
		"\033[0;72;16p",	/* up    = <ctrl-p>  */
		"\033[0;77;6p",		/* right = <ctrl-f>  */
		"\033[0;75;2p",		/* left  = <ctrl-b>  */
		"\033[0;80;14p",	/* down  = <ctrl-n>  */
		"\033[0;81;22p",	/* pg dn = <ctrl-v>  */
		"\033[0;73;3p",		/* pg up = <ctrl-c>  */
		"\033[0;71;96;60p",	/* home  = `<    */
		"\033[0;79;96;62p",	/* end   = `>    */
		"\033[0;83;127p",	/* del   = del       */
		"\033[0;3;96;46p"	/* <ctrl-@> = `. */
	};
	register char *cp;

	for (i = 0; i < sizeof(control)/sizeof(char *); i++) {
		for (cp = control[i]; *cp; )
			ttputc(*cp++);
	}
#endif

#ifdef TERMIOS
	tcgetattr(2,&ostate);
	tcgetattr(2,&nstate);
	for( i=0;i<NCCS;i++ ) nstate.c_cc[i] = 0;
	nstate.c_iflag = 0;
	nstate.c_oflag = 0;
	nstate.c_cflag = CLOCAL|CS8;
	nstate.c_lflag = 0;
	nstate.c_cc[VMIN] = 1;
	nstate.c_cc[VTIME] = 1;
	tcsetattr(2,TCSADRAIN,&nstate);
#else
	/* not TERMIOS */
	ioctl(0,TIOCGETP,&ostate);		/* save old state */
	ioctl(0,TIOCGETP,&nstate);		/* get base of new state */
	nstate.sg_flags |= RAW;
	nstate.sg_flags &= ~(ECHO|CRMOD);	/* no echo for now... */
	ioctl(0,TIOCSETP,&nstate);		/* set mode */
#endif
	return TRUE;
}

/*
 * This function gets called just
 * before we go back home to the command interpreter.
 */
ttclose()
{
#if 0
/* Redefine cursor keys to default values. */
	static char *control[] = {
		"\033[0;72;0;72p",
		"\033[0;77;0;77p",
		"\033[0;75;0;75p",
		"\033[0;80;0;80p",
		"\033[0;81;0;81p",
		"\033[0;73;0;73p",
		"\033[0;71;0;71p",
		"\033[0;79;0;79p",
		"\033[0;83;0;83p",
		"\033[0;3;0;3p"
	};
	register char *cp;
	register int i;

	for (i = 0; i < sizeof(control)/sizeof(char *); i++) {
		for (cp = control[i]; *cp; )
			ttputc(*cp++);
	}
#endif
#ifdef TERMIOS
	tcsetattr(2,TCSADRAIN,&ostate);
#else
	ioctl(0,TIOCSETP,&ostate);
#endif
	return TRUE;
}

/*
 * Write a character to the display.
 */
ttputc(c)
{
	fputc(c, stdout);
	return TRUE;
}

/*
 * Flush terminal buffer. Does real work
 * where the terminal output is buffered up. A
 * no-operation on systems where byte at a time
 * terminal I/O is done.
 */
ttflush()
{
	fflush(stdout);
	return TRUE;
}

/*
 * Read a character from the terminal,
 * performing no editing and doing no echo at all.
 */
ttgetc()
{
    int c;
    char buf[80];
    c = fgetc(stdin);
    if( c < 0 ) {
        c = errno;
        printf("\n\nerrno = %d\n\n",c);
        die("read on closed file descriptor");
    }
    
//    sprintf(buf,"char was: %c (0x%x)\n",c,c);
//    logit(buf);
    return c;
//	return(fgetc(stdin));
}
