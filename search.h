/*
 * SEARCH.H
 *
 * Defines, typdefs, and global variables that are of use for the
 * routines in search.c
 *
 */

/*
 * PTBEG, PTEND, FORWARD, and REVERSE are all toggle-able values for
 * the scan routines.
 */
#define PTBEG   0       /* Leave the point at the beginning on search.*/
#define PTEND   1       /* Leave the point at the end on search.*/
#define FORWARD	0	/* search forward */
#define REVERSE	1	/* search backward */

/*
 * Defines for the metacharacters in the regular expressions.
 */

#define MCNIL           0       /* Like the '\0' for strings.*/
#define LITCHAR         1
#define ANY             2
#define CCL             3
#define NCCL            4
#define BOL             5
#define EOL             6
#define CLOSURE         256     /* An or-able value.*/
#define MASKCL          CLOSURE - 1

#define MC_ANY          '.'     /* 'Any' character (except newline).*/
#define MC_CCL          '['     /* Character class.*/
#define MC_NCCL         '^'     /* Negate character class.*/
#define MC_RCCL         '-'     /* Range in character class.*/
#define MC_ECCL         ']'     /* End of character class.*/
#define MC_BOL          '^'     /* Beginning of line.*/
#define MC_EOL          '$'     /* End of line.*/
#define MC_CLOSURE      '*'     /* Closure - does not extend past newline.*/

#define MC_ESC          '\\'    /* Escape - suppress meta-meaning.*/

#define BIT(n)          (1 << (n))      /* An integer with one bit set.*/
#define CHCASE(c)       ((c) ^ DIFCASE) /* Toggle the case of a letter.*/

/* HICHAR - 1 is the largest character we will deal with.
 * HIBYTE represents the number of bytes in the bitmap.
 */

#define HICHAR          256
#define HIBYTE          HICHAR >> 3

#define isletter(c) ( ('A' <= (c) && (c) <= 'Z') || \
		      ('a' <= (c) && (c) <= 'z') )

typedef char    *BITMAP;

typedef struct {
    short int       mc_type;
    union {
        int     lchar;
        BITMAP  cclmap;
    } u;
} MC;
