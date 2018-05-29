/*
 * this is a comment
 * with text on the last line */

/* func on same line as type; OK in a prototype */
int foo();


/* func on same line as type */
int foo(){
}
static const struct blargle *myfunc(int x, char *y, struct foo *f){
}
/* this one is really long */
static int zfs_close(vnode_t *vp, int flag, int count, offset_t offset,
    cred_t *cr, caller_context_t *ct)

/* Omit the space after some keywords */
int
foo(){
	if(1) {
		while(7){
			if (foo){
			}
		}
	}
}

// A C++ style comment
int foo;    //C++ style inline comment


/* Solo brace after control block */
long
bar(){
	if (x)
	{
		1;
	}
}

/* Empty loop with space before the semicolon */
float
baz()
{
	for (i=0; i<10; i++) ;
}


/* bad inline function */
inline static int zero(void);

/* long control block without braces around single statement body */
if (this
        && that
        && the other)
    do_stuff();


/* Return statement without parens around the argument */
int
foo()
{
    return 0;
}

/* Void return statement without parens.  This is ok */
void
voidfoo()
{
    return ;
}

