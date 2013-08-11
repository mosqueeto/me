/*
This file contains the code for the extension language.  

Objects look like this on creation:

    ints:
        +----------------+
        |   OB_INT       |  flags
        +----------------+
        |    links       |
        +----------------+
        | u.z = value    |
        +----------------+

    reals:
        +----------------+
        |    OB_REAL     |  flags
        +----------------+
        |    links       |
        +----------------+
        | u.r = value    |
        +----------------+

    strings:
        +----------------+
        |   OB_STRING    |  flags
        +----------------+
        |    links       |
        +----------------+
        | u.v -> value   |
        +----------------+

    threads:
        +----------------+
        | OB_THREAD      |  flags
        +----------------+
        |    links       |
        +----------------+    +---------+
        | u.t -----------|--->| n       |
        +----------------+    +---------+
                              | [0]-----|--> ob0
                              +---------+
                              | [1]-----|--> ob1
                              +---------+
                              | ....... |
                              +---------+
                              | [n-1]---|--> obn-1
                              +---------+

    code:
        +----------------+
        | OB_CODE        |  flags
        +----------------+
        |    links       |
        +----------------+
        | u.f -> function|
        +----------------+

                                   
                                   
    name:                          namelist
        +----------------+         +-------+    +-------+
        | OB_NAME        | flags   | name  |    |  name |
        +----------------+         +-------+    +-------+
        |    links       |         | next--|--->| next--|--> ...
        +----------------+         +-------+    +-------+
        | u.n -----------|-------->| ob--+ |    | ob--+ |    
        +----------------+         +-----|-+    +-----|-+
                                         |            |
                                         V            V
                                   +-------+    +-------+
                                   |any    |    |any    |
                                   | object|    | object|
                                   +-------+    +-------+


The "namelist" is a symbol table.  

    namelist:
        +-------+              +-------+
        | name  |              |  name |
        +-------+              +-------+
        | next--|------------->| next--|--> ...
        +-------+              +-------+
        | ob--+ |              | ob--+ |    
        +-----|-+              +-----|-+
              |                      |
              V                      V
        +-------+              +-------+
        |any    |              |any    |
        | object|              | object|
        +-------+              +-------+




To do:
    1) different modes of input each need their own buffers, so
    input can resume
    2) get "break" and "exit" to work
    3) get recursive functions to work
    4) new_ole -- good error message
    6) make stack a linked list
    7) function to interpret a string on the stack    
    8) reset command

    memory management:

    Basic rule: implicitly created objects must be implicitly deleted 
    when they are no longer in use.

    objects are "bound" when they are placed on the namelist -- such
    named objects cannot be implicitly deleted. 

    numbers are always copied, so deleting a number is not a big deal.

    code objects can never be deleted. 

    threads are much more complex.  they are created implicitly, and
    deleted implicitly by the interpreter/compiler.  eg:

    { ... { ... { ... } 3 rpt } 2 rpt } 1 rpt
                      ^       ^       ^
                      3       2       1

    Thread 1 is pushed on the stack for the outer "rpt", and should 
    be deleted when that rpt finishes.  But thread 3 should not be 
    deleted when its following rpt is finished, because the outer rpt 
    is going to execute it again.

    If a thread creates objects, should they
    persist after the thread is done, or be deleted with
    
    bound integers and reals are copied when pushed on the 
    stack.  bound strings are marked "rdonly", and are copied
    on modification.

 */
#include    <stdio.h>
#include    <strings.h>
#include    <unistd.h>
#include    <signal.h>
#include    <limits.h>
#include    "ed.h"

#define MAXLINEBUF      128
#define MAXSTACK        256
#define MAXWORD          32
#define PR_BUFZ         132
#define MAX_PR_INDENT    64

#define OB_INT         1
#define is_int(ob) (ob->flags & OB_INT)

#define OB_REAL        2
#define is_real(ob) (ob->flags & OB_REAL)

#define OB_STRING      4
#define is_string(ob) (ob->flags & OB_STRING)

#define OB_THREAD      8
#define is_thread(ob) (ob->flags & OB_THREAD)

#define OB_CODE       16
#define is_code(ob) (ob->flags & OB_CODE)

#define OB_NAME       32
#define is_name(ob) (ob->flags & OB_NAME)

#define OB_MARK       64
#define is_mark(ob) (ob->flags & OB_MARK)

#define OB_BOUND    128
#define is_bound(ob) (ob->flags & OB_BOUND)

#define OB_IMMED    256
#define is_immed(ob) (ob->flags & OB_IMMED)


#define START_STRING  '<'
#define END_STRING    '>'
#define START_VAR     '@'
#define START_THREAD  "{"
#define END_THREAD    "}"

#define STD_INPUT    1
#define STRING_INPUT 2
#define FILE_INPUT   3

#define OP_ADD       1
#define OP_SUB       2
#define OP_MUL       3
#define OP_DIV       4
#define OP_MOD       5
#define OP_AND       6
#define OP_OR        7
#define OP_EQ        8
#define OP_NE        9
#define OP_LT        10
#define OP_GT        11
#define OP_LE        12
#define OP_GE        13
#define OP_CMP       14
#define OP_CAT       15

#define MIN(a,b)    ((a)<(b)?a:b)

#define chk_stk(n,d) if(top<d ){\
            sprintf(msg,"%s: end of stack",n);\
            mlwrite(msg);\
            interrupted=TRUE;\
            return;\
        }
#define chk_type(ob,type_mask) ( ((ob).flags & (type_mask)) != 0 )

#define STANDALONE 

#ifdef STANDALONE
#define mlwrite(c) (void)printf("%s\n",c)
#endif

char *op_name[] = {"noop","+","-","*","/","%","and","or","eq","ne",
                   "lt","gt","le","ge","strcmp",".","xxxx"};


char msg[132];

/* the fundamental object is an OBJECT -- nothing to do with OOP */
typedef struct _object {
    int flags;
    int links;
    union {
        long z;
        double r;
        char *s;
        struct _var *v;
        struct _thread *t;
        void (*f)();
    } u;
} OBJECT;


/* the parameter stack is a stack of OBJECTS */
OBJECT *stack[MAXSTACK];
int top = 0;


/* variables are named OBJECTS, kept in lists */
typedef struct _var {
    char *name;
    struct _var *next;
    struct _object *ob;
} VAR;

/* "namelist" is a list of named OBJECTS that are global in scope */

VAR *namelist = (VAR *)NULL;

/* THREADs are basically arrays of objects -- can be executable */

typedef struct _thread {
    int    len;
    struct _object    **obs;
} THREAD;

/* an object for the "undefined" function */
OBJECT undefined_ob;
OBJECT *undefined_obs[] = { &undefined_ob };

/* a static thread that prints and "undefined" message */
THREAD undefined_th;
THREAD *current_thread = (THREAD *)NULL;

int interrupted;
int exit_thread = 0;
int break_loop = 0;
int run_next_flag = FALSE;
int compile_depth = 0;
int run_level = 0;

void clear_to_mark();
void compile();
void drop();
void exec();
void free_ob();
char *lookup_ob();
void pr_ob();
void pr_thread();
void pr_array();
void pr_var();
void print_ob();
void rm_name();

/* variables associated with input and output */
int current_input = STD_INPUT;
int cix = 0;
int cichrs = 0;
char *cibuf;
FILE *mel_in;
char linebuf[MAXLINEBUF];
int lbx = 0;
int lbchrs = 0;
int hex_mode = 0;
int oct_mode = 0;

char word[MAXWORD];    /* the global word */
int wx;                /* global index into the global word */

long zint;             /* the global integer */
double zreal;          /* the global real */

int trace_mem = 0;
int n_blks = 0;
int n_obs = 0;
int n_strings = 0;
int n_threads = 0;
int n_names = 0;
int n_ints = 0;
int n_reals = 0;
int n_arrays = 0;
int verbose_dmp = 0;

void mem_trace() { trace_mem = 1 - trace_mem; }
void toggle_verbose() { verbose_dmp = 1 - verbose_dmp; }
void quit() { exit(0); }
void test() { printf("Well, we are executing a test now\n"); }
void undefined() { mlwrite("Executing undefined object."); }

void *MALLOC(who,size)
char *who;
size_t size;
{
    char *p;

    p = (char *)malloc(size);
    if( p ){
        if( trace_mem ){
            sprintf(msg,"MALLOC:%s; %p, %d",who,p,size);
            mlwrite(msg);
        }
        n_blks++;
    } else {
        sprintf(msg,"out of memory: %s",who);
        mlwrite(msg);
    }
    return (void *)p;
}

void *REALLOC(who,p,size)
char *who;
void *p;
size_t size;
{
    char *p1;

    p1 = (char *)realloc(p,size);
    if( !p1 ){
        sprintf(msg,"out of memory: %s",who);
        mlwrite(msg);
    }
    if( trace_mem ){
        sprintf(msg,"REALLOC:%s; %p->%p, %d",who,p,p1,size);
        mlwrite(msg);
    }
    return (void *)p1;
}

void FREE(who,p)
char *who;
void *p;
{
    if( trace_mem ){
        sprintf(msg,"FREE:%s; %p",who,p);
        mlwrite(msg);
    }
    if( p != (void *)NULL ) free(p);
    n_blks--;
}

void mem()
{
    sprintf(msg,
  "blk: %d, ob: %d, t: %d, s: %d, z: %d, r: %d, n: %d",
       n_blks,n_obs,n_threads,n_strings,n_ints,n_reals,n_names);
    mlwrite(msg);
}

void handler(i)
int i;
{
    interrupted = TRUE;
    signal( SIGINT, handler );
}

THREAD *new_thread(num_obs)
int num_obs;
{
    THREAD *t;

    t = (THREAD *)MALLOC("new_thread:th", sizeof(THREAD));
    if( t == (THREAD *)NULL ) return t;

    t->len = num_obs;
    t->obs = 
        (OBJECT **)MALLOC("new_thread:obs",t->len*sizeof(OBJECT *) );
    if( t->obs == (OBJECT **)NULL ){
        FREE("new_thread",t);
        return (THREAD *)NULL;
    }
    n_threads++;
    return t;
}

THREAD *dup_thread(t)
THREAD *t;
{
    THREAD *t1;
    int i;

    t1 = new_thread(t->len);
    if( t1 != (THREAD *)NULL ){
        for( i=0; i<t1->len;i++ ) t1->obs[i] = t->obs[i];
    }
    return t1;
}


void free_thread(t,force)
THREAD *t;
int force;
{
    int i = 0;
    if( t == &undefined_th ) return;
    if( t->obs != (OBJECT **)NULL ){
        for( i=0;i<t->len;i++ ){
            free_ob( t->obs[i]);
        }
        FREE("free_thread:obs",t->obs);
    }
    n_threads--;
    FREE("free_thread:th", t );
}

void dump_thread(t)
THREAD *t;
{
    int i;
    if( t == (THREAD *)NULL ) return;
    printf("len = %d\n, obs =\n",t->len);
    for( i=0;i<t->len;i++ ) print_ob(t->obs[i]);
}

void _bind(ob)
OBJECT *ob;
{
    int i;
    if( ob == (OBJECT *)NULL ) return;
    ob->flags |= OB_BOUND;
}

char *dup_string(s)
char *s;
{
    char *s1;
    s1 = (char *)MALLOC("dup_string",strlen(s)+1);
    if( s1 != (char *)NULL ){
        strcpy(s1,s);
        n_strings++;
    }
    return s1;
} 

void string_input(s)
char *s;
{
    lbx = cix;
    cix = 0;
    cichrs = strlen(s);
    cibuf = s;
    current_input = STRING_INPUT;
}

void file_input(fname)
char *fname;
{
    mel_in = fopen(fname,"r");
    if( mel_in == (FILE *)NULL ){
        mlwrite("file_input: couldn't open");
        return;
    }
    current_input = FILE_INPUT;
    cix = cichrs = 0;
    cibuf = linebuf;
}

void flush_input() { cichrs = 0; }

char nextc()
{
    while( cichrs <= cix ){
        if( current_input == STD_INPUT ){
#ifdef STANDALONE
            printf(">");
            if( fgets(linebuf,MAXLINEBUF,stdin) == (char *)NULL ){
                fprintf(stderr,"EOF on input\n");
                exit(1);
            }
            cichrs = lbchrs = strlen(linebuf);
            if( linebuf[lbchrs-1] == '\n' ){
                linebuf[lbchrs] = ' ';
            }
            cix = 0;
            cibuf = linebuf;
#else
            /* editor input to be implemented */
#endif
        } else if( current_input == STRING_INPUT ){
            current_input = STD_INPUT;
            cix = lbx;
            cichrs = lbchrs;
        } else if( current_input == FILE_INPUT ){
            if( fgets(linebuf,MAXLINEBUF,mel_in) == (char *)NULL ){
                fclose(mel_in);
                current_input = STD_INPUT;
                cix = lbx;
                continue;
            }
            cichrs = lbchrs = strlen(linebuf);
            if( linebuf[lbchrs-1] == '\n' ){
                linebuf[lbchrs] = ' ';
            }
            cix = 0;
        } else {
            current_input = STD_INPUT;
            continue;
        }
            
    }
    return cibuf[cix++];
}

/*
 * The namelist is a self-organizing list.  New items always go
 * the front of the list.  Objects that are searched for and found
 * in the list are moved to the front of the list, so least-accessed
 * objects migrate to the end of the list.
 */

OBJECT *new_ob(who)
char *who;
{
    OBJECT *ob;
    if( who == (char *)NULL ) who = "new_ob";
    ob = (OBJECT *)MALLOC(who,sizeof(OBJECT));
    if( ob ) n_obs++;
    ob->flags = 0;
    ob->links = 0;
    return ob;
}

void free_ob(ob)
OBJECT *ob;
{
    if( ob == (OBJECT *)NULL ) return;
    if( is_bound(ob) ) return;

    if( is_int(ob) ){
        FREE("free_ob:INT",ob);
        n_ints--;
    } else if( is_string(ob) ){
        FREE("free_ob:STRING",ob->u.s);
        n_strings--;
        FREE("free_ob:STRING",ob);
    } else if( is_real(ob) ){
        n_reals--;
        FREE("free_ob:REAL",ob);
    } else if( is_code(ob) ){ 
        return;
    } else if( is_thread(ob) ){
        free_thread(ob->u.t);
        FREE("free_ob:THREAD",ob);
    } else if( is_name(ob) ){
        ob->u.v->ob->links--;
        if( ob->u.v->ob->links <= 0 ) FREE("free_ob:NAME",ob);
    } else if( is_mark(ob) ){
        FREE("free_ob:MARK",ob);
    } else {
        mlwrite("free_ob: unknown object type");
    }
    n_obs--;
    return;
}

void pr_flag(buf,ob)
char *buf;
OBJECT *ob;
{
    int i;

    sprintf(&buf[strlen(buf)],"[");

    if( is_int(ob) )        sprintf(&buf[strlen(buf)],"Z");
    if( is_real(ob) )       sprintf(&buf[strlen(buf)],"R");
    if( is_string(ob) )     sprintf(&buf[strlen(buf)],"S");
    if( is_thread(ob) )     sprintf(&buf[strlen(buf)],"T");
    if( is_code(ob) )       sprintf(&buf[strlen(buf)],"C");
    if( is_name(ob) )       sprintf(&buf[strlen(buf)],"N");
    if( is_mark(ob) )       sprintf(&buf[strlen(buf)],"M");
    if( is_bound(ob) )      sprintf(&buf[strlen(buf)],"B");
    if( is_immed(ob) )      sprintf(&buf[strlen(buf)],"I");

    sprintf(&buf[strlen(buf)],"]:");
}


void pr_ob(indent,ob)
int indent;
OBJECT *ob;
{
    char buf[PR_BUFZ];
    int i,j;
    char *name;

    indent = MIN(indent,MAX_PR_INDENT);
    for( i=0;i<indent;i++ ) buf[i] = ' ';
    buf[i] = '\0';
    buf[PR_BUFZ-1] = '\0';

    if( ob == (OBJECT *)NULL ){
        strcpy(&buf[i],"[NULL]");
        mlwrite(buf);
        return;
    }

    

    if( is_int(ob) ){
        if( verbose_dmp ) {
            pr_flag(buf,ob);
            name = lookup_ob(ob);
            if( name ) sprintf(&buf[strlen(buf)],"%s:",name);
        }
        if( hex_mode ) {
            (void)sprintf(&buf[strlen(buf)],"%lx",ob->u.z);
        } else if( oct_mode ) {
            (void)sprintf(&buf[strlen(buf)],"%lo",ob->u.z);
        } else {
            (void)sprintf(&buf[strlen(buf)],"%ld",ob->u.z);
        }
        mlwrite(buf);
        return;
    }

    if( is_string(ob) ){
        if( verbose_dmp ) {
            pr_flag(buf,ob);
            name = lookup_ob(ob);
            if( name ) sprintf(&buf[strlen(buf)],"%s:",name);
        }
        i = strlen(buf);
        j = 0;
        buf[i++] = '<';
        while( i<PR_BUFZ-10 && ob->u.s[j] ) buf[i++] = ob->u.s[j++];
        buf[i++] = '>';
        buf[i] = 0;
        mlwrite(buf);
    } else if( is_real(ob) ){
        if( verbose_dmp ) {
            pr_flag(buf,ob);
            name = lookup_ob(ob);
            if( name ) sprintf(&buf[strlen(buf)],"%s:",name);
        }
        sprintf(&buf[strlen(buf)],"%f",ob->u.r);
        mlwrite(buf);
    } else if( is_code(ob) ){
        if( verbose_dmp ) {
            pr_flag(buf,ob);
        }
        sprintf(&buf[strlen(buf)],"%s ",lookup_ob(ob));
        mlwrite(buf);
    } else if( is_thread(ob) ){
        if( ob->u.t ) {
            if( verbose_dmp ) pr_flag(buf,ob);
            name = lookup_ob(ob);
            if( name ) sprintf(&buf[strlen(buf)],"%s",name);
            else  sprintf(&buf[strlen(buf)],"%s","unnamed_thread");
            if( verbose_dmp )
                sprintf(&buf[strlen(buf)],":len=%d",ob->u.t->len);
        } else {
            sprintf(&buf[strlen(buf)],"[NULL THREAD]");
        }
        mlwrite(buf);
        if( ob->u.t && verbose_dmp ) {
            i = strlen(buf);
            for( i=0;i<ob->u.t->len;i++ ) pr_ob(indent+3,ob->u.t->obs[i]);
        }
    } else if( is_mark(ob) ){
        strcpy(&buf[i],"[MARK]");
        mlwrite(buf);
    } else if( is_name(ob) ){
        sprintf(&buf[i],"-->");
        mlwrite(buf);
        pr_ob(6,ob->u.v->ob);
    } else {
        strcpy(&buf[i],"[UNKNOWN]");
        mlwrite(buf);
    }
}

void print_ob(ob)
OBJECT *ob;
{
    pr_ob(0,ob);
}

OBJECT *dup_ob(ob)
OBJECT *ob;
{
    OBJECT *ob1;

    ob1 = new_ob("dup_ob");
    if( ob1 != (OBJECT *)NULL ){
        ob1->flags = ob->flags;
        if( is_int(ob) ){
            ob1->u.z = ob->u.z;
            n_ints++;
        } else if( is_real(ob) ){
            ob1->u.r = ob->u.r;
            n_reals++;
        } else if( is_string(ob) ){
            ob1->u.s = dup_string(ob->u.s);
            if( ob1->u.s == NULL ){
                free_ob(ob1);
                ob1 = NULL;
            }
        } else if( is_code(ob) ){
            mlwrite("eh!? duplicating a code object");
            ob1->u.f = ob->u.f;
        } else if( is_thread(ob) ){
            ob1->u.t = dup_thread(ob->u.t);
        } else {
            mlwrite("dup_ob: unknown object type");
        }
    }
    return ob1;
} /* dup_ob */


VAR *new_var()
{
    VAR *v;

    v = (VAR *)MALLOC("new_var", sizeof(VAR));
    n_names++;
    return v;
}

void free_var(v)
VAR *v;
{
    if( v == (VAR *)NULL ) return;
    if( v->ob ) free_ob(v->ob);
    FREE("free_var",v);
}

void dump_namelist()
{
    VAR *v;
    v = namelist;
    while( v != (VAR *)NULL ){
        printf("%s: ",v->name);
        pr_ob(0,v->ob);
        v = v->next;
    }
}

VAR *lookup( name )
char * name;
{
    VAR *v = namelist;
    VAR *tv;

    if( !strcmp( name, v->name ) ) return v;
        
    while( v->next ){
        if( !strcmp( name, v->next->name ) ){
            tv = v->next;
            v->next = v->next->next;
            tv->next = namelist;
            namelist = tv;
            return tv;
        }
        v = v->next;
    }
    return (VAR *)NULL;
}

VAR *insert_ob(name,ob)
char *name;
OBJECT *ob;
{
    VAR *v;

    if( (v = new_var()) == (VAR *)NULL )  return NULL;

    v->next = namelist;
    namelist = v;
    _bind(ob);
    v->ob = ob;

    if( (v->name = dup_string(name)) == (char *)NULL ){
        mlwrite("insert_ob: out of memory");
        return NULL;
    }
    return v;
}

void install_ob(name,ob)
char *name;
OBJECT *ob;
{
    VAR *v;

    if( (v = new_var()) == (VAR *)NULL ) {
            /* error */
            return;
    }
    v->next = namelist;
    namelist = v;
    _bind(ob);
    v->ob = ob;
    v->name = dup_string(name);
}

        
OBJECT *install_code(name,f,flags)
char * name;
void (*f)();
int flags;
{
    OBJECT *ob;

    ob = new_ob("install_code");
    if( ob == (OBJECT *)NULL ) return (OBJECT *)NULL;
    ob->u.f = f;
    ob->flags = OB_CODE | flags;
    if( insert_ob(name,ob) ) return ob;
    else {
        fprintf(stderr,"install_code: initialization error");
        exit(1);
    }
}

void rm_name( name )
char *name;
{
    VAR *tv;
    tv = lookup( name );
    if( tv ){
        if( is_code(tv->ob) ){
            mlwrite("Can't delete code");
            return;
        }
        namelist = tv->next;
        free_ob(tv->ob);
        free_var(tv);
    }
}

char *lookup_ob( ob )
OBJECT * ob;
{
    VAR *v = namelist;
    VAR *tv;

    if( ob == v->ob ) return v->name;
        
    while( v->next ){
        if( ob == v->ob ){
            tv = v->next;
            v->next = v->next->next;
            tv->next = namelist;
            namelist = tv;
            return v->name;
        }
        v = v->next;
    }
    return NULL;
}

void push(ob)
OBJECT *ob;
{
    if( top>=MAXSTACK ){
            mlwrite("stack is full");
            interrupted = TRUE;
            return;
    }
    stack[top++] = ob;
}

OBJECT *pop()
{
    if( top <= 0 ){
        interrupted = TRUE;
        mlwrite("Empty stack.");
        return (OBJECT *)NULL;
    }
    return stack[--top];
}

OBJECT *make_int_ob(i)
int i;
{
    OBJECT *ob;
    ob = new_ob("make_int_ob");
    if( ob == (OBJECT *)NULL ){
        mlwrite("make_int_ob: out of memory");
        return &undefined_ob;
    }
    ob->flags = OB_INT;
    ob->u.z = i;
    n_ints++;
    return ob;
}

OBJECT *make_real_ob(z)
double z;
{
    OBJECT *ob;
    ob = new_ob("make_real_ob");
    if( ob == (OBJECT *)NULL ){
        mlwrite("make_real_ob: out of memory");
        return &undefined_ob;
    }
    ob->flags = OB_REAL;
    ob->u.r = z;
    n_reals++;
    return ob;
}


void push_int(i)
int i;
{
    OBJECT *ob;
    if( top>=MAXSTACK ){
        mlwrite("stack is full");
        interrupted = TRUE;
        return;
    }
    ob = make_int_ob(i);
    push(ob);
}

void push_real(z)
double z;
{
    OBJECT *ob;
    if( top>=MAXSTACK ){
        mlwrite("stack is full");
        interrupted = TRUE;
        return;
    }
    ob = make_real_ob(z);
    push(ob);
}

void push_name()
{
    VAR *v;

    if( top < 1 ) return;
    if( !(is_string(stack[top-1])) )return;
    v = lookup( (char *)stack[top-1]->u.v );
    if( v ){
        push(v->ob);
    }
}

OBJECT *make_string_ob(s)
char *s;
{
    OBJECT *ob;
    ob = new_ob("make_string_ob");
    if( ob == (OBJECT *)NULL ){
        mlwrite("make_string_ob: out of memory");
        return &undefined_ob;
    }
    ob->flags = OB_STRING;
    ob->u.s = s;
    return ob;
}

void push_string(s)
char *s;
{
    OBJECT *ob;
    if( top>=MAXSTACK ){
        mlwrite("stack is full");
        interrupted = TRUE;
        return;
    }
    ob = make_string_ob(s);
    push(ob);
}

void arithmetic_op(op)
int op;
{
    OBJECT *ob1,*ob2;

    chk_stk(op_name[op],2);
    ob1 = pop();
    ob2 = pop();

    if( (!is_int(ob1) && !is_real(ob1)) || 
            (!is_int(ob2) && !is_real(ob2))
        ) {
        sprintf(msg,"illegal operands for arithmetic operation");
        goto errors;
    }
    if( (op == OP_DIV) && 
        ( ( is_int(ob1)  && (ob1->u.z == 0) ) ||
          ( is_real(ob1) && (ob1->u.r == 0.0) ) 
        ) 
    ) {
        sprintf(msg,"attempt to divide by zero");
        goto errors;
    }

    if( is_int(ob1) && is_int(ob2) ){
        switch (op){
        case OP_ADD:    ob2->u.z += ob1->u.z; break;
        case OP_SUB:    ob2->u.z -= ob1->u.z; break;
        case OP_MUL:    ob2->u.z *= ob1->u.z; break;
        case OP_DIV:    ob2->u.z /= ob1->u.z; break;
        case OP_MOD:    ob2->u.z %= ob1->u.z; break;
        case OP_AND:    ob2->u.z &= ob1->u.z; break;
        case OP_OR:    ob2->u.z |= ob1->u.z; break;
        default:    goto operand_err;
        }
        free_ob(ob1);
        push(ob2);
        return;
    } else {
        if( is_int(ob1) ){
            ob1->flags &= (~OB_INT); n_ints--;
            ob1->flags |= OB_REAL;  n_reals++;
            ob1->u.r = (double)ob1->u.z;
        }
        if( is_int(ob2) ){
            ob2->flags &= (~OB_INT); n_ints--;
            ob2->flags |= OB_REAL;  n_reals++;
            ob2->u.r = (float)ob2->u.z;
        }
        switch (op){
        case OP_ADD:    ob2->u.r += ob1->u.r; break;
        case OP_SUB:    ob2->u.r -= ob1->u.r; break;
        case OP_MUL:    ob2->u.r *= ob1->u.r; break;
        case OP_DIV:    ob2->u.r /= ob1->u.r; break;
        default:    goto operand_err;
        }
        free_ob(ob1);
        push(ob2);
        return;
    }
operand_err:
    sprintf(msg,"%s: wrong type for operands",op_name[op]);
errors:
    push(ob2);
    push(ob1);
    mlwrite(msg);
    interrupted = TRUE;
    return;
}

void relational_op(op)
int op;
{
    OBJECT *ob1,*ob2;

    chk_stk(op_name[op],2);
    ob1 = pop();
    ob2 = pop();
    
    if( is_int(ob1) && is_int(ob2) ){
        switch (op){
        case OP_EQ:    ob2->u.z = (ob2->u.z == ob1->u.z);break;
        case OP_NE:    ob2->u.z = (ob2->u.z != ob1->u.z);break;
        case OP_LT:    ob2->u.z = (ob2->u.z  > ob1->u.z);break;
        case OP_GT:    ob2->u.z = (ob2->u.z  < ob1->u.z);break;
        case OP_LE:    ob2->u.z = (ob2->u.z >= ob1->u.z);break;
        case OP_GE:    ob2->u.z = (ob2->u.z <= ob1->u.z);break;
        default:    goto operand_err;
        }
        free_ob(ob1);
        push(ob2);
        return;
    } else if( is_string(ob1) && is_string(ob2) ){
        ob2->flags = OB_INT;
        switch (op){
        case OP_EQ:    ob2->u.z = (strcmp(ob1->u.s,ob2->u.s) == 0);
                break;
        case OP_NE:    ob2->u.z = (strcmp(ob1->u.s,ob2->u.s) != 0);
                break;
        case OP_LT:    ob2->u.z = (strcmp(ob1->u.s,ob2->u.s) < 0);
                break;
        case OP_GT:    ob2->u.z = (strcmp(ob1->u.s,ob2->u.s) > 0);
                break;
        case OP_LE:    ob2->u.z = (strcmp(ob1->u.s,ob2->u.s) <= 0);
                break;
        case OP_GE:    ob2->u.z = (strcmp(ob1->u.s,ob2->u.s) >= 0);
                break;
        default:    goto operand_err;
        }
        free_ob(ob1);
        push(ob2);
        return;
    } else if( (is_int(ob1) || is_real(ob1))  && 
            (is_int(ob2) || is_real(ob2)) 
        ){
        if( is_int(ob1) ){
            ob1->u.r = (double)ob1->u.z;
        }
        if( is_int(ob2) ){
            ob2->flags &= (~OB_INT); n_ints--;
            ob2->flags |= OB_REAL;  n_reals++;
            ob2->u.r = (double)ob2->u.z;
        }
        switch (op){
          case OP_EQ:    ob2->u.z = (ob2->u.r == ob1->u.r);break;
          case OP_NE:    ob2->u.z = (ob2->u.r != ob1->u.r);break;
          case OP_LT:    ob2->u.z = (ob2->u.r  > ob1->u.r);break;
          case OP_GT:    ob2->u.z = (ob2->u.r  < ob1->u.r);break;
          case OP_LE:    ob2->u.z = (ob2->u.r >= ob1->u.r);break;
          case OP_GE:    ob2->u.z = (ob2->u.r <= ob1->u.r);break;
        default:    goto operand_err;
        }
        free_ob(ob1);
        push(ob2);
        return;
    }

operand_err:
    sprintf(msg,"%s: wrong type for operands",op_name[op]);
errors:
    push(ob2);
    push(ob1);
    mlwrite(msg);
    interrupted = TRUE;
    return;
}
void string_op(op)
int op;
{
    OBJECT *ob1,*ob2;
    int i;

    chk_stk(op_name[op],2);

    ob1 = pop();
    ob2 = pop();
    
    if( is_string(ob1) && is_string(ob2) ){
        ob2->flags = OB_INT;
        switch (op){
        case OP_CMP:    ob2->u.z = strcmp(ob1->u.s,ob2->u.s);
                break;
        case OP_CAT:    i = strlen(ob2->u.s) + strlen(ob1->u.s) + 1;
                ob2->u.s =  (char *)REALLOC("cat",ob2->u.s,i);
                (void)strcat(ob2->u.s,ob1->u.s);
                ob2->flags = OB_STRING;
                break;
        default:    goto operand_err;
        }
        free_ob(ob1);
        push(ob2);
        return;
    } 
operand_err:
    sprintf(msg,"%s: wrong type for operands",op_name[op]);
errors:
    push(ob2);
    push(ob1);
    mlwrite(msg);
    interrupted = TRUE;
    return;
}

void add() { arithmetic_op(OP_ADD); }
void sub() { arithmetic_op(OP_SUB); }
void mul() { arithmetic_op(OP_MUL); }
void div() { arithmetic_op(OP_DIV); }
void mod() { arithmetic_op(OP_MOD); }
void eq() { relational_op(OP_EQ); }
void ne() { relational_op(OP_NE); }
void lt() { relational_op(OP_LT); }
void gt() { relational_op(OP_GT); }
void le() { relational_op(OP_LE); }
void ge() { relational_op(OP_GE); }
void cmp() { string_op(OP_CMP); }
void cat() { string_op(OP_CAT); }

void set_hex() { oct_mode = 0; hex_mode = 1; }
void set_oct() { oct_mode = 1; hex_mode = 0; }
void set_dec() { oct_mode = 0; hex_mode = 0; }

void mark()
{
    OBJECT *ob;
    ob = new_ob("mark");
    if( ob == (OBJECT *)NULL ){
        mlwrite("mark: out of memory");
        interrupted = TRUE;
    }
    ob->flags = OB_MARK;
    push(ob);
}

void clear_to_mark()
{
    while( top > 0 && !(stack[top-1]->flags & OB_MARK)) drop();
}

int _count_to_mark()
{
    int i = top - 1;
    int count = 0;

    while( i >= 0 && !(stack[i]->flags & OB_MARK)){
        count++;
        i--;
    }
    if( i < 0 ){
        mlwrite("_count_to_mark: no mark");
        return -1;
    }
    return count;
}

void count_to_mark() { push_int(_count_to_mark()); }


void pack_thread()
{
    OBJECT *ob;
    OBJECT *ob1;
    THREAD *t;
    int len;
    int i;

    len = _count_to_mark();
    if( len < 0 ) {
        mlwrite("pack_thread: error creating array");
        return;
    }

    t = new_thread(len);

    if( t == (THREAD *)NULL ){
        mlwrite("pack_thread: error creating array");
        return;
    }

    for( i=len-1;i>=0;i-- ) {
        ob = pop();
        t->obs[i] = ob;
    }
    free_ob(pop()); /* the mark */

    ob = new_ob("pack_thread:ob");
    if( ob == (OBJECT *)NULL ){
        return;
    }
    ob->flags = OB_THREAD;
    ob->u.t = t;
    push(ob);
}

void printtop()
{
    chk_stk("printtop",1);
    print_ob(stack[top-1]);
}

void printpop()
{
    OBJECT *ob;
    chk_stk("printpop",1);
    ob = pop();
    print_ob(ob);
    free_ob(ob);
}

void pr_top()
{
    if( top < 1 ) return;
    pr_ob(0,stack[top-1]);
}

void printstack()
{
    int i;
    for( i = top-1;i>=0;i--) {
        pr_ob(2,stack[i]);
    }
}

void x_dup()
{
    chk_stk("dup",1);
    push(dup_ob(stack[top-1]));
}

void drop()
{
    OBJECT *ob;
    ob = pop();
    free_ob(ob);
}

void over()
{
    chk_stk("over",2);
    push(dup_ob(stack[top-2]));
}

void rot()
{
    OBJECT *ob1,*ob2,*ob3;
    chk_stk("rot",3);
    ob1 = pop();
    ob2 = pop();
    ob3 = pop();
    push(ob2);
    push(ob1);
    push(ob3);
}

void exch()
{
    OBJECT *ob1,*ob2;
    chk_stk("exch",2);
    ob1 = pop();
    ob2 = pop();
    push(ob1);
    push(ob2);
}

void clear() {    while( top > 0 ) drop(); }

void stack_depth()
{
    push_int(top);
}

void load_file()
{
    OBJECT *ob;

    chk_stk("load",1);
    ob = pop();
    if( !is_string(ob) ){
        mlwrite("load_file: no file name");
        free_ob(ob);
        return;
    }
    if( !access(ob->u.s,R_OK) ) 
        file_input(ob->u.s);
    else {
        mlwrite("load_file: can't access file");
    }
    free_ob(ob);
}

char *scan_string()
{
    char *s = (char *)NULL;
    char c;
    int sx = 0;
    int sz = 16;
    int parens = 1;

    s = (char *)MALLOC("scan_string",sz+1);
    if( !s ) return s;
    while( TRUE ){
        if( sx >= sz ){
            sz += (sz+1);
            s = (char *)REALLOC("scan_string", s,sz);
            if( s == (char *)NULL )    return s;
        }

        c = nextc();
        if( c == '\\' ){
            c = nextc();
        } else if( c == END_STRING ) {
            parens--;
        } else if( c == START_STRING ) {
            parens++;
        }
        if( parens <= 0 ) break;
        s[sx++] = c;
    }
    s[sx] = '\0';
    n_strings++;
    return s;
} /* scan_string */

char *get_word()
{
    char c,nc;
    
    wx = 0;
    while( isspace(word[0] = nextc()) );
    wx++;
    if( word[0] == START_STRING ){
        word[1] = '\0';
        return word;
    }
    while( !isspace(word[wx] = nextc()) && (wx <= MAXWORD) ) wx++;
    if( wx > MAXWORD ) mlwrite("get_word: word truncated");
    word[wx] = '\0';
    return word;
}

int number(w)
char *w;
{
    int i,len;
    double f;
    int sign;

    i = 0;
    sign = 1;
    len = strlen(w);
        zreal = 0.0;

        if( w[i] == '0' ) {
            i++;
            zint = 0;
            if( w[i] == 'x' || w[i] == 'X' ) {
        i++;
                for( ; i<len;i++ ) {
                    if( w[i] <= 'f' && w[i] >= 'a' ) {
                        zint = zint*16 + (w[i]-'a' + 10);
                        continue;
                    } else if( w[i] <= 'F' && w[i] >= 'A' ) {
                        zint = zint*16 + (w[i]-'A' + 10);
                        continue;
                    } else if( isdigit(w[i]) ) {
                        zint = zint*16 + (w[i]-'0');
                        continue;
                    } else {
                        return 0;
                    }
                }
                return OB_INT;
            } else { /* octal */
                for( ; i<len ;i++ ) {
                    if( w[i] < '8' && w[i] >= '0' ) {
                        zint = zint*8 + w[i]-'0';
                    } else {
                        return 0;
                    }
                }
                return OB_INT;
            }
    } else if( w[i] == '-' ) {
        sign = -1;
        i++;
    } else if( w[i] == '+' ) {
        i++;
    }

    zreal = 0;
    for( ;i<len;i++ ){
        if( w[i] == '.' ){
            i++;
            goto real;
        }
        if( !isdigit(w[i]) )return 0;
        zreal = zreal*10.0 + (double)(w[i]-'0');
    }
    if( zreal < INT_MAX ){
        zint = sign * zreal;
        return OB_INT;
    }
real:
    f = 0.1;
    for( ;i<len;i++ ){
        if( !isdigit(w[i]) )return 0;
        zreal += (w[i]-'0')*f;
        f /= 10.0;
    }
    zreal *= sign;
    return OB_REAL;
}

exec_thread(t)
THREAD *t;
{
    char *s;
    int tx;
    tx = 0;    

    while( tx < t->len ){
        exec( t->obs[tx++] );
        if( exit_thread || break_loop || interrupted ) break;
    }
    exit_thread = 0;
}


void exec(ob)
OBJECT *ob;
{

    run_level++;
    if( is_code(ob) ) {
         (*ob->u.f)();
    } 
    else if( is_int(ob) ) {
        push_int(ob->u.z);
    }
    else if( is_real(ob) ) {
        push_real(ob->u.r);
    }
    else if( is_string(ob) ) {
        push_string(dup_string(ob->u.s));
    }
    else if( is_thread(ob) ) {
        if( is_bound(ob) ) exec_thread(ob->u.t);
        else push(ob);
    }
    else if( is_name(ob) ) {
        push(ob);
    }
    else {
        mlwrite("exec: Internal error -- no flag");
    }
    if( (--run_level) <= 0 ) free_ob(ob);
}

void x_exec()
{ 
    OBJECT *ob;
    chk_stk("exec",1);
    ob = pop();
    if( is_thread(ob) ) {
        exec_thread(ob->u.t);
        free_ob(ob);
    } else {
        exec(ob);    
    }
}

void run_next() { run_next_flag = TRUE; }

void interp()
{
    VAR *v;
    OBJECT *ob;
    char *w;
    int flag;

    while( TRUE ){
        interrupted = FALSE;
        run_level = 0;
        w = get_word();    
        if( trace_mem ) mlwrite(w);

        if( w[0] == START_STRING ){
            push_string(scan_string());
            continue;
        }

        if( (v = lookup(w)) == (VAR *)NULL ){
            flag = number(w);
            if( flag == OB_INT ){
                push_int(zint);
            } else if( flag == OB_REAL ){
                push_real(zreal);
            } else {
                /* create a variable with value "undefined value" */
                /* need error handling*/
                v = insert_ob(w,&undefined_ob);
                ob = new_ob("interp:new_var");
                ob->flags = OB_NAME;
                ob->u.v = v;
                push(ob);
            }
        } else if( compile_depth==0 || is_immed(v->ob) || run_next_flag ) {
            run_next_flag = FALSE;
            exec(v->ob);
        } else {
            ob = new_ob("interp:old_var");
            ob->flags = OB_NAME;
            ob->u.v = v;
            push(ob);
        }
    }
} /* interp */

void compile()
{
    mark();
    compile_depth++;
}

void end_compile()
{
    pack_thread();
    compile_depth--;
}

void set()
{
    VAR *v;
    OBJECT *ob;
    char *name;

    chk_stk("set",1);
    ob = pop();
    name = get_word();

    if( (v = lookup(name)) == (VAR *)NULL ){
        (void)insert_ob(name,ob);
    } else {
        if( is_code(v->ob) ){
            mlwrite("set: can't redefine a code object");
        } else {
            free_ob(v->ob);
            _bind(ob);
            v->ob = ob;
        }
    }
}


void pr_var()
{
    OBJECT *ob1;
    VAR *v;
    int t_verbose = verbose_dmp;

    chk_stk("pr_var",1);

    ob1 = pop();
    verbose_dmp = 1;
    if( is_string(ob1) ) {
        if( (v = lookup(ob1->u.s)) ) {
            pr_ob(strlen(ob1->u.s),v->ob);
        } else {
            printf("[NULL]\n");
        }
    } else {
        mlwrite("dmpvar: TOS must be a string");
    }
    verbose_dmp = t_verbose;
    FREE("pr_var",ob1);
}

void def()
{
    OBJECT *ob1, *ob2;
    VAR *v;

    chk_stk("def",2);

    ob1 = pop();
    ob2 = pop();
    if( is_string(ob1) ) {
        if( (v = lookup(ob1->u.s)) ) {
            v->ob->flags &= ~(OB_BOUND);
            FREE("def1",v->ob);
            v->ob = ob2;
            ob2->flags |= (OB_BOUND);
        } else {
            install_ob(ob1->u.s,ob2);
        }
        FREE("def",ob1); /* leave the string for the namelist */
    } else if( is_thread(ob2) ) {
        ob1->u.t = ob2->u.t;
        if( is_bound(ob1) ) ob1->flags = OB_BOUND;
        ob1->flags |= OB_THREAD;
    } else {
        mlwrite("set: TOS is not of right type");
        return;
    }
}

void store()
{
    OBJECT *ob1, *ob2;

    chk_stk("store",2);

    ob1 = pop();
    ob2 = pop();
    if( ! is_name(ob1) ) {
        mlwrite("store: not a variable");
        return;
    }
    
    free_ob(ob1->u.v->ob);
    ob1->u.v->ob = ob2;
    ob2->links++;
}

void load()
{
    OBJECT *ob1, *ob2;

    chk_stk("load",1);

    ob1 = pop();

    if( is_bound(ob1) ) {
        if( is_int(ob1) ) {
            push_int(ob1->u.z);
        } else if( is_real(ob1) ) {
            push_int(ob1->u.r);
        } else if( is_string(ob1) ) {
            push_string(ob1->u.s);
        } else {
            mlwrite("load: illegal variable value");
        }
    } else { 
        mlwrite("load: no variable");
        return;
    }
}

void rpt()
{
    OBJECT *ob1,*ob2;
    int i;

    chk_stk("rpt",2);
    ob1 = pop();
    ob2 = pop();
    if( ! is_int(ob1) ) {
        mlwrite("illegal count for rpt");
        free_ob(ob1);
        if( run_level <= 1 ) free_ob(ob2);
        return;
    }

    for( i = 0;i< ob1->u.z; i++ ){
        if( break_loop > 0 ) break;
        if( interrupted ) break;
        if( is_thread(ob2) ) {
            exec_thread(ob2->u.t);
        } else {
            exec(ob2);    
        }
    }

    free_ob(ob1);

    /* ints, reals are always copied to stack, strings are copied on 
       write, code is never freed, so threads (so far) are the only
       thing to worry about as far as memory management is concerned
    */
    if( (run_level <= 1) || ! (is_thread(ob2)) ) { 
        free_ob(ob2);
    }

    break_loop = 0;
}

void loop()
{
    OBJECT *ob;

    chk_stk("loop",1);
    ob = pop();
    while( TRUE ){ 
        if( break_loop > 0 ) break;
        if( interrupted ) break;
        exec(ob);
    }
    free_ob(ob);
    break_loop = 0;
}

void brk0() { break_loop = 1;}
void brk1() { push(lookup(" _BRK_ ")->ob); }
void exit0() { exit_thread = 1;}
void exit1() { push(lookup(" _EXIT_ ")->ob); }



void not()
{
    OBJECT *ob;
    chk_stk("not",1);
    ob = pop();
    if( ob->u.z ) ob->u.z = 0;
    else ob->u.z = 1;
    push(ob);
}

void x_if()
{
    OBJECT *ob1,*ob2;

    chk_stk("if",2);
    ob1 = pop();
    ob2 = pop();
    if( ob2->u.z ){
        exec(ob1);
    }
    free_ob(ob1);
    free_ob(ob2);
}
    
void init()
{
    signal( SIGINT, handler );
    install_code("t",test,0);
    install_code("q",quit,0);
    install_code("+",add,0);
    install_code("-",sub,0);
    install_code("*",mul,0);
    install_code("/",div,0);
    install_code("%",mod,0);
    install_code(".",cat,0);
    install_code("=",printpop,0);
    install_code("?",printtop,0);
    install_code("dec",set_dec,0);
    install_code("hex",set_hex,0);
    install_code("oct",set_oct,0);
    install_code("not",not,0);
    install_code("ne",ne,0);
    install_code("eq",eq,0);
    install_code("lt",lt,0);
    install_code("gt",gt,0);
    install_code("le",le,0);
    install_code("ge",ge,0);
    install_code("strcmp",cmp,0);
    install_code("depth",stack_depth,0);
    install_code("dup",x_dup,0);
    install_code("drop",drop,0);
    install_code("over",over,0);
    install_code("rot",rot,0);
    install_code("exch",exch,0);
    install_code("clear",clear,0);
    install_code(".c",clear,0);
    install_code("exec",x_exec,0);
    install_code("rpt",rpt,0);
    install_code("def",def,0);
    install_code("!",store,0);
    install_code("@",load,0);
    install_code("loop",loop,0);
    install_code("if",x_if,0);
    install_code("load",load_file,0);
    install_code(".s",printstack,0);
    install_code("namelist",dump_namelist,0);
    install_code("mem",mem,0);
    install_code("mem_trace",mem_trace,0);
    install_code("dmptop",pr_top,0);
    install_code("dmpvar",pr_var,0);
    install_code("push",push_name,0);
    install_code("verbose",toggle_verbose,0);
    install_code(".v",toggle_verbose,0);
    install_code(" _UNDEFINED_ ",undefined,0);
    install_code(" _BRK_ ",brk0,0);
    install_code("break",brk1,0);
    install_code(" _EXIT_ ",exit0,0);
    install_code("exit",exit1,0);
    install_code("[",mark,OB_IMMED);
    install_code("]",pack_thread,OB_IMMED);
    install_code("{",compile,OB_IMMED);
    install_code("}",end_compile,OB_IMMED);
    install_code("count_to_mark",count_to_mark,0);
    install_code("$",run_next,OB_IMMED);

    undefined_ob.flags = OB_CODE;
    undefined_ob.u.f = undefined;
    undefined_th.len = 1;
    undefined_th.obs = undefined_obs;

    if( !access(".melrc",R_OK) ) file_input(".melrc");

}

#ifdef STANDALONE
main()
{
    init();
    interp();
}
#endif
