/* 

   Copyright (C) 2001

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

#include "stdio.h"
#include "windows.h"
#include "winsvc.h"
#include "winupscommon.h"

/*
	This file contains all the code to work with the registry part of the configuration of winnut
	This includes initializing, loading, and saving the config
*/

/*
	This function loads all of the settings for the current operating system into the config
	structure
*/
void  loadSettingsFromRegistry(WinNutConf *curConfig)
{
	int errbuf = ERROR_SUCCESS;
	char tmpbuf[MAXPATHLEN];
	DWORD buflen= 0;
	DWORD regType = REG_SZ;
	DWORD retval = 0;
	HKEY hkUpsMon;
	HKEY hkUpsMonService;
	HKEY hkUpsMonWin9XStartup;

	//load the os level
	curConfig->oslevel = getOSLevel();

	retval = RegOpenKeyEx(HKEY_LOCAL_MACHINE,REG_UPSMON_BASE_KEY,0,KEY_READ,&hkUpsMon);
	if(retval == ERROR_SUCCESS)
	{
		/* load the log file name */
		buflen = MAXPATHLEN;
		regType = REG_SZ;
		errbuf = RegQueryValueEx(hkUpsMon,REG_UPSMON_LOG_FILE,0,&regType,(LPBYTE)curConfig->logPath,&buflen);
		if(errbuf != ERROR_SUCCESS)
		{
			curConfig->logPath[0] = '\0';
		}

		/* Load the log mask */
		regType = REG_DWORD;
		buflen = sizeof(curConfig->logLevel);
		errbuf = RegQueryValueEx(hkUpsMon,REG_UPSMON_LOG_MASK,0,&regType,(LPBYTE)&(curConfig->logLevel),&buflen);
		if(errbuf != ERROR_SUCCESS)
		{
			curConfig->logLevel=15; //use default log level
		}

		/* Load the configuration file name */
		buflen = MAXPATHLEN;
		regType = REG_SZ;
		errbuf = RegQueryValueEx(hkUpsMon,REG_UPSMON_CONF_FILE,0,&regType,(LPBYTE)curConfig->confPath,&buflen);
		if(errbuf != ERROR_SUCCESS)
		{
			curConfig->confPath[0] = '\0';
		}

		
		/* Load the Executable file name */
		buflen = MAXPATHLEN;
		regType = REG_SZ;
		errbuf = RegQueryValueEx(hkUpsMon,REG_UPSMON_EXE_FILE,0,&regType,(LPBYTE)curConfig->execPath,&buflen);
		if(errbuf != ERROR_SUCCESS)
		{
			curConfig->execPath[0] = (char)NULL;
		}

		/* Load the upsd port number*/
		regType = REG_DWORD;
		buflen = sizeof(curConfig->upsdPort);
		errbuf = RegQueryValueEx(hkUpsMon,REG_UPSMON_UPSD_PORT,0,&regType,(LPBYTE)&(curConfig->upsdPort),&buflen);
		if(errbuf != ERROR_SUCCESS)
		{
			curConfig->upsdPort=3493; //use default port number
		}

		/* Load the shutdown after seconds */
		regType = REG_DWORD;
		buflen = sizeof(curConfig->shutdownAfterSeconds);
		errbuf = RegQueryValueEx(hkUpsMon,REG_UPSMON_UPSD_SHUTDOWNAFTERSECONDS,0,&regType,(LPBYTE)&(curConfig->shutdownAfterSeconds),&buflen);
		if(errbuf != ERROR_SUCCESS)
		{
			curConfig->shutdownAfterSeconds=-1; //default to not using shutdown delay
		}

		/* Load the shutdown type*/
		regType = REG_DWORD;
		buflen = sizeof(curConfig->shutdownType);
		errbuf = RegQueryValueEx(hkUpsMon,REG_UPSMON_UPSD_SHUTDOWNTYPE,0,&regType,(LPBYTE)&(curConfig->shutdownType),&buflen);
		if(errbuf != ERROR_SUCCESS)
		{
			curConfig->shutdownType=SHUTDOWN_TYPE_FORCE; //default to forced shutdown
		}

		RegCloseKey(hkUpsMon);
	}

	if(curConfig->oslevel >= OSLEVEL_BASE_NT)
	{
		/* load service info 
		   is install as service */
		
		retval = RegOpenKeyEx(HKEY_LOCAL_MACHINE,REG_UPSMON_SERVICE_BASE_KEY,0,KEY_READ,&hkUpsMonService);
		if(retval == ERROR_SUCCESS)
		{
			curConfig->runAsService = TRUE;

			/* load service startup */
			regType = REG_DWORD;
			buflen = sizeof(curConfig->serviceStartup);
			errbuf = RegQueryValueEx(hkUpsMonService,REG_UPSMON_SERVICE_START,0,&regType,(LPBYTE)&(curConfig->serviceStartup),&buflen);

			if(errbuf != ERROR_SUCCESS)
			{
				curConfig->serviceStartup=3; //use manual as default level
			}		
		}
		else
		{
			curConfig->runAsService = FALSE;
		}
		curConfig->win9XAutoStart = FALSE;
		RegCloseKey(hkUpsMonService);
	}
	else if(   (curConfig->oslevel >= OSLEVEL_BASE_WIN9X)
			&& (curConfig->oslevel <  OSLEVEL_BASE_NT))
	{
		curConfig->runAsService = FALSE;
		curConfig->serviceStartup = 0;

		retval = RegOpenKeyEx(HKEY_LOCAL_MACHINE,REG_UPSMON_WIN9X_RUNSERVICES,0,KEY_READ,&hkUpsMonWin9XStartup);
		/* Try loading the win9x Service*/
		buflen = MAXPATHLEN;
		regType = REG_SZ;
		errbuf = RegQueryValueEx(hkUpsMonWin9XStartup,REG_UPSMON_WIN9X_STARTUP_KEY,0,&regType,(LPBYTE)tmpbuf,&buflen);
		if(errbuf == ERROR_SUCCESS)
		{
			curConfig->win9XAutoStart = TRUE;
		}
		else
		{
			curConfig->win9XAutoStart = FALSE;
		}
		RegCloseKey(hkUpsMonWin9XStartup);
	}
}

/*
	This saves all the config settings to the registry as appropriate for the current OS as
	defined in the curConfig object - IE: the curConfig->oslevel SHOULD NEVER be changed from
	level discovered by getOSLevel.
*/
DWORD saveSettingsToRegistry(WinNutConf *curConfig)
{
	DWORD regType = REG_SZ;
	HKEY hkUpsMon;
	HKEY hkUpsMonService;
	HKEY hkUpsMonEventLog;
	HKEY hkUpsMonWin9XStartup;
	long retval = 0;
	/*FILETIME tmpft;*/
	DWORD lastError = 0;

	retval = RegCreateKey(HKEY_LOCAL_MACHINE,REG_UPSMON_BASE_KEY,&hkUpsMon);
	if(retval == ERROR_SUCCESS)
	{
		regType = REG_SZ;
		retval = RegSetValueEx(hkUpsMon,REG_UPSMON_LOG_FILE,0,regType,(CONST BYTE *)curConfig->logPath,strlen(curConfig->logPath)+1);
		retval = RegSetValueEx(hkUpsMon,REG_UPSMON_EXE_FILE,0,regType,(CONST BYTE *)curConfig->execPath,strlen(curConfig->execPath)+1);
		retval = RegSetValueEx(hkUpsMon,REG_UPSMON_CONF_FILE,0,regType,(CONST BYTE *)curConfig->confPath,strlen(curConfig->confPath)+1);

		regType = REG_DWORD;
		retval  = RegSetValueEx(hkUpsMon,REG_UPSMON_LOG_MASK,0,regType,(CONST BYTE *)&curConfig->logLevel,sizeof(curConfig->logLevel));
		retval  = RegSetValueEx(hkUpsMon,REG_UPSMON_UPSD_PORT,0,regType,(CONST BYTE *)&curConfig->upsdPort,sizeof(curConfig->upsdPort));
		retval  = RegSetValueEx(hkUpsMon,REG_UPSMON_UPSD_SHUTDOWNAFTERSECONDS,0,regType,(CONST BYTE *)&curConfig->shutdownAfterSeconds,sizeof(curConfig->shutdownAfterSeconds));
		retval  = RegSetValueEx(hkUpsMon,REG_UPSMON_UPSD_SHUTDOWNTYPE,0,regType,(CONST BYTE *)&curConfig->shutdownType,sizeof(curConfig->shutdownType));

		RegCloseKey(hkUpsMon);
	}

	if(curConfig->oslevel >= OSLEVEL_WINNT)
	{
		/* now add us to the Event Source Registry since we use the NT Event Application log */
		retval = RegCreateKey(HKEY_LOCAL_MACHINE,REG_UPSMON_EVENTSOURCE_KEY,&hkUpsMonEventLog);
		if(retval == ERROR_SUCCESS)
		{
			DWORD tmpVal;
			regType = REG_SZ;
			retval = RegSetValueEx(hkUpsMonEventLog,REG_UPSMON_EVENTSOURCE_EVENTMESSAGEFILE,0,regType,(CONST BYTE *)curConfig->execPath,strlen(curConfig->execPath)+1);
			retval = RegSetValueEx(hkUpsMonEventLog,REG_UPSMON_EVENTSOURCE_CATEGORYMESSAGEFILE,0,regType,(CONST BYTE *)curConfig->execPath,strlen(curConfig->execPath)+1);
			retval = RegSetValueEx(hkUpsMonEventLog,REG_UPSMON_EVENTSOURCE_PARAMETERMESSAGEFILE,0,regType,(CONST BYTE *)curConfig->execPath,strlen(curConfig->execPath)+1);

			regType = REG_DWORD;
			tmpVal = UPSMON_EVENTSOURCE_TYPESSUPPORTED;
			retval = RegSetValueEx(hkUpsMonEventLog,REG_UPSMON_EVENTSOURCE_TYPESSUPPORTED,0,regType,(CONST BYTE *)&tmpVal,sizeof(tmpVal));

			RegCloseKey(hkUpsMonEventLog);
		}

		if(curConfig->runAsService == TRUE)
		{
			//create / update the NT service
			SC_HANDLE hServiceManager = NULL;
			SC_HANDLE hWinNUTService = NULL;
			
			hServiceManager = OpenSCManager(NULL,NULL,GENERIC_READ|GENERIC_WRITE);
			if(hServiceManager)
			{
				hWinNUTService = OpenService(hServiceManager,"WinNutUPSMon",SERVICE_CHANGE_CONFIG|SERVICE_QUERY_CONFIG);
				if(hWinNUTService)
				{
					ChangeServiceConfig(hWinNUTService,
									SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS ,
									curConfig->serviceStartup,
									SERVICE_ERROR_NORMAL,
									curConfig->execPath,
									"",
									NULL,
									"",
									NULL,
									NULL,
									UPSMON_SERVICE_DISPLAY_NAME);
					lastError = GetLastError();
					CloseServiceHandle(hWinNUTService);
				}
				else
				{
					hWinNUTService = CreateService(hServiceManager,
						REG_UPSMON_SERVICE_NAME,
						UPSMON_SERVICE_DISPLAY_NAME,
						SERVICE_CHANGE_CONFIG|SERVICE_QUERY_CONFIG|GENERIC_WRITE,
						SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS ,
						curConfig->serviceStartup,
						SERVICE_ERROR_NORMAL,
						curConfig->execPath,
						"",
						NULL,
						"",
						NULL,
						NULL);
					lastError = GetLastError();
					CloseServiceHandle(hWinNUTService);
				}
				CloseServiceHandle(hServiceManager);
			}
			
			/* now update the description entry */
			retval = RegCreateKey(HKEY_LOCAL_MACHINE,REG_UPSMON_SERVICE_BASE_KEY,&hkUpsMonService);
			if(retval == ERROR_SUCCESS)
			{
				regType = REG_SZ;
				retval = RegSetValueEx(hkUpsMonService,REG_UPSMON_SERVICE_DESCRIPTION,0,regType,(CONST BYTE *)UPSMON_SERVICE_DESCRIPTION,strlen(UPSMON_SERVICE_DESCRIPTION)+1);
				RegCloseKey(hkUpsMonService);
			}
		}
		else
		{
			//remove the NT service if it exists
			SC_HANDLE hServiceManager = NULL;
			SC_HANDLE hWinNUTService = NULL;
			
			hServiceManager = OpenSCManager(NULL,NULL,GENERIC_READ|GENERIC_WRITE);
			if(hServiceManager)
			{
				hWinNUTService = OpenService(hServiceManager,"WinNutUPSMon",DELETE|SERVICE_STOP|GENERIC_READ);
				if(hWinNUTService)
				{
					/* stop the service if it's currently running */
					SERVICE_STATUS serviceStatus;
					QueryServiceStatus(hWinNUTService,&serviceStatus);
					if(   (serviceStatus.dwCurrentState != SERVICE_STOPPED)
					   && (serviceStatus.dwCurrentState != SERVICE_STOP_PENDING))
					{
						ControlService(hWinNUTService,SERVICE_CONTROL_STOP,&serviceStatus);
					}
					DeleteService(hWinNUTService);
					lastError = GetLastError();
					CloseServiceHandle(hWinNUTService);
				}
				CloseServiceHandle(hServiceManager);
			}
			/*
			retval = RegOpenKeyEx(HKEY_LOCAL_MACHINE,REG_UPSMON_SERVICE_BASE_KEY,0,KEY_ALL_ACCESS,&hkUpsMonService);
			if(retval == ERROR_SUCCESS)
			{
				// mildly annoying - when the service is started, a subkey can be created which blocks NT from deleting the key directly (can't delete a key with children keys)
				// Shortcoming of this code - we don't handle multiple nesting of these subkeys
				buflen = 1024;
				buflen2 = 1024;
				retval = RegEnumKeyEx(hkUpsMonService,0,tmpbuf,&buflen,0,tmpbuf2,&buflen2,&tmpft);
				while(retval == ERROR_SUCCESS) //end case is a return ERROR_NO_MORE_ITEMS
				{
					retval = RegDeleteKey(hkUpsMonService,tmpbuf);
					buflen = 1024;
					buflen2 = 1024;
					retval = RegEnumKeyEx(hkUpsMonService,0,tmpbuf,&buflen,0,tmpbuf2,&buflen2,&tmpft);
				}
				RegCloseKey(hkUpsMonService);
				retval = RegDeleteKey(HKEY_LOCAL_MACHINE,REG_UPSMON_SERVICE_BASE_KEY);
			}
			*/
			
		}
	}
	else
	{
		if(curConfig->win9XAutoStart == TRUE)
		{
			retval = RegCreateKey(HKEY_LOCAL_MACHINE,REG_UPSMON_WIN9X_RUNSERVICES,&hkUpsMonWin9XStartup);
			if(retval == ERROR_SUCCESS)
			{
				regType = REG_SZ;
				retval = RegSetValueEx(hkUpsMonWin9XStartup,REG_UPSMON_WIN9X_STARTUP_KEY,0,regType,(CONST BYTE *)curConfig->execPath,strlen(curConfig->execPath)+1);
				RegCloseKey(hkUpsMonWin9XStartup);
			}
		}
		else
		{
			retval = RegCreateKey(HKEY_LOCAL_MACHINE,REG_UPSMON_WIN9X_RUNSERVICES,&hkUpsMonWin9XStartup);
			if(retval == ERROR_SUCCESS)
			{
				retval = RegDeleteValue(hkUpsMonWin9XStartup,REG_UPSMON_WIN9X_STARTUP_KEY);
				RegCloseKey(hkUpsMonWin9XStartup);
			}
		}
	}
	return lastError;
}

/*
	This returns an integer constant defined in winupscommon.h that defines the current os version
*/
int  getOSLevel()
{
	int oslevel = OSLEVEL_UNKNOWN;
	OSVERSIONINFO verInfo;
	ZeroMemory(&verInfo,sizeof(verInfo));
	verInfo.dwOSVersionInfoSize=sizeof(verInfo);

	if(GetVersionEx((FAR OSVERSIONINFO *) &verInfo))
	{
		if(verInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		{
			if(verInfo.dwMajorVersion <= 4)
			{
				/* This means we've got NT 3.51 or 4 */
				oslevel = OSLEVEL_WINNT;
			}
			else if(   (verInfo.dwMajorVersion == 5)
					&& (verInfo.dwMinorVersion == 0))
			{
				/* We've got Win2k */
				oslevel = OSLEVEL_WIN2K;
			}
			else if(   (verInfo.dwMajorVersion == 5)
					&& (verInfo.dwMinorVersion == 1))
			{
				/* We've got Windows XP */
				oslevel = OSLEVEL_WINXP;
			}
			else if(   (verInfo.dwMajorVersion == 5)
					&& (verInfo.dwMinorVersion == 2))
			{
				/* We've got Windows 2003 Server */
				oslevel = OSLEVEL_WIN2K3;
			}
			else if(   (verInfo.dwMajorVersion == 6)
					&& (verInfo.dwMinorVersion == 0))
			{
				/* We've got Windows Vista */
				oslevel = OSLEVEL_WINVISTA;
			}
			else if(   (verInfo.dwMajorVersion == 6)
					&& (verInfo.dwMinorVersion == 1))
			{
				/* We've got Windows 7 */
				oslevel = OSLEVEL_WIN7;
			}
			else
			{
				/* Must be some crazy new microsoft product =) default to NT so we treat it as and OS with services */
				oslevel = OSLEVEL_WINNT;
			}
		}
		else if(verInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		{
			if(   (verInfo.dwMajorVersion == 4)
			   && (verInfo.dwMinorVersion == 0))
			{
				/* win95, win95 OSR2 2.5, etc*/
				oslevel = OSLEVEL_WIN95;
			}
			else if(   (verInfo.dwMajorVersion == 4)
					&& (verInfo.dwMinorVersion == 10))
			{
				/* win98, win98 SE */
				oslevel = OSLEVEL_WIN98;
			}
			else if(   (verInfo.dwMajorVersion == 4)
					&& (verInfo.dwMinorVersion == 90))
			{
				/* This would be win ME */
				oslevel = OSLEVEL_WINME;
			}
			else
			{
				/* unknown - treat as win95 for now*/
				oslevel = OSLEVEL_WIN95;
			}
		}
		else
		{
			/* we don't know what this version is, could be Win32s (like win3.11 uggghhh)  */
			/* we don't support that so, set it to unknown */
			oslevel = OSLEVEL_UNKNOWN;
		}
	}
	return oslevel;
}


/*
	This initializes the values in the structure so that if there are no registry settings
	we have some defaults in here.
*/
void  initializeConfig(WinNutConf *curConfig)
{
	curConfig->confPath[0]='\0';
	curConfig->curAppPath[0]='\0';
	curConfig->execPath[0]='\0';
	curConfig->logLevel=14; //default to highest 3 levels
	curConfig->logPath[0]='\0';
	curConfig->oslevel=getOSLevel();
	curConfig->runAsService=FALSE;
	curConfig->serviceStartup=3; //this is manual start setting
	curConfig->upsdPort=3493; //this is the default port used by NUT 0.50.0 and later - previous versions was 3305
	curConfig->win9XAutoStart=FALSE;
	curConfig->shutdownType = EWX_FORCE;
	curConfig->shutdownAfterSeconds = -1;
}

/* DO NOT MODIFY RETURNED STRINGS */
char *getNameForOSLevel(int oslevel)
{
	static char win95[]="Windows 95";
	static char win98[]="Windows 98";
	static char winME[]="Windows ME";
	static char winNT[]="Windows NT";
	static char win2K[]="Windows 2000";
	static char winXP[]="Windows XP";
	static char win2K3[]="Windows 2003 Server";
	static char winVista[]="Windows Vista";
	static char win7[]="Windows 7";
	static char unknown[]="UNKNOWN OS VERSION";
	
	switch(oslevel)
	{
	case OSLEVEL_WIN95:
		return win95;
	case OSLEVEL_WIN98:
		return win98;
	case OSLEVEL_WINME:
		return winME;
	case OSLEVEL_WINNT:
		return winNT;
	case OSLEVEL_WIN2K:
		return win2K;
	case OSLEVEL_WINXP:
		return winXP;
	case OSLEVEL_WIN2K3:
		return win2K3;
	case OSLEVEL_WINVISTA:
		return winVista;
	case OSLEVEL_WIN7:
		return win7;
	default:
		return unknown;
	}
}

DWORD __cdecl getWinNUTStatus(WinNutConf *curConfig)
{
	HWND hWnd = NULL;
	DWORD retVal = 0;
	
	hWnd = FindWindow(UPSMON_APPLICATION_NAME,NULL);
	if(hWnd != NULL)
	{
		retVal |= RUNNING_APP_MODE;
	}
	if(curConfig->oslevel >= OSLEVEL_BASE_NT)
	{
		SERVICE_STATUS serviceStatus;
		SC_HANDLE hServiceManager = NULL;
		SC_HANDLE hWinNUTService = NULL;
		
		hServiceManager = OpenSCManager(NULL,NULL,GENERIC_READ);
		if(hServiceManager)
		{
			hWinNUTService = OpenService(hServiceManager,"WinNutUPSMon",GENERIC_READ);
			if(hWinNUTService)
			{
				QueryServiceStatus(hWinNUTService,&serviceStatus);
				if(   (serviceStatus.dwCurrentState == SERVICE_RUNNING)
				   || (serviceStatus.dwCurrentState == SERVICE_START_PENDING))
				{
					retVal |= RUNNING_SERVICE_MODE;
				}
				CloseServiceHandle(hWinNUTService);
			}
			CloseServiceHandle(hServiceManager);
		}
	}
	return retVal;
}

BOOL stopWinNUT(WinNutConf *curConfig)
{
	HWND hWnd = NULL;
	
	hWnd = FindWindow(UPSMON_APPLICATION_NAME,NULL);
	if(hWnd != NULL)
	{
		BOOL ret = PostMessage(hWnd,WM_CLOSE,0,0);
		return ret;
	}
	/* didn't find it running as an application - if under NT, try as a service */
	else if(curConfig->oslevel >= OSLEVEL_BASE_NT)
	{
		SERVICE_STATUS serviceStatus;
		SC_HANDLE hServiceManager = NULL;
		SC_HANDLE hWinNUTService = NULL;
		
		hServiceManager = OpenSCManager(NULL,NULL,GENERIC_READ);
		if(hServiceManager)
		{
			hWinNUTService = OpenService(hServiceManager,"WinNutUPSMon",SERVICE_STOP|GENERIC_READ);
			if(hWinNUTService)
			{
				BOOL ret = FALSE;
				QueryServiceStatus(hWinNUTService,&serviceStatus);
				if(   (serviceStatus.dwCurrentState != SERVICE_STOPPED)
				   && (serviceStatus.dwCurrentState != SERVICE_STOP_PENDING))
				{
					ret =  ControlService(hWinNUTService,SERVICE_CONTROL_STOP,&serviceStatus);
				}

				CloseServiceHandle(hWinNUTService);
				CloseServiceHandle(hServiceManager);
				return ret;
			}
			CloseServiceHandle(hServiceManager);
		}
	}
	return FALSE;
}

BOOL startWinNUT(WinNutConf *curConfig)
{
	if(   (curConfig->oslevel >= OSLEVEL_BASE_NT)
	   && (curConfig->runAsService == TRUE))
	{
		//Start up the NT service
		SC_HANDLE hServiceManager = NULL;
		SC_HANDLE hWinNUTService = NULL;
		
		hServiceManager = OpenSCManager(NULL,NULL,GENERIC_READ);
		if(hServiceManager)
		{
			hWinNUTService = OpenService(hServiceManager,"WinNutUPSMon",SERVICE_START);
			if(hWinNUTService)
			{
				BOOL ret = StartService(hWinNUTService,0,NULL);
				CloseServiceHandle(hWinNUTService);
				CloseServiceHandle(hServiceManager);
				return ret;
			}
			CloseServiceHandle(hServiceManager);
		}
	}
	else
	{
		//must be starting it up as an app
		char	exec[512]="";
		STARTUPINFO startInfo;
		PROCESS_INFORMATION processInfo;

		if(curConfig->execPath != NULL)
		{
			ZeroMemory(&startInfo,sizeof(startInfo));
			startInfo.cb=sizeof(startInfo);
			_snprintf (exec, sizeof(exec), "\"%s\"", curConfig->execPath);
			//system (exec);
			
			/* Trying to avoid the command window popping up when the message is issued.
			   This still happens with "net" command (ie net send), but the alertpopup tool
			   seems to not have this issue since it is not written for direct command line 
			   use.
			 */
			return CreateProcess(NULL,exec,NULL,NULL,FALSE,0,NULL,NULL,&startInfo,&processInfo);
		}
	}
	return FALSE;
}


/*
  This function returns the mask used with the 9x style shutdown functions to determine if it's a forced shutdown
  or not.
 */
DWORD get9xShutdownMask(WinNutConf *curConfig)
{
	switch(curConfig->shutdownType)
	{
	case SHUTDOWN_TYPE_NORMAL:
		return 0;
	case SHUTDOWN_TYPE_FORCE:
		return EWX_FORCE;
	case SHUTDOWN_TYPE_FORCEIFHUNG:
		return EWX_FORCEIFHUNG;
	default:
		return EWX_FORCE;
	}
}