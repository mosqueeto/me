//#define CRYPT_S 0       // support encryption commands
#define CRYPT_S 1       // support encryption commands
#define CVMVAS  1       // C-V, M-V arg. in screens.
#define NFILEN  80      // # of bytes, file name
#define NBUFN   16      // # of bytes, buffer name
#define MAXPW	64		// # of bytes in a password
#define NFILES  256     // # of files on command line
#define NLINE   256     // # of bytes, line
#define NKBDM   256     // # of strokes, keyboard macro
#define MAXPAT  80      // # of bytes, pattern
#define HUGE    1000    // Huge number
#define MAXVARS 25      // size of variable table

#define METACH  0x1B    // M- prefix,   Control-[, ESC
#define CTMECH  0x1C    // C-M- prefix, Control-\
#define EXITCH  0x1D    // Exit level,  Control-]
#define CTRLCH  0x1E    // C- prefix,   Control-^
#define HELPCH  0x1F    // Help key,    Control-_

#define CNTRL   0x0100  // Control flag, or'ed in
#define META    0x0200  // Meta flag, or'ed in
#define CTLX    0x0400  // ^X flag, or'ed in

#define FALSE   0   // False, no, bad, etc.
#define TRUE    1   // True, yes, good, etc.
#define ABORT   2   // Death, ^G, abort, etc.

#define FIOSUC  0   // File I/O, success.
#define FIOFNF  -1  // File I/O, file not found.
#define FIOEOF  -2  // File I/O, end of file.
#define FIOERR  -3  // File I/O, error.
#define FIONEW  -4  // File not found, created a new one

#define CFCPCN  0x0001  // Last command was C-P, C-N
#define CFKILL  0x0002  // Last command was a kill

#define INTVAL  0x0001  // variable is an int
#define STRVAL  0x0002  // variable is a string
#define BUILTIN 0x0004  // variable is builtin
#define RDONLY  0x0008  // variable cannot be set

#define MDMAGIC 0x0001  // use magic patterns in search
#define MDEXACT 0x0002  // match exact case

#define TAB 0x09    // tab character

#define ME_MAGIC "#ME1.42$" // magic prefix for me files
#define ME_SECRET "5fa49404e62a2f71ccee2640ff26b851"

#define BYTE unsigned char

/*
 * There is a window structure allocated for
 * every active display window. The windows are kept in a
 * big list, in top to bottom screen order, with the listhead at
 * "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands
 * to guide redisplay; although this is a bit of a compromise in
 * terms of decoupling, the full blown redisplay is just too
 * expensive to run for every input character. 
 */
typedef struct  WINDOW {
    struct  WINDOW  *wndp;  // Next window
    struct  BUFFER  *bufp;  // Buffer displayed in window
    struct  LINE    *topp;  // Top screen line in the window
    int             topo;   // offset to the beginning of the top screen line
    struct  LINE    *dotp;  // Line containing "."
    struct  LINE    *markp; // Line containing "mark"
    int     doto;   // Byte offset for "."
    int     marko;  // Byte offset for "mark"
    int     toprow; // Origin 0 top row of window
    int     ntrows; // # of rows of text in window
    int     force;  // If NZ, forcing row.
    int     flag;   // Flags.
}   WINDOW;

#define WFFORCE 0x01    // Window needs forced reframe
#define WFMOVE  0x02    // Movement from line to line
#define WFEDIT  0x04    // Editing within a line
#define WFHARD  0x08    // Better to a full display
#define WFMODE  0x10    // Update mode line.

/*

Text is kept in buffers.  A buffer header, described below, exists for every
buffer in the system.  The buffers are kept in a big list, so that commands
that search for a buffer by name can find the buffer header.  There is a safe
store for the dot and mark in the header, but this is only valid if the
buffer is not being displayed (that is, if "nwnd" is 0).  The text for the
buffer is kept in a circularly linked list of lines, with a pointer to the
header line in "lines". 

*/
typedef struct  BUFFER {
    struct  BUFFER *bufp;   // Link to next BUFFER
    struct  LINE *dotp;     // Link to "." LINE structure
    int doto;               // Offset of "." in above LINE
    struct  LINE *markp;    // The same as the above two,
    int marko;              // but for the "mark"
    struct  LINE *lines;    // Link to the header LINE
    int nwnd;               // Count of windows on buffer
    int flag;               // Flags
    int mode;               // Mode for this buffer
    BYTE    fname[NFILEN];  // File name
    BYTE    bname[NBUFN];   // Buffer name
	BYTE	passwd[MAXPW];  // Password, if encrypted
    struct  stat *stat;     // status of file at readin time
}   BUFFER;

#define BFTEMP  0x01    // Internal temporary buffer
#define BFCHG   0x02    // Changed since last write

/*

The starting position of a region, and the size of the region in characters,
is kept in a region structure.  Used by the region commands. 

*/
typedef struct  {
    struct  LINE *lines;    // Origin LINE address.
    int offset;             // Origin LINE offset.
    int size;               // Length in characters.
}   REGION;

/*

All text is kept in circularly linked lists of "LINE" structures.  These
begin at the header line (which is the blank line beyond the end of the
buffer).  This line is pointed to by the "BUFFER".  Each line contains a the
number of bytes in the line (the "used" size), the size of the text array,
and the text.  The end of line is not stored as a byte; it's in the flags. 
Future additions will include update hints, and a list of marks into the
line. 

*/
typedef struct  LINE {
    struct  LINE *fp;   // Link to the next line
    struct  LINE *bp;   // Link to the previous line
    int flags;          // line flags
    int size;           // Allocated size
    int used;           // Used size
    BYTE *text;         // A bunch of characters.
}   LINE;


#define L_NL 0x1
    // line has a newline at the end
#define L_EXT 0x2
    // line data is in another buffer, instead of a chunk allocated
    // for this line.  The allocated size is 0, in this case
#define L_HEAD 0x4
    // line is the dummy header line in a buffer

//#define lforw(lp)       (((lp)->fp->flags && L_HEAD)?(lp):(lp)->fp)
#define lforw(lp)       ((lp)->fp)
//#define lback(lp)       (((lp)->bp->flags && L_HEAD)?(lp):(lp)->bp)
#define lback(lp)       ((lp)->bp)
#define lgetc(lp, n)    ((lp)->text[(n)]&0xFF)
#define lputc(lp, n, c) ((lp)->text[(n)]=(c))
#define llength(lp)     ((lp)->used)

/*
 * The editor communicates with the display
 * using a high level interface. A "TERM" structure
 * holds useful variables, and indirect pointers to
 * routines that do useful operations. The low level get
 * and put routines are here too. This lets a terminal,
 * in addition to having non standard commands, have
 * funny get and put character code too. The calls
 * might get changed to "termp->t_field" style in
 * the future, to make it possible to run more than
 * one terminal type.
 */  
typedef struct  {
    int nrow;         // Number of rows.
    int ncol;         // Number of columns.
    int (*open)();    // Open terminal at the start.
    int (*close)();   // Close terminal at end.
    int (*getchar)(); // Get character from keyboard.
    int (*putchar)(int); // Put character to display.
    int (*flush)();   // Flush output buffers.
    int (*move)(int, int); // Move the cursor, origin 0.
    int (*eeol)();    // Erase to end of line.
    int (*eeop)();    // Erase to end of page.
    int (*beep)();    // Beep.
    int (*hilight)(int); // Hilight character.
}   TERM;

typedef struct {
    BYTE    *name;      // variable name
    int flag;           // its type
    void    *val;       // pointer to its value
} EVAR;

typedef struct {
    int   k_code;           // Key code
    int (*k_fp)(int, int);  // Routine to handle it
    char *name;             // Built-in name string
    char *help;             // Help text
} KEYTAB;

#define NUSER_KEYS 64

typedef struct {
    int   k_code;           // Key code
    int (*k_fp)(int, int);  // Non-NULL: rebind to built-in
    char *k_cmd;            // Non-NULL: pipe shell command
} USER_KEY;

extern KEYTAB    keytab[];
extern USER_KEY  user_keys[];
extern int       n_user_keys;

extern int  pipe_interactive(int, int);
extern int  user_execute(int, int, int);
extern void read_init_file(BYTE *);
extern void init_rc_dir(BYTE *);

#ifdef  ED_MAIN
int dbug;
int auto_backup;        // automatically create backup
int currow;             // Working cursor row
int curcol;             // Working cursor column
int rmarg;              // Current right fill column
int lmarg;              // Current left fill column
int C_tabs;             // tab setting for "C" mode
int thisflag;           // Flags, this command
int lastflag;           // Flags, last command
int escape_pressed;     // input terminated w escape
int curgoal;            // Goal column
int tabsize;            // spaces to display for a tab
int softtabs;           // use spaces  for tabs
BUFFER  *curbp;         // Current buffer
WINDOW  *curwp;         // Current window
BUFFER  *bheadp;        // BUFFER listhead
WINDOW  *wheadp;        // WINDOW listhead
BUFFER  *blistp;        // Buffer list BUFFER
BUFFER  *helpbp;         // help buffer pointer
int kbdm[NKBDM];        // Macro
int *kbdmip;            // Input  for above
int *kbdmop;            // Output for above
BYTE pat[MAXPAT];       // Pattern
BYTE version[16];       // version string
BYTE *fnames[NFILES];   // array of input filenames
int dot_macro_flag;     // in dot macro
int text_chg_flag;      // record text changes outside macros
int vi_mode;            // are we in vi mode?
#else
extern  int dbug;
extern  int auto_backup;    // automatically create backup
extern  int rmarg;          // Right margin
extern  int lmarg;          // Left margin
extern  int C_tabs;         // "C" mode tab setting
extern  int currow;         // Cursor row
extern  int curcol;         // Cursor column
extern  int thisflag;       // Flags, this command
extern  int lastflag;       // Flags, last command
extern  int escape_pressed; // input terminated w escape
extern  int curgoal;        // Goal for C-P, C-N
extern  int tabsize;        // spacing for display of tabs
extern  int softtabs;       // use soft tabs (spaces) for tabs

extern  int sgarbf;         // State of screen unknown
extern  WINDOW  *curwp;     // Current window
extern  BUFFER  *curbp;     // Current buffer
extern  WINDOW  *wheadp;    // Head of list of windows
extern  BUFFER  *bheadp;    // Head of list of buffers
extern  BUFFER  *blistp;    // Buffer for C-X C-B
extern  BUFFER  *helpbp;    // Buffer for M-h
extern  int kbdm[];         // Holds keyboard macro data
extern  int *kbdmip;        // Input pointer for above
extern  int *kbdmop;        // Output pointer for above
extern  BYTE pat[];         // Search pattern
extern  BYTE rpat[];        // Search pattern
extern  BYTE version[];     // version string
extern  BYTE *fnames[];     // input filenames
extern  int dot_macro_flag; // in dot macro
extern  int text_chg_flag;  // record text changes outside macros
extern  int vi_mode;       // are we in vi mode?
#endif

extern  int mpresf;         // Stuff in message line
extern  TERM    term;       // Terminal information.
extern  BUFFER  *bfind(BYTE *, int, int);   // Lookup a buffer by name
extern  WINDOW  *wpopup();  // Pop up window creation
extern  LINE    *lalloc(int);  // Allocate a line
extern  LINE    *lballoc(); // Allocate a line block
extern  LINE    *ladd(LINE *, int);    // Add a line before the arg
extern  BYTE    *ffslurpfile(); // read an entire file
extern  BYTE    *getpw(int);	// read a password

