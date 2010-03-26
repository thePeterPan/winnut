#ifndef NUT_WINNUTMAIN_H
#define NUT_WINNUTMAIN_H

/* WinNUTMain.h

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

/* This is the definition for the structure used to pass the arguements into the 
   thread that calls the network code so we can time it out if it doesn't return
   in a reasonable amount of time
*/
typedef struct upscli_getvarThreadArgsStruct
{
	utype *ups;
	char *varname;
	char *buf;
	int buflen;
	HANDLE hTimeoutEvent;
	int retval;
	int xerrno;
	int upserror;
} upscli_getvarThreadArgs;

#define UPSCLI_GETVAR_EVENT_NAME	"upscli_getvarEventName"
#define NET_TIMEOUT_LEN	(10) /* time out length in seconds */

/* Define event return codes to handle in monitor */
#define EVENT_NONE	(0)
#define EVENT_STOP	(1)

struct EventQueueNodeStruct
{
	DWORD eventId;
	struct EventQueueNodeStruct *next;
};
typedef struct EventQueueNodeStruct EventQueueNode;

#define STOP_MONITOR_EVENT_NAME "WinNUTMonitorStopEvent"
#define EVENT_QUEUE_MUTEX_NAME "NTServiceEventQueueMutex"
#define WINNUTUPSMON_APPLICATION_MUTEX "WinNUTApplicatonMutex"
#define NO_EVENT (0)

/*
	Functions that need to be made available to the outside
 */
void createNTEventLogEntry(DWORD eventType, char *message,...);
/* these message ID's are the ones used with createNTEventLogEntry */
#include "WinNUTMessages.h"

int handleEvents();
void doWinShutdown(int delayTime);
DWORD WINAPI upscli_getvarThread(LPVOID lpvArgs);





#endif