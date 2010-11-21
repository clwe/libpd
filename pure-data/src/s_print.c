/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "s_stuff.h"
#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf sprintf_s
#endif

t_printhook sys_printhook;
int sys_printtostderr;

/* escape characters for tcl/tk */
static char* strnescape(char *dest, const char *src, size_t len)
{
    int ptin = 0;
    unsigned ptout = 0;
    for(; ptout < len; ptin++, ptout++)
    {
        int c = src[ptin];
        if (c == '\\' || c == '{' || c == '}' || c == ';')
            dest[ptout++] = '\\';
        dest[ptout] = src[ptin];
        if (c==0) break;
    }

    if(ptout < len) 
        dest[ptout]=0;
    else 
        dest[len-1]=0;

    return dest;
}

static void dopost(const char *s)
{
    if (sys_printhook)
        (*sys_printhook)(s);
    else if (sys_printtostderr)
        fprintf(stderr, "%s", s);
    else
    {
        char upbuf[MAXPDSTRING];
        sys_vgui("::pdwindow::post 3 {%s}\n", strnescape(upbuf, s, MAXPDSTRING));
    }
}

static void doerror(const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

    // what about sys_printhook_error ?
    if (sys_printhook) 
    {
        snprintf(upbuf, MAXPDSTRING-1, "error: %s", s);
        (*sys_printhook)(upbuf);
    }
    else if (sys_printtostderr)
        fprintf(stderr, "error: %s", s);
    else
    {
        sys_vgui("::pdwindow::post 1 {%s}\n", strnescape(upbuf, s, MAXPDSTRING));
    }
}
static void doverbose(int level, const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

    // what about sys_printhook_verbose ?
    if (sys_printhook) 
    {
        snprintf(upbuf, MAXPDSTRING-1, "verbose(%d): %s", level, s);
        (*sys_printhook)(upbuf);
    }
    else if (sys_printtostderr) 
    {
        fprintf(stderr, "verbose(%d): %s", level, s);
    }
    else
    {
        sys_vgui("::pdwindow::post %d {%s}\n", level+4, strnescape(upbuf, s, MAXPDSTRING));
    }
}

static void dobug(const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

    // what about sys_printhook_bug ?
    if (sys_printhook) 
    {
        snprintf(upbuf, MAXPDSTRING-1, "consistency check failed: %s", s);
        (*sys_printhook)(upbuf);
    }
    else if (sys_printtostderr)
        fprintf(stderr, "consistency check failed: %s", s);
    else
    {
        char upbuf[MAXPDSTRING];
        sys_vgui("::pdwindow::post 3 {%s}\n", strnescape(upbuf, s, MAXPDSTRING));
    }
}


void post(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    dopost(buf);
}

void startpost(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    dopost(buf);
}

void poststring(const char *s)
{
    dopost(" ");

    dopost(s);
}

void postatom(int argc, t_atom *argv)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        char buf[MAXPDSTRING];
        atom_string(argv+i, buf, MAXPDSTRING);
        poststring(buf);
    }
}

void postfloat(t_float f)
{
    char buf[80];
    t_atom a;
    SETFLOAT(&a, f);

    postatom(1, &a);
}

void endpost(void)
{
    if (sys_printhook)
        (*sys_printhook)("\n");
    else if (sys_printtostderr)
        fprintf(stderr, "\n");
    else post("");
}

void error(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;

    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    doerror(buf);
    endpost();
}

void verbose(int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;

    if(level>sys_verbose)return;

    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    doverbose(level, buf);

    endpost();
}

    /* here's the good way to log errors -- keep a pointer to the
    offending or offended object around so the user can search for it
    later. */

static void *error_object;
static char error_string[256];
void canvas_finderror(void *object);

void pd_error(void *object, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    static int saidit;

    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    doerror(buf);
    endpost();  

    error_object = object;
    if (!saidit)
    {
        verbose(0, "... you might be able to track this down from the Find menu.");
        saidit = 1;
    }
}

void glob_finderror(t_pd *dummy)
{
    if (!error_object)
        post("no findable error yet.");
    else
    {
        post("last trackable error:");
        post("%s", error_string);
        canvas_finderror(error_object);
    }
}

void bug(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    dobug(buf);
    endpost();
}

    /* this isn't worked out yet. */
static const char *errobject;
static const char *errstring;

void sys_logerror(const char *object, const char *s)
{
    errobject = object;
    errstring = s;
}

void sys_unixerror(const char *object)
{
    errobject = object;
    errstring = strerror(errno);
}

void sys_ouch(void)
{
    if (*errobject) error("%s: %s", errobject, errstring);
    else error("%s", errstring);
    sys_gui("bell\n");
}
