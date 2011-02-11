#ifndef NUT_WINUPSCOMMON_H
#define NUT_WINUPSCOMMON_H

/* 
   Copyright (C) 2001 Andrew Delpha (delpha@computer.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, WRITE to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.              
*/

/*
	This declares the prototypes for the functions in winupscommon.c and the
	struct used to hold all the parameters
*/

/* OS Level constants */
#define OSLEVEL_UNKNOWN (-1)	/* default and initializer - shouldn't ever really need this */
#define OSLEVEL_BASE_WIN9X	(1)		/* used to define start of win9x versions */
#define OSLEVEL_WIN95	(1)		/* Windows 95, 95 osr1, 95 osr 2 */
#define OSLEVEL_WIN98	(2)		/* Windows 98, 98SE */
#define OSLEVEL_WINME	(3)		/* Windows ME */
#define OSLEVEL_BASE_NT (64)	/* used to define start of NT versions */
#define OSLEVEL_WINNT	(64)	/* Windows NT 4 */
#define OSLEVEL_WIN2K	(65)	/* Windows 2000 */	
#define OSLEVEL_WINXP	(66)	/* Windows XP */	
#define OSLEVEL_WIN2K3	(67)	/* Windows 2003 Server */	
#define OSLEVEL_WINVISTA (68)	/* Windows Vista */	
#define OSLEVEL_WIN7	(69)	/* Windows 7 */	


/* Registry key constants */
#define UPSMON_APPLICATION_NAME "WinNutUPSMon"
#define UPSMON_APPLICATION_DISPLAY_NAME "WinNut UPS Monitor"
#define REG_UPSMON_BASE_KEY "Software\\WinNUT\\upsmon"
#define REG_UPSMON_CONF_FILE "ConfigurationFilePath"		/* The fully specified path of the conf file (including file name) */
#define REG_UPSMON_LOG_FILE "LogFilePath"					/* The fully specified path of the log file (including file name) */
#define REG_UPSMON_LOG_MASK "LogMask"						/* DWORD mask that match the levels desired to be in the logs */
#define REG_UPSMON_EXE_FILE "ExeFilePath"					/* The fully specified path of the executable file (including file name) */
#define REG_UPSMON_UPSD_PORT "Port"						    /* DWORD port number upsd is running on */
#define REG_UPSMON_UPSD_SHUTDOWNTYPE "ShutdownType"			/* DWORD bitmask of the shutdown options to use in the ExitWindowsEx function - note NT types will interpret this for the InitiateSystemShutdown call*/
#define REG_UPSMON_UPSD_SHUTDOWNAFTERSECONDS "ShutdownAfterSeconds" /* DWORD integer number of seconds to wait after ups goes on batt before shutting down: -1 means wait for the ups to go critical or FSD*/
#define REG_UPSMON_SERVICE_NAME "WinNutUPSMon"
#define REG_UPSMON_SERVICE_BASE_KEY "SYSTEM\\CurrentControlSet\\Services\\WinNutUPSMon"  //base key for the service info
#define REG_UPSMON_SERVICE_DESCRIPTION "Description"
#define UPSMON_SERVICE_DESCRIPTION "Talks to a Network UPS Tools (NUT) server on the network for UPS related shutdowns and alerts"
#define REG_UPSMON_SERVICE_DISPLAY_NAME "DisplayName"
#define UPSMON_SERVICE_DISPLAY_NAME "WinNut UPS Monitor"
#define REG_UPSMON_SERVICE_ERROR_CONTROL "ErrorControl"
#define UPSMON_SERVICE_ERROR_CONTROL (1)
#define REG_UPSMON_SERVICE_IMAGE_PATH "ImagePath"
#define REG_UPSMON_SERVICE_OBJECT_NAME "ObjectName"
#define UPSMON_SERVICE_OBJECT_NAME "LocalSystem"
#define REG_UPSMON_SERVICE_START "Start"
#define REG_UPSMON_SERVICE_TYPE "Type"
#define UPSMON_SERVICE_TYPE (272)
#define REG_UPSMON_WIN9X_RUNSERVICES "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices"
#define REG_UPSMON_WIN9X_STARTUP_KEY "WinNutUPSMon"
#define REG_UPSMON_EVENTSOURCE_KEY "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\WinNutUPSMon"
#define REG_UPSMON_EVENTSOURCE_EVENTMESSAGEFILE "EventMessageFile"
#define REG_UPSMON_EVENTSOURCE_CATEGORYMESSAGEFILE "CategoryMessageFile"
#define REG_UPSMON_EVENTSOURCE_PARAMETERMESSAGEFILE "ParameterMessageFile"
#define REG_UPSMON_EVENTSOURCE_TYPESSUPPORTED "TypesSupported"
#define UPSMON_EVENTSOURCE_TYPESSUPPORTED (7)
#define RUN_AS_NT_SERVICE (1)
#define RUN_AS_APP (0)

/* shutdown types */
#define SHUTDOWN_TYPE_NORMAL		(0x00000001)
#define SHUTDOWN_TYPE_FORCE			(0x00000004)  /* value compatible with EWX_FORCE used in previous versions */
#define SHUTDOWN_TYPE_FORCEIFHUNG	(0x00000010)  /* value compatible with EWX_FORCEIFHUNG used in previous versions */
#define SHUTDOWN_TYPE_HIBERNATE     (0x00000100)  /* new value for hibernating the system */
/* constants for getWinNUTStatus */
#define RUNNING_SERVICE_MODE (1)
#define RUNNING_APP_MODE (2)

#define MAXPATHLEN (1023)

#ifndef WinNUTConfStruct
#define WinNUTConfStruct
// Structure used to pass the configuration parameters
typedef struct WinNUTConfStruct
{
	char execPath[MAXPATHLEN+1];
	char confPath[MAXPATHLEN+1];
	char logPath[MAXPATHLEN+1];
	int logLevel;
	BOOL runAsService;
	BOOL win9XAutoStart;
	int serviceStartup;
	int upsdPort;

	int oslevel;
	char curAppPath[MAXPATHLEN+1];
	int shutdownType;
	int shutdownAfterSeconds;	/*Shut machine down after x many seconds or LOWBATT/FSD whichever comes first. -1 means wait for LOWBATT or FSD */



} WinNutConf;
#endif

void loadSettingsFromRegistry(WinNutConf *curConfig);
DWORD saveSettingsToRegistry(WinNutConf *curConfig);
int getOSLevel();
void initializeConfig(WinNutConf *curConfig);
DWORD get9xShutdownMask(WinNutConf *curConfig);
/* DO NOT MODIFY RETURNED STRINGS */
char *getNameForOSLevel(int oslevel);

DWORD getWinNUTStatus(WinNutConf *curConfig);
BOOL startWinNUT(WinNutConf *curConfig);
BOOL stopWinNUT(WinNutConf *curConfig);

/* having some trouble getting with my system settings seeing this in winuser.h */
#ifndef EWX_FORCEIFHUNG
#define EWX_FORCEIFHUNG     0x00000010
#endif

/* Buffer sizes used for various functions */
#define SMALLBUF        512
#define LARGEBUF        1024


#endif
