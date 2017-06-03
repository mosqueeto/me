/*

This program is based on the original public domain micro emacs, 
written by Dave G. Conroy.

*/

/*
Version History:

1.2 -- fixed up control characters in macros
1.3 -- better UNIX "spawncli"
1.4 -- fixed listbuffers, added "i", "/" to vi mode
1.5 -- added "\" to vi mode, started a simple help reminder...
1.6 -- added ESC-ESC to enter vi mode, deleted ESC-V and ESC-B
1.7 -- modified termio.c to use termios on linux
1.8 -- added "x", "s", and "A" to vi mode
1.9 -- fixed kill for long lines
1.10-- backup logic; CTLX-CTL-I to insert a file
1.11-- changed backupname to "%filename.nn"
1.12-- changed backup logic to only backup on write, name is now
       "filename.~n~", and "filename.~~n" for a hangup
       signal.  Also changed default fill column
1.13-- changed backup logic once again -- name is now "filename.~"
       In practice the many backups became cumbersome and
       dangerous.  Also fixed a long standing and irritating
       bug in the way word wrap worked.
1.14-- added magic patterns, query replace
1.15-- tuned up bug fix in 1.13.
1.16-- tuned up tuneup in 1.15.
1.17-- fixed a couple of wordwrap problems, cray termios added,
       added META [ and META ] for macro definitions
1.18-- fixed up termio.c to deal with coral, and improved it
1.19-- changed backup name to ",,fname"; moved around version logic
       "rm" and "lm" as synonyms for "rmarg" and "lmarg",
       now set Environment var "EDITOR_SUBSHELL=ME", so prompt
       can change in subshells, wrapword honors left margin,
       fillpara bug fixed, added "reread", changed key
       binding for killregion
1.19a- initialized kbdmin to null -- bug uncovered on wren
1.19b- subtle bug in lscrunch
1.19c- added "setindent", bound to META|I
1.20-- fixed setinent, wordwrap bug
1.20a- more on wordwrap bug, break para on indent or ">"
1.20b- I don't like break para on indent
1.20c- fill para now leaves cursor at end of last line
1.20d- fill para breaks on C comments, "- ", and "o "
1.20e- make setindent also set the tab stop
1.21-- changed automatic backup to only work if root
1.21a- fixed bug -- endless loop in fillpara with 'bullets'
1.22-- special treatement of 'esc' in mlreply
1.23- always write backups for all files on sighup, also fixed up
      forwchar, backchar, gotoeol when L_NL is set
1.24-- I don't like <space> -> new page in vi mode
1.25-- I don't like <space> -> <space> in vi mode
1.26-- default tabs to 4 spaces
1.27-- 'm' -> backpage
1.28-- perl comments, other chars now exit para fill
1.29-- hard-coded default rmarg at 72, C_tabs var
1.30-- bounds checking on vtputc, 'b' backpage in vi mode
1.31-- long lines problem in vputc, linsertc
1.32-- ditto, modified display.c
1.33-- backslash escapes in searches
1.34-- removed MSDOS/UNIX defines
1.35-- added code to fileio.c to read in the entire file
1.36-- added getopt, and multiple filenames on command line
1.37-- added support for arrow and pgup/pgdown keys.  also made
       chars after meta key case sensitive, so can use cap letters
       for rare commands
1.38-- added automatic showcpos for page movement commands
1.39-- rudimentary save and restore of keyboard macros
1.40-- added support for function keys, cleaned up character input 
       processing
1.41-- changed tab processing
1.42-- added first part of encryption
1.43-- more encryption, clean up error processing on file read
1.44-- fixed "getgoal" to deal properly with variable tab sizes
1.45-- added "help", slight revamp of getkey()
1.45x-- use the input buffer for line storage....
1.46-- <esc>i+, <esc>i-, <esc>i=
1.47-- fixed longstanding bug in pattern matching routines, cleanup
1.48-- segfault in decrypt with bad input
1.49-- . macro?
1.50-- start of external change detection
1.51-- fixed bug in external change detection
1.52-- another bug in external change detection
1.53-- change initial rmarg to line_max-8
1.54-- mods to showcpos
1.55-- added support to resize windows
1.56-- redid the line code back to 1 line per text line
1.57-- major rewrite of display code to do line folding in the display
       rather than in the line store.  also fixed an old bug having to 
       do with word wrap at the end of the buffer.
1.58-- cleanup of above code, decided to remove screen update optimizations
       because they don't seem to have a performance implication in practice
1.59-- fixed serious oversight in case where line ends exactly at right edge 
        of screen
1.60-- took out the "ISTRIP" from tcsetattr call in termio.c - allow chars
       w/ high bit set
1.61-- unexpected consequence of the above was to change the way newline
        is returned -- used to be a ^M, now a ^J.  this was caused because
        I didn't correctly initilize the "nstate.c_iflag" value to zero
        after removing the ISTRIP flag.  Also fixed the indent code to
        not insert tabs,
1.62-- problem with long lines with tabs -- loses count of virtual offset
1.63-- added hex representation of character in mode line
1.64-- unconfigured logging...
1.65-- little bit of clean up, nothing functional, hopefully
1.66-- fixed big delete problem w/ killbuffer -- pre-allocate a big killbuffer
        if the delete is large (ldelete)
1.67-- turn off dot macro if any other text changes have occured.
1.68-- mlwrite an error when can't open a file.  still need to not change
        the name of the buffer when an open fails...
1.69-- increase the maximum size of the "word" buffer in fillpara to 1024
        so long urls don't get truncated.  (still needs work)
1.70-- forgot bp argument to 2 calls to readin.
1.71-- fixed an introduced bug -- opening a non-existent file now says 
        "[new file]"
1.72-- changed backup file algorithm.  now any changes result in a new 
        inode, and, if root, a backup is created at each write.  ? should 
        a backup only be created on the *first* write
1.73-- changed file write logic to preserve permissions while also changing
        inode.  major change
1.74-- show number of bytes read on completion of a read, lots of bug fixes 
        for new write logic -- putline had problems, last of which concerned 
        the special case of a file w/ no terminating newline.  Needed
        to revive the L_NL flag, which seriously complicated the code.
1.75-- L_NL introduced a bug in a brand new file; fixed by addeding a 
        "lnewline - backchar" sequence to linsert in the special
        case of an empty new file.  unsatisfactory
1.76-- fixed ifile to not use fileio.c -- wasn't working at all (segfault)
       fixed loop bug in display code.  turned out that the bug was in
        file.c; closed fd 0 (stdin), and so couldn't read it any more
        so got an endless loop...
1.77-- tried to fix . macro to not work when defining a macro or running a 
        macro 
1.78-- fixed a bug in the crypto -- need to do a null putline to be sure 
        buffer is flushed
1.79-- went back to the inode-preserving mode in 1.73 -- the code maintained
        that as an option in function "writeout".  However, need to 
        test this a bit...
1.80-- think I fixed the annoying "initial insert into a blank file" bug
1.81-- separated buffer encryption code into separate file.
1.82-- now use perl regular expressions
1.83-- refactored duplicate code out of readin and ifile
1.84-- check stat of file to be written, complain if it has changed
1.85-- make case insensitive search default; put caps in pattern to match 
        case (no way to force lower case match, though)
1.86-- bug fix -- file metadata wasn't updated on write, so the next write 
        would fail with "file changed externally" error cuz WE changed the 
        file, but kept the old recorded stat data
1.87-- bug fix -- do_read segfaulted because bp wasn't passed in
1.88-- ffget_stat call to update file state after write -- only encrypted case
1.89-- added meta-? as help
1.90-- changed esc-e (encryptb) to esc-E, consistent w/ esc-D
1.91-- recoded mlwrite to use varargs -- should have done long ago...
1.92-- fixed bug -- insert file failed if inserting into newly created
        file.  changed insert logic to leave at start of insert.  added
        L_HEAD flag for the dummy line, not using it though
1.93-- expanded help system.
*/
#define VERSION_NAME "ME1.93"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pwd.h>
#include <termios.h>
#include <libgen.h>

#define ED_MAIN 1
#include    "ed.h"


#ifndef GOOD
#define GOOD    0
#endif

int vi_mode = FALSE;
typedef struct  {
    int k_code;         // Key code
    int (*k_fp)();      // Routine to handle it
    char *name;         // lot's of literal string assignments here
	char *help;
}   KEYTAB;

extern  int backchar();     // Move backward by characters
extern  int backdel();      // Backward delete
extern  int backline();     // Move backward by lines
extern  int backpage();     // Move backward by pages
extern  int backsearch();   // Search backwards
extern  int backword();     // Backup by words
extern  int bufchars();		// count characters in buffer
extern  int capword();      // Initial capitalize word.
extern  int clr_vi();       // turn off vi mode
extern  int copyregion();   // Copy region to kill buffer.
extern  int ctrlg();        // Abort out of things
extern  int ctlxe();        // Execute macro
extern  int ctlxedot();     // Execute macro
extern  int ctlxlp();       // Begin macro
extern  int ctlxrp();       // End macro
extern  int deblank();      // Delete blank lines
extern  int decryptb();     // Save a decrypted version of a buffer
extern  int defaultargs();  // verify that f and n are default values
extern  int delbword();     // Delete backward word.
extern  int delfword();     // Delete forward word.
extern  int dot();          // dot macro
extern  int enlargewind();  // Enlarge display window.
extern	int	encryptb();		// Encrypt a buffer to a file
extern  int filename();     // Adjust file name
extern  int fileread();     // Get a file, read write
extern  int filesave();     // Save current file
extern  int filevisit();    // Get a file, read only
extern  int filewrite();    // Write a file
extern  int fillpara();     // fill a paragraph.
extern  int forwchar();     // Move forward by characters
extern  int forwdel();      // Forward delete
extern  int forwline();     // Move forward by lines
extern  int forwpage();     // Move forward by pages
extern  int forwsearch();   // Search forward
extern  int forwword();     // Advance by words
//extern int    getccol();  // Get current column
extern  int getvcol();      // Get virtual column
extern  int gotobob();      // Move to start of buffer
extern  int gotobol();      // Move to start of line
extern  int gotoeob();      // Move to end of buffer
extern  int gotoeol();      // Move to end of line
extern  int gotoline();     // Go to a line number
extern	int help();         // print reminder
extern  int helpkeys();     // complete list of key bindings
extern  int indent();       // Insert CR-LF, then indent
extern  int insfile();      // Insert file at current line
extern  int key_pageup();   // pageup key
extern  int key_pagedown(); // pagedown key
extern  int key_left();     // cursor left key
extern  int key_right();    // cursor right key
extern  int key_up();       // cursor up key
extern  int key_down();     // cursor down key
extern  int key_f1();       // f1 key
extern  int key_f2();       // f2 key
extern  int key_f3();       // f3 key
extern  int key_f4();       // f4 key
extern  int key_f5();       // f5 key
extern  int key_f6();       // f6 key
extern  int key_f7();       // f7 key
extern  int key_f8();       // f8 key
extern  int key_f9();       // f9 key
extern  int key_f10();      // f10 key
extern  int key_f11();      // f11 key
extern  int key_f12();      // f12 key
extern  int killfw();       // Kill forward
extern  int killbuffer();   // Make a buffer go away.
extern  int killregion();   // Kill region.
extern  int listbuffers();  // Display list of buffers
extern  int lowerregion();  // Lower case region.
extern  int lowerword();    // Lower case word.
extern  int mvdnwind();     // Move window down
extern  int mvupwind();     // Move window up
extern  int nextfile();     // Move to the next file given on command line
extern  int nextwind();     // Move to the next window
extern  int newline();      // Insert CR-LF
extern  int onlywind();     // Make current window only one
extern  int openline();     // Open up a blank line
extern  int pack();         // pack a buffer
extern  int prevwind();     // Move to the previous window
extern  int quit();         // Quit
extern  int quote();        // Insert literal
extern  int qreplace();     // query replace
extern  int refresh();      // Refresh the screen
extern  int reread();       // Reread a file
extern  int reposition();   // Reposition window
extern  int set_vi();       // set vi mode
extern  int setrmarg();     // Set right fill column.
extern  int setindent();    // set the indent
extern  int setmark();      // Set mark
extern  int setvar();       // set a named variable.
extern  int showcpos();     // Show the cursor position
extern  int shrinkwind();   // Shrink window.
extern  int spawn();        // Run a command in a subjob.
extern  int spawncli();     // Run CLI in a subjob.
extern  int splitwind();    // Split current window
extern  int swapmark();     // Swap "." and mark
extern  int twiddle();      // Twiddle characters
extern  int tab();          // Insert tab
extern  int upperregion();  // Upper case region.
extern  int upperword();    // Upper case word.
extern  int usebuffer();    // Switch a window to a buffer
extern  int vi_A();         // "A" vi command
extern  int vi_s();         // "s" vi command
extern  int yank();         // Yank back from killbuffer.

extern  int logit();        // Log a message to the log file, /tmp/log.me
extern  int die();
extern void resize();

//extern  BYTE *strcpy();  // copy string.
//extern  BYTE *ffslurp(); // read an entire file
/*
  Command table.
  This table  is *roughly* in ASCII
  order, left to right across the characters
  of the command. This expains the funny
  location of the control-X commands.
*/
KEYTAB  keytab[] = {
'.',            dot,       "dot",
    ".       either . macro or insert a .",
CNTRL|'@',      setmark,   "setmark",
    "^@      setmark",
CNTRL|'A',      gotobol,   "gotobol",
    "^A      goto beginning of line",
CNTRL|'B',      backchar,  "backchar",
    "^B      back character",
CNTRL|'C',      backpage,  "backpage",
    "^C      back page",
CNTRL|'D',      forwdel,   "forwdel",
    "^D      forward delete character",
CNTRL|'E',      gotoeol,   "gotoeol",
    "^E      goto end of line",
CNTRL|'F',      forwchar,  "forwchar",
    "^F      forward character",
CNTRL|'G',      ctrlg,     "abort",
    "^G      abort",
CNTRL|'H',      backdel,   "backdel",
    "^H      backward delete character",
CNTRL|'I',      tab,       "tab",
    "^I      insert tab",
CNTRL|'J',      indent,    "indent",
    "^J      indent",
CNTRL|'K',      killfw,    "killfw",
    "^K      kill line",
CNTRL|'L',      refresh,   "refresh",
    "^L      refresh screen",

// available for "match"??

CNTRL|'M',      newline,   "newline",
    "^M      insert newline",
CNTRL|'N',      forwline,  "forwline",
    "^N      forward line",

// maybe this should be "options"??
//CNTRL|'O',      openline,  "openline","open a following line",

CNTRL|'O',      indent,    "indent",
    "^O      indent",

CNTRL|'P',      backline,  "backline",
    "^P      backward line",
CNTRL|'Q',      quote,     "quote",
    "^Q      quote following character",
CNTRL|'R',      backsearch,"backsearch",
    "^R      reverse search",
CNTRL|'S',      forwsearch,"forwsearch",
    "^S      forward search",
CNTRL|'T',      twiddle,   "twiddle",
    "^T      twiddle characters at point",
CNTRL|'V',      forwpage,  "fowrpage",
    "^V      forward page",
CNTRL|'Y',      yank,      "yank",
    "^Y      yank text back from kill buffer",
CNTRL|'Z',      spawncli,  "spawncli",
    "^Z      spawn shell",
CTLX|CNTRL|'B', listbuffers, "listbuffers",
    "^X^B    list buffers",
CTLX|CNTRL|'C', quit,      "quit",
    "^X^C    quit",
CTLX|CNTRL|'F', fileread,  "fileread",
    "^X^F    read file",
CTLX|CNTRL|'I', insfile,   "insfile",
    "^X^I    insert file",
CTLX|CNTRL|'L', lowerregion, "lowerregion",
    "^X^L    lowercase marked region",
CTLX|CNTRL|'M', deblank,   "delblank",
    "^X^M    delete blank lines",
CTLX|CNTRL|'O', deblank,   "delblank",
    "^X^O    delete blank lines",
CTLX|CNTRL|'N', mvdnwind,  "mvdnwind",
    "^X^N    move window down one line",
CTLX|CNTRL|'P', mvupwind,  "mvupwind",
    "^X^P    move window up one line",
CTLX|CNTRL|'R', filename,  "filename",
    "^X^R    change file name",
CTLX|CNTRL|'S', filesave,  "filesave",
    "^X^S    save file",
CTLX|CNTRL|'U', upperregion, "upperregion",
    "^X^U    uppercase marked region",
CTLX|CNTRL|'V', filevisit, "filevisit",
    "^X^V    visit file",
CTLX|CNTRL|'W', killregion, "killregion",
    "^X^W    kill region (wipe)",
CTLX|CNTRL|'X', swapmark,   "swapmark",
    "^X^X    swap mark",
CTLX|CNTRL|'Z', shrinkwind, "shrinkwind",
    "^X^Z    shrink window",
CTLX|'!',       spawn,      "spawn",
    "^X!     spawn shell",
CTLX|'=',       showcpos,   "showcpos",
    "^X=     show current position",
CTLX|'(',       ctlxlp,     "ctlxlp",
    "^X(     open macro",
CTLX|')',       ctlxrp,     "ctlxrp",
    "^X)     close macro",
CTLX|'[',       ctlxlp,     "ctlxlp",
    "^X[     open macro",
CTLX|']',       ctlxrp,     "ctlxrp",
    "^X]     close macro",
CTLX|'1',       onlywind,   "onlywind",
    "^X1     one window",
CTLX|'2',       splitwind,  "splitwind",
    "^X2     two windows",
CTLX|'b',       usebuffer,  "usebuffer",
    "^Xb     use buffer",
CTLX|'e',       ctlxe,      "ctlxe",
    "^Xe     execute macro",
CTLX|'f',       setrmarg,   "setrmarg",
    "^Xf     set right margin",
CTLX|'g',       enlargewind, "enlargewind",
    "^Xg     grow window",
CTLX|'k',       killbuffer, "killbuffer",
    "^Xk     kill buffer",
CTLX|'n',       nextwind,   "nextwind",
    "^Xn     next window",
CTLX|'p',       prevwind,   "prevwind",
    "^Xp     previous window",
CTLX|'x',       swapmark,   "swapmark",
    "^Xx     swap mark",
META|CNTRL|'H', delbword,    "delbword",
    "M-^H    delete backward one word",	
META|CNTRL|'[', set_vi,      "set_vi",
    "M-^[    set vi mode",
META|CNTRL|'N', nextfile,    "nextfile",
    "M-^N    next file",
META|CNTRL|'R', reread,      "reread",
    "M-^R    reread file",
META|CNTRL|'W', filewrite,   "filewrite",
    "M-^W    write file",
META|' ',       setmark,     "setmark",
    "M-Space set mark",
META|'.',       ctlxedot,    "ctlxedot",
    "M-.     set dot macro",
META|'!',       reposition,  "reposition",
    "M-!     reposition",
META|'>',       gotoeob,     "gotoeob",
    "M->     goto end of buffer",
META|'<',       gotobob,     "gotobob",
    "M-<     goto beginning of buffer",
META|'[',       ctlxlp,      "ctlxlp",
    "M-[     open macro",
META|']',       ctlxrp,      "ctlxrp",
    "M-]     close macro",
META|'B',		bufchars,    "bufchars",
    "M-B     change buffer",
META|'c',       capword,     "capword",
    "M-c     capitalize word",
META|'d',       delfword,    "delfword",
    "M-d     delete forward word",
META|'D',       decryptb,    "decryptb",
    "M-D     decrypt buffer",
META|'E',		encryptb,    "encryptb",
    "M-E     encrypt buffer",
META|'f',       forwword,    "forwword",
    "M-f     forward word",
META|'h',       helpkeys,    "helpkeys",
    "M-h     print key mappings",
META|'i',       setindent,   "setindent",
    "M-i     set indent",
META|'l',       lowerword,   "lowerword",
    "M-l     lowercase word",
META|'n',       gotoline,    "gotoline",
    "M-n     goto line number",
META|'p',       pack,        "pack",
    "M-p     pack buffer",
META|'q',       fillpara,    "fillpara",
    "M-q     fill paragraph",
META|'r',       qreplace,    "qreplace",
    "M-r     query replace",
META|'s',       setvar,      "setvar",
    "M-s     set value of variable",
META|'u',       upperword,   "upperword",
    "M-u     uppercase word",
META|'v',       set_vi,      "set_vi",
    "M-v     set vi(ew) mode",
META|'w',       copyregion,  "copyregion",
    "M-w     copy region",
META|'?',       help,        "help",
    "M-?     explain following key",
META|0x7F,      delbword,    "delbword",
    "M-DEL   delete backward word",
0x7F,           backdel,     "backdel",
    "DEL     delete backward character",
0,              0, 0, 0
};

#define NKEYTAB (sizeof(keytab)/sizeof(keytab[0]))

KEYTAB vi_keytab[] = {
'h',        backchar, "backchar","backward one character (vi)",
'i',        clr_vi,  "clr_vi",	"leave vi mode (vi)",
'j',        forwline, "forwline","forward one line (vi)",
'k',        backline, "backline","backward one line (vi)",
'l',        forwchar, "forwchar","forward one character (vi)",
'n',        forwpage, "forwpage","forward one page (vi)",
'm',        backpage, "backpage","backward one page (vi)",
'u',        backpage, "backpage","backward one page (vi)",
'b',        backpage, "backpage","backward one page (vi)",
'x',        forwdel, "forwdel","forward delete (vi)",
'A',        vi_A, "vi_A","append at end of line (vi)",
's',        vi_s, "vi_s","substitute (vi)",
'q',        quit, "quit","quit (vi)",
'/',        forwsearch, "forwsearch","forward search (vi)",
'\\',       backsearch, "backsearch","backward search (vi)",
CNTRL|'G',  clr_vi, "clr_vi","leave vi(ew) mode (vi)",
0,  0, 0, 0
};
static resize_window = 0;
static int lookahead = 0;
static int t47;
static int do_log = 0;
BYTE log_buf[256];
BYTE rc_dir[256];
BYTE kbm_file[256];
int running_macro_flag = 0;
BYTE *help_txt;

void sig_handler(i)
int i;
{
    BYTE fname[80];
    BYTE s[80];

    curbp = bheadp;
    while( curbp != NULL ) {
        if( (curbp->flag&BFTEMP) == 0 ) {   // Real file.
            strncpy((char *)s,(char *)curbp->fname,80);
            sprintf((char *)fname,"%s/~~%s",
                    dirname((char *)s),basename((char *)s));
            i++; // avoid warning?
            writeout(fname,1);
            curbp->flag &= ~BFCHG;
        }
        curbp = curbp->bufp;
    }
    vttidy();
    exit(GOOD);
}
void sig_winch(i)
int i;
{
   resize_window = ++i;
}

main(argc, argv)
char    *argv[];
{
    int    c;
    int    f;
    int    n;
    int    mflag;
    int    i;
    BYTE   bname[NBUFN];
    BYTE   fname[256];

//    logit("me starting...\n");
    
    for( n=0;n++;n<NKBDM ) kbdm[n] = 0;

    strcpy((char *)bname, "main");  // name of the default buffer.

    while( (c = getopt(argc,argv,"+hD:m:o:n:f:")) > 0 ){
        switch (c) { 
        case 'h':
            usage();
            break;
//        case 'm':   //specify meta data directory
//            strncpy((char *)metadir,optarg,255);
//            break;
        case 'D':
            dbug = atoi(optarg);
            do_log = 1;
            break;  
//        case 'o':  
//            strncpy(outfn,optarg,255);
//            break;
        default:
            usage(); 
        }
    }

    i=optind;
    n=0;
    while( argv[i] && n < (NFILES-1) ) {
        fnames[n] = (BYTE *)malloc( strlen(argv[i])+1 );
        strcpy((char *)fnames[n],argv[i]);
        i++;n++;
    }
    fnames[n] = NULL;
            
    (void)vtinit();
    (void)edinit(bname);
    (void)helpinit();

    signal(SIGHUP,sig_handler);
    signal(SIGWINCH,sig_winch);

    auto_backup = 0;
    if( geteuid() == 0 ) auto_backup = 1;

    if( !strcmp(argv[0],"v") ) vi_mode = 1;

    update();               // clean up the screen
    if (argc > 1) {
        pushkey(META|CNTRL|'N'); // just get the "next" (first) file
//        strncpy((char *)fname,argv[1],255);
//        fname[255] = '\0';
//        n = strlen((char *)fname);
//        fileread_1(0,1,fname);
    }
    update();               // clean up the screen
    lastflag = 0;
    mpresf = 1;
    t47=0;
loop:
    if( resize_window ) {   // set by signal handler
        resize();
        resize_window = 0;
    }
    if( t47 ) {
    }
    t47 = 1;
    c = getkey();
    if (mpresf != FALSE) {
//logit("gothere 1\n");
        mlerase();
        update();
    }
    f = FALSE;
    n = 1;
    if( vi_mode ){
        if( (c=='-') || ((c>='0')&&(c<='9')) ){
            n = 0;
            mflag = 1;
            if( c=='-' ){
                mflag = -1;
                c = getkey();
            }
            while( (c>='0') && (c<='9') ){
                n = n*10 + (c - '0');
                c = getkey();
            }
            if( n == 0 ) n = 1;
            n *=mflag;
        }
    }
        
    if( c == (CNTRL|'U') ){ // ^U, start argument
        f = TRUE;           // means we have an argument??
        n = 4;              // initialize with argument of 4
        mflag = 0;          // that can be discarded.
        mlwrite("Arg: 4");
        while ((c=getkey()) >='0' && c<='9' || c==(CNTRL|'U') || c=='-'){
            if (c == (CNTRL|'U'))
                n = n*4;
            /*
             * If dash, and start of argument string, set arg.
             * to -1.  Otherwise, insert it.
             */
            else if (c == '-') {
                if (mflag)
                    break;
                n = 0;
                mflag = -1;
            }
            /*
             * If first digit entered, replace previous argument
             * with digit and set sign.  Otherwise, append to arg.
             */
            else {
                if (!mflag) {
                    n = 0;
                    mflag = 1;
                }
                n = 10*n + c - '0';
            }
            mlwrite("Arg: %d", (mflag >=0) ? n : (n ? -n : -1));
        }
        /*
         * Make arguments preceded by a minus sign negative and change
         * the special argument "^U -" to an effective "^U -1".
         */
        if (mflag == -1) {
            if (n == 0) n++;
            n = -n;
        }
    }

    if (c == (CNTRL|'X')) {     //^X is a prefix
        c = CTLX | getctl();
    }
    if (kbdmip != NULL) {       // Save macro strokes.
        if( kbdmip>&kbdm[NKBDM-6] ){
            ctrlg(FALSE, 0);
            goto loop;
        }
        if (f != FALSE) {
            *kbdmip++ = (CNTRL|'U');
            *kbdmip++ = n;
        }
        *kbdmip++ = c;
    }
    execute(c, f, n);       // Do it
    update();               // clean up the screen
    goto loop;
}

    
/*
 * This is the general command execution
 * routine. It handles the fake binding of all the
 * keys to "self-insert". It also clears out the "thisflag"
 * word, and arranges to move it to the "lastflag", so that
 * the next command can look at it. Return the status of
 * command.
 */
execute(c, f, n)
{
    register KEYTAB *ktp;
    register int    status;
    int c1,c2,c3;

    // handle the arrow and function keys
    //  u: ^[[A   d: ^[[B   r: ^[[C   l: ^[[D
    //  pu: ^[[5~  pd: ^[[~
    //  f1: ^[OP     f2: ^[OQ     f3: ^[OR      f4: ^[OS
    //  f5: ^[[15~   f6: ^[[17~   f7: ^[[18~    f8: ^[[19~
    //  f9: ^[[20~   f10: ^[[21~  f11: ^[[23~   f12: ^[[24~

    if( c == (META|'O') ) { // F1 - F4
        c1 = getkey();
        if( c1 == 'P' ) { return key_f1(f,n); } // F1
        if( c1 == 'Q' ) { return key_f2(f,n); } // F2
        if( c1 == 'R' ) { return key_f3(f,n); } // F3
        if( c1 == 'S' ) { return key_f4(f,n); } // F4
    }
    if( c == (META|'[') ) {
        c1 = getkey();
        if( c1 == 'A' ) { return backline(f,n); } // up
        if( c1 == 'B' ) { return forwline(f,n); } // down
        if( c1 == 'C' ) { return forwchar(f,n); } // right
        if( c1 == 'D' ) { return backchar(f,n); } // left
        if( c1 == '5' ) { c = getkey(); return backpage(f,n); } // pgup
        if( c1 == '6' ) { c = getkey(); return forwpage(f,n); } // pgdn
        if( c1 == '1' ) { // F5 - F8
            c2 = getkey();
            // need to throw away the '~'
            if( c2 == '5' ) { c1 = getkey(); return key_f5(f,n); }
            if( c2 == '7' ) { c1 = getkey(); return key_f6(f,n); }
            if( c2 == '8' ) { c1 = getkey(); return key_f7(f,n); }
            if( c2 == '9' ) { c1 = getkey(); return key_f8(f,n); }
        }
        if( c1 == '2' ) {  // F9 - F12
            c2 = getkey();
            if( c2 == '0' ) { c1 = getkey(); return key_f9(f,n); }
            if( c2 == '1' ) { c1 = getkey(); return key_f10(f,n); }
            if( c2 == '3' ) { c1 = getkey(); return key_f11(f,n); }
            if( c2 == '4' ) { c1 = getkey(); return key_f12(f,n); }
        }
    }

    if( vi_mode ) {
        ktp = &vi_keytab[0];
        while( ktp->k_code != 0 ) {
            if( ktp->k_code == c ) {
                thisflag = 0;
                status = (*ktp->k_fp)(f,n);
                lastflag = thisflag;
                return (status);
            }
            ++ktp;
        }
    }
    ktp = &keytab[0];   // Look in key table.
    while (ktp < &keytab[NKEYTAB]) {
        if (ktp->k_code == c) {
            thisflag = 0;
            status   = (*ktp->k_fp)(f, n);
            lastflag = thisflag;
            return (status);
        }
        ++ktp;
    }
    
    if( vi_mode ) return TRUE;  // don't do self-insert

// If a space was typed, fill column is defined, the argument is non-
// negative, and we are now past fill column, perform word wrap.
// This should be a display-only feature...
    if( c>=0x20 ) {  // Self inserting character
        if (n <= 0) {
            lastflag = 0;
            return (n<0 ? FALSE : TRUE);
        }
        thisflag = 0;           // For the future.
        status   = linsert(n, c);
        if(  (c == ' ') && 
             (rmarg > 0 ) && 
             (getvcol(FALSE) > rmarg) &&
             (lgetc(curwp->dotp, curwp->doto-2) != ' ')
             ) 
        {
            wrapword();
        }
        lastflag = thisflag;
        return (status);
    }
    lastflag = 0;               // Fake last flags.
    return (FALSE);
}

/*
 * Read in a key.
 * Do the standard keyboard preprocessing.
 * Convert the keys to the internal character set. 
 */
getkey()
{
    register int c;
    int i;

    // get a character; do ad hoc 1 character lookahead
    if( lookahead ) { c = lookahead; lookahead = 0; } 
    else { c = (*term.getchar)(); }

    // convert to internal form

    // M- prefix.  note recursive call -- collapses esc presses
    if (c == METACH) { c = getkey(); return (META | c); }

    // C- prefix
    if (c == CTRLCH) { c = getctl(); return (CNTRL | c); }

    // C-M- prefix
    if (c == CTMECH) { c = getctl(); return (CNTRL | META | c); }

    // C0 control -> C-
    if ( c>=0x00 && c<=0x1F ) { 
		c = CNTRL | (c+'@');
		if (c == (CNTRL|'X')) {     //^X is a prefix
	        c = CTLX | getkey();	// note recursion
		}
		return (c);
    }
//	if( c == EXITCH ) {  "EXITCH" conflicts w/ some internal name?
    if( c == 0x1d ) {
		mlwrite("EXITCH");
		return c;
	}
	if( c == CTRLCH ) {
		mlwrite("CTRLCH");
		return c;
	}
	if( c == HELPCH ) {
		mlwrite("HELPCH");
		return c;
	}
	return c;
}

pushkey(int c) { lookahead = c; }


//
// Get a key and apply control modifications to it
//
getctl()
{
    register int    c;

    c = (*term.getchar)();
    // Force to uppercase
    if (c>='a' && c<='z') c -= 0x20;
    // C0 control -> C-
    if (c>=0x00 && c<=0x1F) c = CNTRL | (c+'@');
    return (c);
}

set_vi(f,n)
{
    (void)defaultargs(f,n);
    vi_mode = TRUE;
    return TRUE;
}
clr_vi(f,n)
{
    (void)defaultargs(f,n);
    vi_mode = FALSE;
    return TRUE;
}

vi_A(f,n)
{
    clr_vi(f,n);
    return gotoeol(f,n);
}

vi_s(f,n)
{
    forwdel(f,n);
    return clr_vi(f,n);
}

set_mode(m)
{
    if( m == 'C' ){
        mlwrite("Setting C mode");
        (void)tab(TRUE,C_tabs);
//        sleep(1);
    }
}

/*
 * Quit command. If an argument, always
 * quit. Otherwise confirm if a buffer has been
 * changed and not written out. Normally bound
 * to "C-X C-C".
 */
quit(f, n)
{
    register int s;

    (void)defaultargs(f,n);
    if (f != FALSE              // Argument forces it.
    || anycb() == FALSE         // All buffers clean.
    || (s=mlyesno("Quit")) == TRUE) {   // User says it's OK.
        vttidy();
        save_kbdm(kbm_file);
        exit(GOOD);
    }
    return (s);
}

/*
 * Begin a keyboard macro.
 * Error if not at the top level
 * in keyboard processing. Set up
 * variables and return.
 */
ctlxlp(f, n)
{
    (void)defaultargs(f,n);
    if (kbdmip!=NULL || kbdmop!=NULL) {
        mlwrite("Not now");
        return (FALSE);
    }
    mlwrite("[Start macro]");
    kbdmip = &kbdm[0];
    return (TRUE);
}

/*
 * End keyboard macro. Check for
 * the same limit conditions as the
 * above routine. Set up the variables
 * and return to the caller.
 */
ctlxrp(f, n)
{
    (void)defaultargs(f,n);
    if (kbdmip == NULL) {
        mlwrite("Not now");
        return (FALSE);
    }
    mlwrite("[End macro]");
    if( kbdmip > &kbdm[0] ) kbdmip--;
    *kbdmip = '\0';
    kbdmip = NULL;
    return (TRUE);
}
/*
 * save the keyboard macro, if any
 */
save_kbdm(fname) 
BYTE *fname;
{
    int fd,l;
    l = strlen((char *)kbdm);
    if( l > 0 ) {
        fd = open((char *)fname,O_RDWR|O_CREAT,0600);
        if( fd < 0 ) {
            mlwrite("warning: couldn't save kbm");
            perror("");
//            sleep(2);
            return TRUE;
        }
        write(fd,kbdm,sizeof(kbdm));
    }
    close(fd);
}

/*
 * restore the keyboard macro, if any
 */
rest_kbdm(fname) 
BYTE *fname;
{
    int fd,l;
    BYTE buf[255];
    fd = open((char *)fname,O_RDONLY);
    if( fd < 0 ) {
//        sprintf(buf,"problem restoring kbdm %s",fname);
//        perror(buf);
//        sleep(1);
        return TRUE;
    }
    read(fd,kbdm,sizeof(kbdm));
    close(fd);
    return TRUE;
}

/*
 * Execute a macro.
 * The command argument is the
 * number of times to loop. Quit as
 * soon as a command gets an error.
 * Return TRUE if all ok, else
 * FALSE.
 */
ctlxedot(f, n)
{
    dot_macro_flag = 1 - dot_macro_flag;
    if( dot_macro_flag ) {
        ctlxe(f,n);
        text_chg_flag = 0;
    }
}
int dot(f,n)
{
    // if defining a macro, just insert a .
    if( kbdmip != NULL || running_macro_flag || !dot_macro_flag) {
        linsert(1,'.');
        text_chg_flag = 0;  //since we are defining a macro, by
        return;             //definition we have not changed any text
    }

    // have there been any text changes outside the . macro?
    // if so, treat it as a normal insert, and turn off macro
    if( text_chg_flag ) {
        linsert(1,'.');
        dot_macro_flag = 0;
        text_chg_flag = 0;
    }

    if( dot_macro_flag ) {
        ctlxe(f,n);
        text_chg_flag = 0; // cleared every time . macro runs
                                // set by every other change.
    }
}
ctlxe(f, n)
{
    register int    c;
    register int    af;
    register int    an;
    register int    s;

    (void)defaultargs(f,n);
    if (kbdmip!=NULL || kbdmop!=NULL) {
        mlwrite("Not now");
        return (FALSE);
    }
    if (n <= 0) {
        mlwrite("ctlxe: here"); sleep(1);
        return (TRUE);
    }
    running_macro_flag = 1;
    do {
        kbdmop = &kbdm[0];
        do {
            af = FALSE;
            an = 1;
            if ((c = *kbdmop++) == (CNTRL|'U')) {
                af = TRUE;
                an = *kbdmop++;
                c  = *kbdmop++;
            }
            s = TRUE;
        } while( c && (s=execute(c, af, an))==TRUE );
        kbdmop = NULL;
    } while (s==TRUE && --n);
    running_macro_flag = 0;
    return (s);
}

/*
 * Abort.
 * Beep the beeper.
 * Kill off any keyboard macro,
 * etc., that is in progress.
 * Sometimes called as a routine,
 * to do general aborting of
 * stuff.
 */
ctrlg(f, n)
{
    (void)defaultargs(f,n);
    (*term.beep)();
    if (kbdmip != NULL) {
        kbdm[0] = (CTLX|')');
        kbdmip  = NULL;
    }
    return (ABORT);
}

/*

make sure that the f and n args seem to be used.  this is only to keep cc
from spitting out useless warnings for hundreds of function references... 
 
*/
int defaultargs(f,n)
int f,n;
{
    if( f || (n!=1) )   return 1;
    else return 0;
}
int usage()
{
    printf("me [options] file1 file2 ...\n");
    exit(0);
}

/*

Initialize all of the buffers and windows.  The buffer name is passed down as
an argument, because 'main' may have been told to read in a file by default,
and we want the buffer name to be right. 

*/
edinit(bname)
BYTE    bname[];
{
    register BUFFER *bp;
    register WINDOW *wp;
    int i;
    struct passwd *pw;

    strcpy((char *)version,VERSION_NAME);

    rmarg = 77;     // an initial guess...
    tabsize = 4;    // default size of tab
    softtabs = 1;   // use soft tabs
    C_tabs = 4;     // in case we want them different, later
    text_chg_flag =  0;  // any text changes outside current macro?
    dot_macro_flag =  0;  // in a dot macro?
    
    bp = bfind(bname, TRUE, 0);     // First buffer
    blistp = bfind("[List]", TRUE, BFTEMP);     // Buffer list buffer
    wp = (WINDOW *) malloc(sizeof(WINDOW));     // First window
    if (bp==NULL || wp==NULL || blistp==NULL) 
        abort();
    curbp  = bp;            // Make this current
    wheadp = wp;
    curwp  = wp;
    wp->wndp  = NULL;       // Initialize window
    wp->bufp  = bp;
    bp->nwnd  = 1;          // Displayed.
    wp->topp = bp->lines;
    wp->topo = 0;
    wp->dotp  = bp->lines;
    wp->doto  = 0;
    wp->markp = NULL;
    wp->marko = 0;
    wp->toprow = 0;
    wp->ntrows = term.nrow-1;     // "-1" for mode line
    wp->force = 0;
    wp->flag  = WFMODE;

    pw = getpwuid(getuid());
    if( pw == NULL ) {
        fprintf(stderr,"no uid.\n");
        exit(1);
    }
    snprintf((char *)rc_dir,sizeof(rc_dir)-1,"%s/.me",pw->pw_dir);
    snprintf((char *)kbm_file,sizeof(kbm_file)-1,"%s/kbm_file",rc_dir);
    if( access((char *)rc_dir,R_OK|W_OK|X_OK) ) {
        // try to create the directory and the kbm file
        if( mkdir((char *)rc_dir,0700) ) {
            printf("Note: couldn't create state directory %s\r\n",
                (char *)rc_dir);
            sleep(2);
        }
        else {
            printf("Note: initializing state directory %s\r\n",(char *)rc_dir);
            sleep(2);
        }
    }
    kbdmip = NULL;
    for( i=0;i<NKBDM;i++ ) kbdm[i] = 0;
    rest_kbdm(kbm_file);


    escape_pressed = 0;

    return TRUE;
}

/*
 * initialize the help buffer
 */
helpinit()
{
	KEYTAB *hp;
    helpbp = bfind("[Help]",TRUE,BFTEMP);       // help buffer
    addline(helpbp,"[^Xn for next window; ^Xp for previous window]");
    hp = &keytab[0];   // Look in key table.
    while (hp->k_code) {
        addline(helpbp,hp->help);
        ++hp;
    }
}

/*
 * print the help text in the mode line.
 */
help(f, n)
{
	int	s;
	int	c;
	BYTE *str;
	KEYTAB *ktp;
	
	(void)defaultargs(f,n);

	c = getkey();
    if( vi_mode ) {
        ktp = &vi_keytab[0];
        while( ktp->k_code != 0 ) {
            if( ktp->k_code == c ) {
                mlwrite(ktp->help);
                return (TRUE);
            }
            ++ktp;
        }
    }
    ktp = &keytab[0];   // Look in key table.
    while (ktp < &keytab[NKEYTAB]) {
        if (ktp->k_code == c) {
            mlwrite(ktp->help);
            return (TRUE);
        }
        ++ktp;
    }
     
    if( vi_mode ) {
		mlwrite("no action");
		return TRUE;  // don't do self-insert
	}
	mlwrite("self insert");
	return TRUE;
}

logit( BYTE *s )
{
    int lfd;
    int i;
    if( do_log ) {
        lfd = open("/tmp/log.me", O_CREAT|O_APPEND|O_WRONLY,0644);
        if( lfd < 0 ) die("open of log file failed\n");
        if( (i = write(lfd,(char *)s,strlen((char *)s)) ) < 0 ) 
            die("write failed\n");
        close(lfd);
    }
}
logchr( int c )
{
    int lfd;
    BYTE buf[8];
    lfd = open("/tmp/log.me", O_CREAT|O_APPEND|O_WRONLY,0644);
    if( lfd < 0 ) die("open of log file failed\n");
    buf[0] = c & 0xff;
    if( write(lfd,buf,1) < 0 ) die("logchr: write failed\n");
    close(lfd);
}
logint( BYTE *s, int i )
{
    int lfd;
    BYTE buf[256];
    lfd = open("/tmp/log.me", O_CREAT|O_APPEND|O_WRONLY,0644);
    if( lfd < 0 ) die("open of log file failed\n");
    sprintf((char *)buf,(char *)s,i);
    if( write(lfd,(char *)buf,strlen((char *)buf)) < 0 ) 
        die("logint: write failed\n");
    close(lfd);
}
die( BYTE *s )
{
    fprintf(stderr,"%s",(char *)s);
    perror(0);
    exit(1);
}
bkp()
{
    return;
}
