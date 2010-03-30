/* syslog.c

   upsmon - monitor power status over the 'net (talks to upsd via TCP/UDP)

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

#include "common.h"
#include "syslog.h"
#include "../winupscommon.h"

/* Log file pointer */
FILE *s_syslogFile = NULL;
/* Log Mutex Name */
char s_mutexName[] = "WinNUTLogMutex";
HANDLE s_hLogMutex = NULL;

/* get the external sysconfig */
extern WinNutConf s_curConfig;

/*
Log an entry to the syslog file
*/

void syslog(int loglevel, char *format, ...)
{
	struct tm *curTime;
	time_t theTime;
	char tmpbuf[1024]="";
	va_list	args;
	if(s_hLogMutex != NULL)
	{
		/* get exclusive access to write to the log */
		while(WaitForSingleObjectEx(s_hLogMutex,INFINITE,FALSE) != WAIT_OBJECT_0);
		if(   (s_syslogFile != NULL)
		   && (loglevel & s_curConfig.logLevel))
		{
			theTime=time(NULL);
			curTime=localtime(&theTime);
			fprintf(s_syslogFile,"Level %8s\t",strerrlevel(loglevel));
			fprintf(s_syslogFile,"%02d/%02d/%04d %02d:%02d:%02d\t",
				curTime->tm_mon+1, curTime->tm_mday, curTime->tm_year+1900,
				curTime->tm_hour, curTime->tm_min, curTime->tm_sec);

			va_start (args, format);
			vfprintf(s_syslogFile, format, args);
			va_end (args);

			fprintf(s_syslogFile,"\n");
			fflush(s_syslogFile);
		}
		ReleaseMutex(s_hLogMutex);
	}
	return;
}

/*
    This allows compatablity with nut debug method but logs to our log file instead of stdout since we don't
	have a stdout.
 */
void debug(char *format, ...)
{
	int loglevel = LOG_DEBUG;  //TODO: are these useful and sparse enough to put in INFO?
	struct tm *curTime;
	time_t theTime;
	char tmpbuf[1024]="";
	va_list	args;
	if(s_hLogMutex != NULL)
	{
		/* get exclusive access to write to the log */
		while(WaitForSingleObjectEx(s_hLogMutex,INFINITE,FALSE) != WAIT_OBJECT_0);
		if(   (s_syslogFile != NULL)
		   && (loglevel & s_curConfig.logLevel))
		{
			theTime=time(NULL);
			curTime=localtime(&theTime);
			fprintf(s_syslogFile,"Level %8s\t",strerrlevel(loglevel));
			fprintf(s_syslogFile,"%02d/%02d/%04d %02d:%02d:%02d\t",
				curTime->tm_mon+1, curTime->tm_mday, curTime->tm_year+1900,
				curTime->tm_hour, curTime->tm_min, curTime->tm_sec);

			va_start (args, format);
			vfprintf(s_syslogFile, format, args);
			va_end (args);

			fprintf(s_syslogFile,"\n");
			fflush(s_syslogFile);
		}
		ReleaseMutex(s_hLogMutex);
	}
	return;
}


/**
 * Close the system log file
 **/
void closesyslog()
{
	if(s_hLogMutex != NULL)
	{
		/* acquire log access */
		while(WaitForSingleObjectEx(s_hLogMutex,INFINITE,FALSE) != WAIT_OBJECT_0);
		if(s_syslogFile != NULL)
		{
			fclose(s_syslogFile);
			s_syslogFile = NULL;
		}
		ReleaseMutex(s_hLogMutex);
	}
}

/**
 *  Setup the system log to output to a given file
 **/
void setupsyslog(char *filename)
{
	if(s_hLogMutex == NULL)
	{
		HANDLE tmphdl = CreateMutex(NULL,FALSE,s_mutexName);
		if(GetLastError() != ERROR_ALREADY_EXISTS)
			s_hLogMutex = tmphdl;
		else if(s_hLogMutex == NULL)
			s_hLogMutex = tmphdl;
	}
	closesyslog();
	/* acquire log access */
	while(WaitForSingleObjectEx(s_hLogMutex,INFINITE,FALSE) != WAIT_OBJECT_0);
	if(   (filename != NULL)
	   && (strlen(filename) != 0))
	{
		s_syslogFile = fopen(filename,"a+");
	}
	ReleaseMutex(s_hLogMutex);
}

/* 
  This function returns a pointer to a string name for an integer errorlevel
  DO NOT MODIFY RETURNED STRINGS 
*/
char *strerrlevel(int errorlevel)
{
	static char logAlert[]="ALERT";
	static char logCrit[]="CRITICAL";
	static char logErr[]="ERROR";
	static char logWarning[]="WARNING";
	static char logNotice[]="NOTICE";
	static char logInfo[]="INFO";
	static char logDebug[]="DEBUG";
	static char logUnknown[]="UNKNOWN";

	if(errorlevel & LOG_ALERT)
	{
		return logAlert;
	}
	else if(errorlevel &LOG_CRIT)
	{
		return logCrit;
	}
	else if(errorlevel &LOG_ERR)
	{
		return logErr;
	}
	else if(errorlevel &LOG_WARNING)
	{
		return logWarning;
	}
	else if(errorlevel &LOG_NOTICE)
	{
		return logNotice;
	}
	else if(errorlevel &LOG_INFO)
	{
		return logInfo;
	}
	else if(errorlevel &LOG_DEBUG)
	{
		return logDebug;
	}
	else
	{
		return logUnknown;
	}
}


