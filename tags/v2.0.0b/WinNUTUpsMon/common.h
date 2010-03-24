#ifndef NUT_COMMON_H
#define NUT_COMMON_H

/* common.h - prototypes for the common useful functions

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <windows.h>
#include <process.h>
#include <winbase.h>
#include <io.h>
#include <math.h>
#include <assert.h>
#include <tchar.h>
#include "syslog.h"

#include "config.h"
#include "version.h"

int snprintfcat(char *dst, size_t size, const char *fmt, ...);

const char *xbasename(const char *file);

/* enable writing upslog() type messages to the syslog */
void syslogbit_set(void);

void upslog(int priority, const char *fmt, ...);
/*void upslogx(int priority, const char *fmt, ...);*/
void upsdebug(int level, const char *fmt, ...);
void upsdebugx(int level, const char *fmt, ...);

void fatal(const char *fmt, ...);
void fatalx(const char *fmt, ...);

extern int nut_debug_level;

void *xmalloc(size_t size);
void *xcalloc(size_t number, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *string);

void rtrim(char *in, char sep);

/* Buffer sizes used for various functions */
#define SMALLBUF	512
#define LARGEBUF	1024

/* Provide declarations for getopt() global variables */

#ifdef NEED_GETOPT_H
#include <getopt.h>
#else
#ifdef NEED_GETOPT_DECLS
extern char *optarg;
extern int optind; 
#endif /* NEED_GETOPT_DECLS */
#endif /* HAVE_GETOPT_H */

/* logging flags: bitmask! */

#define UPSLOG_STDERR		0x0001
#define UPSLOG_SYSLOG		0x0002
#define UPSLOG_STDERR_ON_FATAL	0x0004
#define UPSLOG_SYSLOG_ON_FATAL	0x0008

#ifndef HAVE_SETEUID
#	define seteuid(x) setresuid(-1,x,-1)    /* Works for HP-UX 10.20 */
#	define setegid(x) setresgid(-1,x,-1)    /* Works for HP-UX 10.20 */
#endif

#endif /* NUT_COMMON_H */
