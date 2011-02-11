/* WinNUTMain.c

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

#define WIN32BUILD
#include "common.h"
#include "upsclient.h"
#include "config.h"
#include "version.h"
#include "syslog.h"
#include "..\winupscommon.h"
#include "upsmon.h"
#include "winupsmon.h"
#include "WinNUTMain.h"

/*****************************************************************************************************************
	General Stuff
 *****************************************************************************************************************/
	/* 
		This is a handle to a windows event that gets signaled whenever we need to stop the monitor loop
		This is used in the services to handle service requests and is used in application mode when the gui
		needs to signal a close to the monitor thread.
     */
	HANDLE  s_hStopMonitorEvent = NULL;

	/* this is the flag indicating whether the windows network has been started */
	int s_networkStarted = 0;

	/* Struct for holding all config info loaded from the registry */
	WinNutConf s_curConfig;

	void StartupWindowsNetwork();
	void CleanupWindowsNetwork();


/*****************************************************************************************************************
	Stuff for Application mode
 *****************************************************************************************************************/

	/* windows handles */
	HINSTANCE s_hInstance = NULL;
    HINSTANCE s_hPrevInstance = NULL;
	HWND s_shutdownWindow = NULL;
	int s_nCmdShow = 0;

	/* This is a flag tells us when the windows message loop has shutdown */
	BOOL s_messageLoopClosed=FALSE;

	/* handle on the exitwindowsex shutdown thread */
	HANDLE s_hExitWindowsExThread = NULL;

	DWORD WINAPI MonitorLoopThread(LPVOID lpvParam);
	LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
	void RunInApplicationMode();

/*****************************************************************************************************************
	Stuff for all windows 9x
 *****************************************************************************************************************/
	int s_downtime=20;
	
	void doWin9xShutdown(int timeDelay);
	DWORD WINAPI DoExitWindowsExThread(LPVOID lpvParam);
	DWORD WINAPI CountDown(LPVOID lpvParam);

/*****************************************************************************************************************
	Stuff for all windows NT
 *****************************************************************************************************************/
	void doWinNTShutdown(int waitTime);

/*****************************************************************************************************************
	Stuff for NT Service mode
 *****************************************************************************************************************/
	SERVICE_STATUS          s_Status;       // current status of the service
	SERVICE_STATUS_HANDLE   s_hStatusHandle;

	HANDLE s_hEventQueueAccess = NULL;
	EventQueueNode *s_EventQueueHead = NULL;

	void RunInServiceMode();
	void WINAPI winNUTServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
	VOID WINAPI winNUTServiceHandler(DWORD newControlCode);
	BOOL UpdateSCMStatus(DWORD newState);
	void insertEventInQueue(newControlCode);
	DWORD getNextEvent();

/****************************************************************************************************************
 ****************************************************************************************************************
	This section of code is for functions used under all platforms
 ****************************************************************************************************************
 ****************************************************************************************************************/

	/*
		This is the main program entry point - all versions start here
	 */
	int APIENTRY WinMain(HINSTANCE hInstance,
						 HINSTANCE hPrevInstance,
						 LPSTR     lpCmdLine,
						 int       nCmdShow)
	{
		char tmpbuf[1024]="";
		char *tmp = NULL;
		HANDLE hAppMutex = NULL;
		s_hInstance = hInstance;
		s_hPrevInstance = hPrevInstance;

		initializeConfig(&s_curConfig);
		loadSettingsFromRegistry(&s_curConfig);

		/* 
			read command line parameters 
			only one we're currently looking for is
			-c stop
			This will kill winnutupsmon running in application mode
		*/
		if(   (lpCmdLine != NULL)
		   && (strlen(lpCmdLine) > 0))
		{
			strncpy(tmpbuf,lpCmdLine,sizeof(tmpbuf));
			tmp = strtok(tmpbuf," ");
			while(tmp)
			{
				if(   (tmp[0] == '-')
				   && (tmp[1] == 'c' || tmp[1] == 'C')
				   && (tmp[2] == '\0'))
				{
					tmp = strtok(NULL," ");
					if(   (tmp != NULL)
					   && (tmp[0] == 's' || tmp[0] == 'S')
					   && (tmp[1] == 't' || tmp[1] == 'T')
					   && (tmp[2] == 'o' || tmp[2] == 'O')
					   && (tmp[3] == 'p' || tmp[3] == 'P')
					   && (tmp[4] == '\0'))
					{
						/* stop all running instances of WinNUT */
						while(stopWinNUT(&s_curConfig));
						return NO_ERROR;
					}
					else if(   (tmp != NULL)
					   && (tmp[0] == 's' || tmp[0] == 'S')
					   && (tmp[1] == 't' || tmp[1] == 'T')
					   && (tmp[2] == 'a' || tmp[2] == 'A')
					   && (tmp[3] == 'r' || tmp[3] == 'R')
					   && (tmp[4] == 't' || tmp[4] == 'T')
					   && (tmp[5] == '\0'))
					{
						/* stop all other running instances of WinNUT */
						while(stopWinNUT(&s_curConfig));
						if(   (s_curConfig.runAsService == TRUE)
						   && (s_curConfig.oslevel >= OSLEVEL_BASE_NT))
						{
							/* need to start WinNUT as a service - the common start function does this */
							startWinNUT(&s_curConfig);
							return NO_ERROR;
						}
						else
						{
							/* in application mode - all other instances have been stopped, so just 
							   continue on normally
							 */
							break;
						}
					}

					break;
				}
				tmp = strtok(NULL," ");
			}
		}

		/* 
			Attempt to gain control of the application mutex, if we can't, then someone else is running it.
			So we exit - this prevents multiple copies from executing the monitor at the same time
		*/
		hAppMutex = CreateMutex(NULL,FALSE,WINNUTUPSMON_APPLICATION_MUTEX);
		if(WaitForSingleObjectEx(hAppMutex,5000,FALSE) != WAIT_OBJECT_0)
		{
			return ERROR_SERVICE_ALREADY_RUNNING;
		}

		setupsyslog(s_curConfig.logPath);
		if(   (s_curConfig.confPath == NULL)
		   || (strlen(s_curConfig.confPath) == 0))
		{
			/* If we don't have a config file, we're screwed - display error message and exit now */
			MessageBox(NULL,"ERROR: Could not load registry information for configuration file location\nPlease use the WinNUT Configuration Tool to set the location of your configuration file","WinNUT UPSMon Error",MB_OK);
			exit(REG_ERROR_CONF_FILE);
		}

		syslog(LOG_DEBUG,"WinUPSMon starting (through WinMain)");

		if(s_curConfig.oslevel == OSLEVEL_UNKNOWN)
		{
			syslog(LOG_ERR,"ABORTING! Unknown Operating system - currently supported: Win NT4, Win 2000, WinXP, Win 9x, WinME!");
			closesyslog();
			MessageBox(NULL,"ERROR: Could not determine operating system version","WinNUT UPSMon Error",MB_OK);
			exit(-1);
		}

		syslog(LOG_INFO,"Detected OS as %s",getNameForOSLevel(s_curConfig.oslevel));

		StartupWindowsNetwork();

		/* create the stop monitor event */
		s_hStopMonitorEvent = CreateEvent(NULL,TRUE,FALSE,STOP_MONITOR_EVENT_NAME);

		if(   (s_curConfig.oslevel >= OSLEVEL_BASE_NT)
		   && (s_curConfig.runAsService == TRUE))
		{
			RunInServiceMode();
		}
		else
		{
			RunInApplicationMode();
		}

		CleanupWindowsNetwork();
		ReleaseMutex(hAppMutex);
		CloseHandle(hAppMutex);

		if(s_hExitWindowsExThread != NULL)
		{
			/* wait for shutdown thread to finish - not sure if it ever does, but we've closed out everything else 
				We have to wait for it, otherwise it seems the shutdown gets screwed up...  Well documented "features"
				of ExitWindowsEx
			*/
			WaitForSingleObjectEx(s_hExitWindowsExThread,INFINITE,FALSE);
			CloseHandle(s_hExitWindowsExThread);
		}

		if(s_hStopMonitorEvent)
			CloseHandle(s_hStopMonitorEvent);
		closesyslog();
		/* Just to make sure we really close down */
		ExitProcess(0);
		return 0;
	}


	void doWinShutdown(int delayTime)
	{
		switch(s_curConfig.oslevel)
		{
		case OSLEVEL_WINNT:
		case OSLEVEL_WIN2K:
		case OSLEVEL_WINXP:
		case OSLEVEL_WIN2K3:
		case OSLEVEL_WINVISTA:
		case OSLEVEL_WIN7:
				/* WinNT/2k */
			doWinNTShutdown(delayTime);
			break;
		case OSLEVEL_WIN95:
		case OSLEVEL_WIN98:
		case OSLEVEL_WINME:
			doWin9xShutdown(delayTime);
			break;
		default:
			syslog(LOG_CRIT,"ERROR: hit the default case in the shutdown where I should have used switched to an OS specific shutdown code.  This is bad...");
		}
	}

	void StartupWindowsNetwork()
	{
		WORD versionRequested;
		WSADATA wsaData;

		if(s_networkStarted == 0)
		{
			s_networkStarted = 1;
			versionRequested=MAKEWORD( 2, 2 );
			WSAStartup(versionRequested,&wsaData);

		}
	}

	void CleanupWindowsNetwork()
	{
		if(s_networkStarted != 0)
		{
			s_networkStarted = 0;
			WSACleanup();
		}
	}

	int handleEvents()
	{
		if(WAIT_OBJECT_0 == WaitForSingleObject(s_hStopMonitorEvent,100))
		{
			ResetEvent(s_hStopMonitorEvent);
			return EVENT_STOP;
		}
		return EVENT_NONE;
	}

	/*
		This function does the call to getupsvarfd via a thread so we can have a timeout event
		if the call finishes, we signal the wait event so the parent thread knows we're done
	*/
	DWORD WINAPI upscli_getvarThread(LPVOID lpvArgs)
	{
		upscli_getvarThreadArgs *args = (upscli_getvarThreadArgs*) lpvArgs;
		syslog(LOG_DEBUG,"upscli_getvarThread - calling upscli_getvar");
		args->retval = get_var(args->ups, args->varname, args->buf, args->buflen);
		args->xerrno = errno;
		syslog(LOG_DEBUG,"upscli_getvarThread - signaling event");
		SetEvent(args->hTimeoutEvent);
		ExitThread( args->retval);
		return args->retval;
	}


/****************************************************************************************************************
 ****************************************************************************************************************
	This section of code is for all functions used when running in application mode
 ****************************************************************************************************************
 ****************************************************************************************************************/

	void RunInApplicationMode()
	{
		WNDCLASS wc;
		DWORD monitorLoopThreadID;
		HANDLE hMonitorLoop = NULL;
		MSG msg;
		int ret = 0;

		/* We're running as an application - set this process as a service so we don't get killed with logouts 
			under win9x - don't think we can do this under NT
		*/
		if(   (s_curConfig.oslevel >= OSLEVEL_BASE_WIN9X)
		   && (s_curConfig.oslevel < OSLEVEL_BASE_NT))
		{
			HMODULE krnl32 = NULL;
			FARPROC RegisterServiceProcess = NULL;
			krnl32 = GetModuleHandle("KERNEL32.DLL");
			if(krnl32 == NULL)
			{
				syslog(LOG_WARNING,"Could not register as a service process - couldn't get handle on KERNEL32.DLL");
			}
			else
			{
				RegisterServiceProcess = GetProcAddress(krnl32,"RegisterServiceProcess");
				if(RegisterServiceProcess == NULL)
				{
					syslog(LOG_WARNING,"Could not register as a service process - couldn't get handle to RegisterServiceProcess in KERNEL32.DLL");
				}
				else
				{
					if(RegisterServiceProcess(NULL,1) == 0)
					{
						syslog(LOG_WARNING,"Could not register as a service process - an error occured while registering as a service process");
					}
					else
					{
						syslog(LOG_INFO,"WinNutUpsMon successfully registered as a service process");
					}
				}
			}
		}

		/* do stuff to register the application */
		if( !s_hPrevInstance )
		{
		   wc.lpszClassName = UPSMON_APPLICATION_NAME;
		   wc.lpfnWndProc = MainWndProc;
		   wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
		   wc.hInstance = s_hInstance;
		   wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
		   wc.hCursor = LoadCursor( NULL, IDC_ARROW );
		   wc.hbrBackground = (HBRUSH) COLOR_WINDOW;
		   wc.lpszMenuName = NULL;
		   wc.cbClsExtra = 0;
		   wc.cbWndExtra = 0;

		   RegisterClass( &wc );
		}

		/* create the window so we get in the message loop */
		s_shutdownWindow = CreateWindow( UPSMON_APPLICATION_NAME,
			   UPSMON_APPLICATION_DISPLAY_NAME,
			   WS_POPUPWINDOW|WS_CAPTION,
			   200, 200, 500, 80,
			   NULL, NULL, s_hInstance, NULL);
		ShowWindow( s_shutdownWindow, s_nCmdShow );
		ShowWindow( s_shutdownWindow, SW_HIDE);

		/* kick off the monitor thread to actually monitor the ups - this thread becomes the windows message loop */
		hMonitorLoop  = CreateThread(NULL,0,MonitorLoopThread,NULL,0,&monitorLoopThreadID);

	   /* window message loop */
		while( (ret = GetMessage( &msg, s_shutdownWindow, 0, 0 )) ) 
		{
		   if(ret == -1)
			   break;
		   TranslateMessage( &msg );
		   DispatchMessage( &msg );
		}
		s_messageLoopClosed = TRUE;

		/* 
			make sure we don't kill off everything before we're finished with monitor stuff 
			monitor loop should always get notification that we're closing out, so this should
			always return shortly.
		*/
		syslog(LOG_INFO,"Waiting for monitor thread to finish...");
		SetEvent(s_hStopMonitorEvent);
		
		WaitForSingleObjectEx(hMonitorLoop,INFINITE,FALSE);
		CloseHandle(hMonitorLoop);
		syslog(LOG_INFO,"All threads clear - exiting");

	}

	/* This function runs the monitor function in a separate thread */
	DWORD WINAPI MonitorLoopThread(LPVOID lpvParam)
	{
		doMonitorPreConfig();
		syslog(LOG_DEBUG,"Monitor pre config finished... Beginning monitor loop");
		monitor();
		syslog(LOG_DEBUG,"Monitor loop finished...  Beginning to logoff all UPSes");
		logoffAllUPSs();
		syslog(LOG_DEBUG,"Logoff of all upses finished");
		
		if(s_messageLoopClosed == FALSE)
		{
			//just to make sure that we do exit the program after exiting the monitor thread
			PostMessage(s_shutdownWindow,WM_DESTROY,0,0);
		}

		syslog(LOG_DEBUG,"Message loop closed, and Monitor Loop Thread is exiting");

		ExitThread(0);
		return 0;
	}

	/**
	 * This is the windows event handler for redrawing the dialog box
	 */
	LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam,
	   LPARAM lParam )
	{
	   PAINTSTRUCT ps;
	   HDC hDC;

	   char tmpStr[1024]="";

	   switch( msg ) 
	   {
		  case WM_PAINT:
			 if(s_downtime > 0)
				 _snprintf(tmpStr,sizeof(tmpStr),"SYSTEM SHUTTING DOWN in %2d seconds!",s_downtime);
			 else
				 _snprintf(tmpStr,sizeof(tmpStr),"SYSTEM SHUTTING DOWN NOW!            ");
			 hDC = BeginPaint( hWnd, &ps );
			 ps.fErase=1;
			 SetBkMode(hDC,OPAQUE);
			 SetBkColor(hDC,GetSysColor(COLOR_3DFACE));
			 TextOut( hDC, 10, 10, "WARNING: UPS on battery", 23 );
			 TextOut( hDC, 250,30, "                                          ", 40);
			 TextOut( hDC, 10, 30, tmpStr, strlen(tmpStr));

			 EndPaint( hWnd, &ps );
			 break;
		  case WM_ENDSESSION:
			  if(   ((BOOL)wParam == TRUE)     /* is the session really ending */
				 && ((void *)lParam == NULL))  /* is the system being shutdown - vs just user logging off */
			  {
				 /* signal to the main monitor loop that we need to close out */
				 SetEvent(s_hStopMonitorEvent);
				 syslog(LOG_INFO,"In WM_ENDSESSION handler");
			  }
			  else
			  {
				  /* it's just a log out event - we don't exit on logout, so don't return 0 */
				  return 1;
			  }
		  case WM_DESTROY:
			 syslog(LOG_INFO,"In WM_DESTROY handler");
			  //signal to the main monitor loop that we need to close out
			 SetEvent(s_hStopMonitorEvent);
			 Sleep(20);
			 syslog(LOG_INFO,"In WM_DESTROY handler - leaving");
			 PostQuitMessage( 0 );
			 break;
		  default:
			 return( DefWindowProc( hWnd, msg, wParam, lParam ));
	   }

	   return 0;
	}


/****************************************************************************************************************
 ****************************************************************************************************************
	This section of code is for all functions used when running under Windows 9x
 ****************************************************************************************************************
 ****************************************************************************************************************/

	/*
		This function creates and draws a window with a countdown to poweroff.
		at the end of the timer it calls the shutdownand poweroff functions.
	 */
	void doWin9xShutdown(int timeDelay)
	{
		HINSTANCE hInstance = s_hInstance;
		HINSTANCE hPrevInstance = s_hPrevInstance;
    
		HANDLE hTimer;
		DWORD timerID;
		DWORD shutdownID;
		int flag = 0;


		s_downtime = timeDelay;

		syslog(LOG_INFO,"doWin9xShutdown: Displaying countdown dialog");
		/* the windows has been previously created, and should have been hidden - show it now and start countdown */
		ShowWindow( s_shutdownWindow, SW_SHOWDEFAULT);

		PlaySound("Default sound",NULL,SND_ASYNC|SND_ALIAS );
		/* create timer thread */
		hTimer=CreateThread(NULL,0,CountDown,&s_shutdownWindow,0,&timerID);
		/* now wait for it to finish, and then initiate the shutdown */
		WaitForSingleObjectEx(hTimer,INFINITE,FALSE);
		CloseHandle(hTimer);
		hTimer = NULL;
		syslog(LOG_INFO,"doWin9xShutdown: preparing to create shutdown thread");
		/* do exit windowsEx in separate thread, as it seems to block too long to finish the monitor thread */
		s_hExitWindowsExThread = CreateThread(NULL,0,DoExitWindowsExThread,NULL,0,&shutdownID);
		syslog(LOG_INFO,"doWin9xShutdown: created shutdown thread - returning control to monitor loop");
	}

	/**
	 * This function is run in a separate thread and fires redraw events every second to get the timer displayed
	 **/
	DWORD WINAPI DoExitWindowsExThread(LPVOID lpvParam)
	{
		DWORD ret = 0;
		ret = ExitWindowsEx(EWX_SHUTDOWN|get9xShutdownMask(&s_curConfig),0);
		syslog(LOG_CRIT,"Sys shutdown command returns %d: %u\n",ret,GetLastError());
		if(ret == 0)
		{
			MessageBox(NULL,"ERROR: WinNUT was unable to shutdown the machine\nUPS power will be cut soon, shutdown NOW!","SHUTDOWN FAILED",MB_OK);
			/* do we really want winnut to exit here? - can cause master to shutdown earlier than absolutely necessary */
			/* making this have the same function as under NT, where we keep talking to upsd
			PostMessage(s_shutdownWindow,WM_DESTROY,0,0); */
		}
		ExitThread(0);
		return 0;
	}
	/**
	 * This function is run in a separate thread and fires redraw events every second to get the timer displayed
	 **/
	DWORD WINAPI CountDown(LPVOID lpvParam)
	{
		HWND hWnd=* ((HWND *) lpvParam);
		/* sleep 1 second */
		RedrawWindow(hWnd,NULL,NULL,RDW_INVALIDATE);
		Sleep(1000);
		/* while still counting down */
		while(s_downtime > 0)
		{
			/* decrement the seconds counter */
			s_downtime--;
			/* force redraw of window */
			RedrawWindow(hWnd,NULL,NULL,RDW_INVALIDATE);
			/* force us to foreground */
			SetForegroundWindow(hWnd);
			
			/* one beep every 10 sec */
			if(s_downtime % 10 == 0)
			{
				PlaySound("Default sound",NULL,SND_ASYNC|SND_ALIAS );
			}

			/* sleep another second */
			Sleep(1000);
		}
		RedrawWindow(hWnd,NULL,NULL,RDW_INVALIDATE);

		ExitThread(0);
		return 0;
	}



/****************************************************************************************************************
 ****************************************************************************************************************
	This section of code is for all functions used when running under Windows NT
 ****************************************************************************************************************
 ****************************************************************************************************************/

	/* This is for the NT event log messages lookup */
	BOOL WINAPI DllMain(
	  HINSTANCE hinstDLL,  // handle to DLL module
	  DWORD fdwReason,     // reason for calling function
	  LPVOID lpvReserved   // reserved
	)
	{
		return TRUE;
	}


	/*
		This creates an entry in the NT event log.  It's severity is the eventType (defined in WinNUTMessages.h)
	 */
	void createNTEventLogEntry(DWORD eventType, char *message,...)
	{
		TCHAR   buf[1024];
		HANDLE  hEventSource;
		LPTSTR  messages[1];
		va_list	args;


		hEventSource = RegisterEventSource(NULL, REG_UPSMON_SERVICE_NAME);
		/* format the message */
		va_start (args, message);
		_vsnprintf(buf,sizeof(buf), message, args);
		va_end (args);
		messages[0] = buf;

		if (hEventSource != NULL) 
		{
			ReportEvent(hEventSource,(WORD)eventType,0,eventType,NULL,1,0,messages,NULL);
			DeregisterEventSource(hEventSource);
		}
	}

	BOOL aquireNTShutdownPriviledges()
	{
		BOOL privSup=FALSE;
		HANDLE curThread=0;
		HANDLE curProcess=0;
		HANDLE curToken=0;
		LUID luid;
		TOKEN_PRIVILEGES tkp;
		char msg[512];

		if(!(curProcess=GetCurrentProcess()))
		{
		   syslog(LOG_ERR,"ERROR: while starting service - could not get current thread\nAborting");
		   createNTEventLogEntry(MSG_GENERAL_ERROR,"UPSMON: ERROR: while starting service - could not get current thread\nAborting");
		   return FALSE;
		}
		if(!(OpenProcessToken(curProcess,TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&curToken)))
		{
		   _snprintf(msg,sizeof(msg),"ERROR: while starting service - could not open process token:%d\nAborting"
			   ,GetLastError());
		   syslog(LOG_ERR,msg);
		   createNTEventLogEntry(MSG_GENERAL_ERROR,msg);
		   return FALSE;
		}
		if(!LookupPrivilegeValue(NULL,"SeShutdownPrivilege",&luid))
		{
		   _snprintf(msg,sizeof(msg),"ERROR: while starting service - could not get privilege name: %d\nAborting\n",
			   GetLastError());
		   syslog(LOG_ERR,msg);
		   createNTEventLogEntry(MSG_GENERAL_ERROR,msg);
		   return FALSE;
		}
		tkp.PrivilegeCount=1;
		tkp.Privileges[0].Luid=luid;
		tkp.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
		if(!(AdjustTokenPrivileges(curToken,FALSE,&tkp, sizeof(TOKEN_PRIVILEGES), 
			(PTOKEN_PRIVILEGES) NULL, (PDWORD) NULL)))
		{
		   _snprintf(msg,sizeof(msg),"ERROR: while starting service - Could not set shutdown privilege: %u\nAborting\n",
			   GetLastError());
		   syslog(LOG_ERR,msg);
		   createNTEventLogEntry(MSG_GENERAL_ERROR,msg);
		   return FALSE;
		} 
		return TRUE;
	}

	int isShutdownTypeHibernate()
	{
		if(s_curConfig.shutdownType==SHUTDOWN_TYPE_HIBERNATE)
		{
			return TRUE;
		}
		return FALSE;
	}
	/**
	 * This is the code that shuts down a winNT/2000 box
	 * The waitTime parameter specifies the amount of delay before shutting down 
	 **/
	void doWinNTShutdown(int waitTime)
	{
		char msg[512];
		HANDLE hToken;              // handle to process token 
		TOKEN_PRIVILEGES tkp;       // pointer to token structure 
		BOOL forceShutdown = FALSE;
		BOOL ret = FALSE;
 
		createNTEventLogEntry(MSG_GENERAL_WARNING,"UPS on battery and battery low. Shutting down NOW!");
		syslog(LOG_CRIT,"UPSMON: UPS on battery and battery low. Shutting down NOW!");

		/*
			reinterpret the force flags - note that since we don't use the exitwindowsex function, we don't get
			the option for shutdowns with the force if hung - maybe in the future we should add the ability to
			use the ExitWindowsEx function instead of InitiateSystemShutdown
		*/
		if(   (s_curConfig.shutdownType == SHUTDOWN_TYPE_FORCE)
		   || (s_curConfig.shutdownType == SHUTDOWN_TYPE_FORCEIFHUNG))
		{
			forceShutdown = TRUE;
		}

 
		// Get the current process token handle so we can get shutdown 
		// privilege. 
 
		if (!OpenProcessToken(GetCurrentProcess(), 
				TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) 
			syslog(LOG_CRIT,"UPSMON: Unable to complete shutdown - couldn't get process token to add shutdown priviledge");
 
		// Get the LUID for shutdown privilege. 
 
		LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, 
				&tkp.Privileges[0].Luid); 
 
		tkp.PrivilegeCount = 1;  // one privilege to set    
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
 
		// Get shutdown privilege for this process. 
 
		AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
			(PTOKEN_PRIVILEGES) NULL, 0); 
 
		// Cannot test the return value of AdjustTokenPrivileges. 
 
		if (GetLastError() != ERROR_SUCCESS) 
			syslog(LOG_CRIT,"UPSMON: Unable to complete shutdown - failed to get shutdown priviledge");
 


		/* need to treat win NT shutdowns differently than win2k and win XP*/

		switch(s_curConfig.oslevel)
		{
		case OSLEVEL_WINNT:
		case OSLEVEL_WIN2K:
		case OSLEVEL_WINXP:
		case OSLEVEL_WIN2K3:
		case OSLEVEL_WINVISTA:
		case OSLEVEL_WIN7:

			if(s_curConfig.shutdownType==SHUTDOWN_TYPE_HIBERNATE)
			{	
				//MessageBox(NULL,"Initiating System Hibernation.","HIBERNATION IN PROGRESS",MB_OK);
				ret = SetSuspendState(TRUE, FALSE, FALSE);
			}
			else
			{			
				ret = InitiateSystemShutdown(NULL,"UPS on battery and battery low. Shutting down NOW!",waitTime,forceShutdown,FALSE);
			}
			_snprintf(msg,sizeof(msg),"Shutdown in %d seconds.  Sys shutdown command returns %d: %u\n",
				waitTime,
				ret,
				GetLastError());
			if(ret == FALSE)
			{
				MessageBox(NULL,"ERROR: Shutdown failed\nUPS will be cutting power soon - SHUTDOWN NOW","SHUTDOWN FAILED",MB_OK);
				/*TODO: should we signal the service stop event to shutdown winNUT
					Advantage: the program is left in a state it cannot recover from - it is waiting for the immenent shutdown
						which should be imminent because the master will power off the ups no matter what now.  If somehow the 
						machine doesn't shut down, winnut won't function properly.
					Disadvantage: If will allow nut to exit, the master assumes we've shut off, and may power off the ups 
						sooner than absolutely necessary, not giving the user enough time to manually exit.  On the other hand
						if the master waits too long, it may not have time to properly shutdown before the ups gives out.  Note
						that it should time out waiting on us with time to shutdown itself, but that gets into a lot of guess 
						work.
				 */
			}

			syslog(LOG_CRIT,msg);
			createNTEventLogEntry(MSG_GENERAL_WARNING,msg);
			break;
	/*   TODO: used direct dll calls to load this information
		case OSLEVEL_WIN2K:
		case OSLEVEL_WINXP:
		case OSLEVEL_WIN2K3:
		case OSLEVEL_WINVISTA:
		case OSLEVEL_WIN7:
			_snprintf(msg,sizeof(msg),"Shutdown in %d seconds.  Sys shutdown command returns %d: %u\n",
				waitTime,
				InitiateSystemShutdownEx(NULL,"UPS on battery and battary low.  Shutting down NOW!",waitTime,TRUE,FALSE,0),
				GetLastError());
			syslog(LOG_CRIT,msg);
			createNTEventLogEntry(MSG_GENERAL_WARNING,msg);
			break;
			*/
		}
	}

/****************************************************************************************************************
 ****************************************************************************************************************
	This section of code is for all functions used when running as a NT Service
 ****************************************************************************************************************
 ****************************************************************************************************************/

	void RunInServiceMode()
	{
		BOOL retVal = FALSE;
		SERVICE_TABLE_ENTRY serviceStartTable[2];
		serviceStartTable[0].lpServiceName = REG_UPSMON_SERVICE_NAME;
		serviceStartTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)winNUTServiceMain;
		serviceStartTable[1].lpServiceName = NULL;
		serviceStartTable[1].lpServiceProc = NULL;

		retVal = StartServiceCtrlDispatcher(serviceStartTable);
    
		if (retVal == FALSE)
		{
			syslog(LOG_CRIT,"RunInServiceMode - StartServiceCtrlDispatcher failed - startup aborted");
			createNTEventLogEntry(MSG_GENERAL_ERROR, "RunInServiceMode - StartServiceCtrlDispatcher failed - startup aborted");
		}
	}

	void WINAPI winNUTServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
	{
		DWORD currentEvent = 0;
		s_hStatusHandle = RegisterServiceCtrlHandler( REG_UPSMON_SERVICE_NAME, winNUTServiceHandler);
		if (s_hStatusHandle == 0)
		{
			createNTEventLogEntry(MSG_GENERAL_ERROR,"Could not register service control handler - aborting startup");
			UpdateSCMStatus(SERVICE_STOPPED);
			return;
		}

		UpdateSCMStatus(SERVICE_START_PENDING);

		if(!aquireNTShutdownPriviledges())
		{
			createNTEventLogEntry(MSG_GENERAL_ERROR,"Could not aquire shutdown priviledges - aborting startup");
			syslog(LOG_ALERT,"Could not aquire shutdown priviledges - aborting startup");
			UpdateSCMStatus(SERVICE_STOPPED);
			return;
		}

		UpdateSCMStatus(SERVICE_START_PENDING);
		doMonitorPreConfig();

		UpdateSCMStatus(SERVICE_RUNNING);
		syslog(LOG_ALERT,"WinNUTUpsMon Service is starting to monitor UPS");

		while(currentEvent != SERVICE_CONTROL_STOP)
		{
			monitor();
			currentEvent = getNextEvent();
			syslog(LOG_INFO,"In winNUTServiceMain- Received service command ID: %d",currentEvent);
			while(   (currentEvent != SERVICE_CONTROL_STOP)
				  && (currentEvent != 0))
			{
				switch(currentEvent)
				{
				case SERVICE_CONTROL_NETBINDADD:
				case SERVICE_CONTROL_NETBINDREMOVE:
				case SERVICE_CONTROL_NETBINDENABLE:
				case SERVICE_CONTROL_NETBINDDISABLE:
					/* this will force us to re-login to the ups's when we go back to monitoring */
					UpdateSCMStatus(s_Status.dwCurrentState);
					logoffAllUPSs();

				}
				currentEvent = getNextEvent();
				syslog(LOG_INFO,"In winNUTServiceMain- Received service command ID: %d",currentEvent);
			}
		}
		UpdateSCMStatus(SERVICE_STOP_PENDING);
		
		/* TODO: any other cleanup code */
		logoffAllUPSs();

		UpdateSCMStatus(SERVICE_STOPPED);

	}

	VOID WINAPI winNUTServiceHandler(DWORD newControlCode)
	{
		switch(newControlCode)
		{
			case SERVICE_CONTROL_SHUTDOWN:
				/* make service shutdown look like service stop */
				insertEventInQueue(SERVICE_CONTROL_STOP);
				SetEvent(s_hStopMonitorEvent);
				break;
			case SERVICE_CONTROL_STOP:
			case SERVICE_CONTROL_NETBINDADD:
			case SERVICE_CONTROL_NETBINDREMOVE:
			case SERVICE_CONTROL_NETBINDENABLE:
			case SERVICE_CONTROL_NETBINDDISABLE:
				insertEventInQueue(newControlCode);
				SetEvent(s_hStopMonitorEvent);
				break;
			case SERVICE_CONTROL_INTERROGATE:
				UpdateSCMStatus(s_Status.dwCurrentState);
				break;
			default:
				break;
		}
	}

	/* 
	 This sends the Service Control Manager (SCM) the updated SERVICE_STATUS structure specifiying our new state
	 */
	BOOL UpdateSCMStatus(DWORD newState)
	{
		static int sCheckPoint=1;
		BOOL ret = FALSE;
		s_Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS;
		s_Status.dwWin32ExitCode = NO_ERROR;
		s_Status.dwServiceSpecificExitCode = NO_ERROR;
		s_Status.dwCurrentState = newState;
		switch(newState)
		{
		case SERVICE_START_PENDING:
		case SERVICE_STOP_PENDING:
			if(s_curConfig.oslevel >= OSLEVEL_WIN2K)
				s_Status.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN|SERVICE_ACCEPT_NETBINDCHANGE|SERVICE_CONTROL_INTERROGATE;
			else
				s_Status.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN|SERVICE_CONTROL_INTERROGATE;
			s_Status.dwWaitHint = 3000;
			s_Status.dwCheckPoint = ++sCheckPoint;
			break;
		case SERVICE_RUNNING:
			if(s_curConfig.oslevel >= OSLEVEL_WIN2K)
				s_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN|SERVICE_ACCEPT_NETBINDCHANGE|SERVICE_CONTROL_INTERROGATE;
			else
				s_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN|SERVICE_CONTROL_INTERROGATE;
			s_Status.dwWaitHint = 0;
			s_Status.dwCheckPoint = 0;
			break;
		case SERVICE_STOPPED:
			s_Status.dwControlsAccepted = SERVICE_CONTROL_INTERROGATE;
			s_Status.dwWaitHint = 0;
			s_Status.dwCheckPoint = 0;
			break;
		
		}
		ret = SetServiceStatus( s_hStatusHandle, &s_Status);
		if (!ret) 
		{
			syslog(LOG_CRIT,"UpdateStatus failed - Status code to be returned: %d, Last Error %d",newState, GetLastError());
		}
		return ret;
	}

	/*
		this inserts a contol code into the event queue for later retrieval by the main service.
		Note that this function can fail to put the event in the queue if it cannot gain control
		of the mutex in 500ms as this function will typically be called from threads that are shouldn't
		be blocked for long periods.
	*/
	void insertEventInQueue(newControlCode)
	{
		EventQueueNode *newNode = NULL;
		if(s_hEventQueueAccess == NULL)
		{
			HANDLE tmphdl = CreateMutex(NULL,FALSE,EVENT_QUEUE_MUTEX_NAME);
			if(GetLastError() != ERROR_ALREADY_EXISTS)
				s_hEventQueueAccess = tmphdl;
		}

		newNode = (EventQueueNode *)malloc(sizeof(EventQueueNode));
		if(newNode == NULL)
		{
			createNTEventLogEntry(MSG_GENERAL_ERROR,"Malloc failed - couldn't create event node for queue, requested control code %d will be ignored",newControlCode);
			syslog(LOG_CRIT,"Malloc failed - couldn't create event node for queue, requested control code %d will be ignored",newControlCode);
			return;
		}
		newNode->eventId = newControlCode;
		newNode->next=NULL;
		/* we do a timeout here and ignore the event in the queue if we can't access it, as we don't want to block the thread */
		if(WaitForSingleObjectEx(s_hEventQueueAccess,500,FALSE) != WAIT_OBJECT_0)
		{
			createNTEventLogEntry(MSG_GENERAL_ERROR,"Couldn't gain control of event queue mutex in reasonable amount of time, requested control code %d will be ignored",newControlCode);
			syslog(LOG_CRIT,"Couldn't gain control of event queue mutex in reasonable amount of time, requested control code %d will be ignored",newControlCode);
		}
		/* entering critical section */
		if(s_EventQueueHead == NULL)
		{
			s_EventQueueHead = newNode;
		}
		else
		{
			EventQueueNode *tmp = s_EventQueueHead;
			while(tmp->next != NULL)
			{
				tmp = tmp->next;
			}
			tmp->next = newNode;
		}
		ReleaseMutex(s_hEventQueueAccess);
	}

	/*
		This gets the next control code in the queue, or returns NO_EVENT if there are none
	*/
	DWORD getNextEvent()
	{
		DWORD nextVal = 0;
		if(s_hEventQueueAccess != NULL)
		{
			/* wait for access - we can wait a while in this loop */
			while(WaitForSingleObjectEx(s_hEventQueueAccess,INFINITE,FALSE) != WAIT_OBJECT_0);
			if(s_EventQueueHead != NULL)
			{
				nextVal = s_EventQueueHead->eventId;
				s_EventQueueHead = s_EventQueueHead->next;
			}
			ReleaseMutex(s_hEventQueueAccess);
		}
		return nextVal;
	}


