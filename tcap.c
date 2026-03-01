#include <stdio.h>
#include <stdlib.h>
#include "ed.h"
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define BEL	0x07
#define ESC	0x1B

extern int	ttopen(void);
extern int	ttgetc(void);
extern int	ttputc(int);
extern int	ttflush(void);
extern int 	ttclose(void);
extern int 	tcapmove(int, int);
extern int 	tcapeeol(void);
extern int	tcapeeop(void);
extern int	tcapbeep(void);
extern int	tcapopen(void);
extern int tcaphilight(int);
//extern int	tput();
extern char	*tgoto(const char *, int, int);
extern char	*tgetstr(const char *, char **);
extern int	tgetent(char *, const char *);
extern int	tgetnum(const char *);

#define TCAPSLEN 315

char tcapbuf[TCAPSLEN];
char PC,
	*CM,
	*CL,
	*CE,
	*UP,
	*CD,
    *MB,
    *ME,
    *MH,
    *MR,
    *US,
    *UE;


TERM term = {
	0,
	0,
	tcapopen,
	ttclose,
	ttgetc,
	ttputc,
	ttflush,
	tcapmove,
	tcapeeol,
	tcapeeop,
	tcapbeep,
    tcaphilight
};

tcapopen()
{
	char *t, *p;
	char tcbuf[1024];
	char *tv_stype;
	char err_str[72];
    struct winsize ws;

	if ((tv_stype = getenv("TERM")) == NULL)
	{
		puts("Environment variable TERM not defined!");
		exit(1);
	}

	if((tgetent(tcbuf, tv_stype)) != 1)
	{
		sprintf(err_str, "Unknown terminal type %s!", tv_stype);
		puts(err_str);
		exit(1);
	}

	if ((term.nrow=(short)tgetnum("li")-1) == -1){
	       puts("termcap entry incomplete (lines)");
	       exit(1);
	}
	
	if( p = getenv("ROWS") ) term.nrow = atoi(p)-1;
	if( p = getenv("LINES") ) term.nrow = atoi(p)-1;

	if ((term.ncol=(short)tgetnum("co")) == -1){
	       puts("Termcap entry incomplete (columns)");
	       exit(1);
	}
	
	if( p = getenv("COLS") ) term.ncol = atoi(p);
	if( p = getenv("COLUMNS") ) term.ncol = atoi(p);


    int fd = open("/dev/tty",O_RDWR);
    
    if (ioctl(fd,TIOCGWINSZ,&ws)!=0) {
        perror("ioctl(/dev/tty,TIOCGWINSZ)");
        exit(1);
    }
    term.nrow = ws.ws_row - 1;
    term.ncol = ws.ws_col;

//printf("rows = %i, cols = %i\n",term.nrow,term.ncol);
//sleep(1);

	p = tcapbuf;
	t = tgetstr("pc", &p);
	if(t)
		PC = *t;

	CD = tgetstr("cd", &p);   // clear to end of screen
	CM = tgetstr("cm", &p);   // cursor move to row %1 and column %2
	CE = tgetstr("ce", &p);   // clear to eol
	UP = tgetstr("up", &p);   // up one line
    MB = tgetstr("mb", &p);   // bold mode
    ME = tgetstr("me", &p);   // mode end
    MH = tgetstr("mh", &p);   // mode half-bright
    MR = tgetstr("mr", &p);   // mode reverse video
    US = tgetstr("us", &p);   // start underlining
    UE = tgetstr("ue", &p);   // end underlining
    
	if(CD == NULL || CM == NULL || CE == NULL || UP == NULL ||
       ME == NULL || MR == NULL 
//       MB == NULL || ME == NULL || MH == NULL || MR == NULL 
//       US == NULL || UE == NULL 
      )
	{
		puts("Incomplete termcap entry\n");
		exit(1);
	}

	if (p >= &tcapbuf[TCAPSLEN])
	{
		puts("Terminal description too big!\n");
		exit(1);
	}
	ttopen();
    fputs("\033[?1002h", stdout);   /* enable button-event mouse reporting */
    fflush(stdout);
}

int tcapmove(int row, int col)
{
	putpad(tgoto(CM, col, row));
}

tcapeeol()
{
	putpad(CE);
}

tcapeeop()
{
	putpad(CD);
}

tcapbeep()
{
	ttputc(BEL);
}
tcaphilight( int c )
{
    putpad(MR);ttputc(c);putpad(ME);
}

int putpad(char *str)
{
	tputs(str, 1, ttputc);
}

int putnpad(char *str, int n)
{
	tputs(str, n, ttputc);
}
