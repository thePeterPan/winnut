#ifndef NUT_SYSLOG_H
#define NUT_SYSLOG_H
/*
   Copyright (C) 1998  Russell Kroll <rkroll@exploits.org>

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.              
 */

#define LOG_ALERT       2       /* action must be taken immediately */
#define LOG_CRIT        4       /* critical conditions */
#define LOG_ERR         8       /* error conditions */
#define LOG_WARNING     16      /* warning conditions */
#define LOG_NOTICE      32      /* normal but significant condition */
#define LOG_INFO        64      /* informational */
#define LOG_DEBUG       128     /* debug-level messages */

/* nice little hack to make they're logging use our logger */
#define upslogx syslog

void syslog(int loglevel, char *format, ...);
void debug(char *msg, ...);
char * strerrlevel(int errorlevel);
void closesyslog();
void setupsyslog(char *filename);

#endif