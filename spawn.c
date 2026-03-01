/*
 * The routines in this file
 * are called to create a subjob running
 * a command interpreter.
 */
#include	<stdio.h>
#include    <string.h>
#include	"ed.h"
#include	<signal.h>

/*
 * Create a subjob with a copy
 * of the command intrepreter in it. When the
 * command interpreter exits, mark the screen as
 * garbage so that you do a full repaint.
 */
int spawncli(int f, int n)
{
	register char *cp;
	char	exec_cmd[80];
	char	*getenv(const char *);

	(void)defaultargs(f,n);
	movecursor(term.nrow, 0);		/* Seek to last line.	*/
	(*term.flush)();
	ttclose();				/* stty to old settings	*/

	strcpy(exec_cmd,"exec /bin/sh");
	
	if( (cp = getenv("SHELL")) != NULL && *cp != '\0' ){
		strcpy(&(exec_cmd[5]),cp);
	}
	putenv("EDITOR_SUBSHELL=ME");
	system(exec_cmd);
	sgarbf = TRUE;
	ttopen();
	return(TRUE);
}

/*
 * Run a one-liner in a subjob.
 * When the command returns, wait for a single
 * character to be typed, then mark the screen as
 * garbage so a full repaint is done.
 * Bound to "C-X !".
 */
int spawn(int f, int n)
{
	register int	s;
	char		line[NLINE];

	(void)defaultargs(f,n);
	if ((s=mlreply("! ", line, NLINE)) != TRUE)
		return (s);
	(*term.putchar)('\n');		/* Already have '\r'	*/
	(*term.flush)();
	ttclose();				/* stty to old modes	*/
	system(line);
//	sleep(2);
	ttopen();
	mlwrite("[End]");			/* Pause.		*/
	(*term.flush)();
	while ((s = (*term.getchar)()) != '\r' && s != ' ')
		;
	sgarbf = TRUE;
	return (TRUE);
}






