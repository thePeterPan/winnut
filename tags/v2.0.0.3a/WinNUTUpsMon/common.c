/* common.c - common useful functions

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
/*
#include "common.h"

#include <ctype.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
*/
#include "common.h"
#include "syslog.h"
#include "WinCompatabilityFunctions.h"


	int	nut_debug_level = 0;
	static	int	upslog_flags = UPSLOG_STDERR;

static void xbit_set(int *val, int flag)
{
	*val = (*val |= flag);
}

static void xbit_clear(int *val, int flag)
{
	*val = (*val ^= (*val & flag));
}

static int xbit_test(int val, int flag)
{
	return ((val & flag) == flag);
}

/* enable writing upslog() type messages to the syslog */
void syslogbit_set(void)
{
	xbit_set(&upslog_flags, UPSLOG_SYSLOG);
}

int snprintfcat(char *dst, size_t size, const char *fmt, ...)
{
	va_list ap;
	size_t len = strlen(dst);
	int ret;

	size--;
	assert(len <= size);

	va_start(ap, fmt);
	ret = vsnprintf(dst + len, size - len, fmt, ap);
	va_end(ap);

	dst[size] = '\0';
	return len + ret;
}

const char *xbasename(const char *file)
{
	const char *p = strrchr(file, '/');

	if (p == NULL)
		return file;
	return p + 1;
}

static void vupslog(int priority, const char *fmt, va_list va, int use_strerror)
{
	int	ret;
	char	buf[LARGEBUF];

	ret = vsnprintf(buf, sizeof(buf), fmt, va);

	if ((ret < 0) || (ret >= (int) sizeof(buf)))
		syslog(LOG_WARNING, "vupslog: vsnprintf needed more than %d bytes",
			LARGEBUF);

	if (use_strerror)
		snprintfcat(buf, sizeof(buf), ": %s", strerror(errno));

	if (xbit_test(upslog_flags, UPSLOG_STDERR))
		fprintf(stderr, "%s\n", buf);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG))
		syslog(priority, "%s", buf);
}

/* logs the formatted string to any configured logging devices + the output of strerror(errno) */
void upslog(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vupslog(priority, fmt, va, 1);
	va_end(va);
}

/* logs the formatted string to any configured logging devices */
/*
void upslogx(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vupslog(priority, fmt, va, 0);
	va_end(va);
}*/

void upsdebug(int level, const char *fmt, ...)
{
	va_list va;
	
	if (nut_debug_level < level)
		return;

	va_start(va, fmt);
	vupslog(LOG_DEBUG, fmt, va, 1);
	va_end(va);
}

void upsdebugx(int level, const char *fmt, ...)
{
	va_list va;
	
	if (nut_debug_level < level)
		return;

	va_start(va, fmt);
	vupslog(LOG_DEBUG, fmt, va, 0);
	va_end(va);
}

static void vfatal(const char *fmt, va_list va, int use_strerror)
{
	if (xbit_test(upslog_flags, UPSLOG_STDERR_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_STDERR);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_SYSLOG);

	vupslog(LOG_ERR, fmt, va, use_strerror);
}

void fatal(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfatal(fmt, va, 1);
	va_end(va);

	exit(EXIT_FAILURE);
}

void fatalx(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfatal(fmt, va, 0);
	va_end(va);

	exit(EXIT_FAILURE);
}

static const char *oom_msg = "Out of memory";

void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (p == NULL)
		fatal("%s", oom_msg);
	return p;
}

void *xcalloc(size_t number, size_t size)
{
	void *p = calloc(number, size);

	if (p == NULL)
		fatal("%s", oom_msg);
	return p;
}

void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	if (p == NULL)
		fatal("%s", oom_msg);
	return p;
}

char *xstrdup(const char *string)
{
	char *p = strdup(string);

	if (p == NULL)
		fatal("%s", oom_msg);
	return p;
}

/* modify in - strip all trailing instances of <sep> */
void rtrim(char *in, char sep)
{
	char	*p = NULL;

	p = &in[strlen(in) - 1];

	while (p >= in) {
		if (*p != sep)
			return;

		*p-- = '\0';
	}
}
