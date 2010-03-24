/* 

   Copyright (C) 2000

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
// WinNUTConfigurationTool.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"

extern "C"
{
#include "..\winupscommon.h"
}

#include "WinNUTConfigurationTool.h"
#include "ConfigToolDlg.h"
#include <direct.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWinNUTConfigurationToolApp

BEGIN_MESSAGE_MAP(CWinNUTConfigurationToolApp, CWinApp)
	//{{AFX_MSG_MAP(CWinNUTConfigurationToolApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinNUTConfigurationToolApp construction

CWinNUTConfigurationToolApp::CWinNUTConfigurationToolApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CWinNUTConfigurationToolApp object

CWinNUTConfigurationToolApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CWinNUTConfigurationToolApp initialization

BOOL CWinNUTConfigurationToolApp::InitInstance()
{
	WinNutConf curConfig={0};
	int pos = 0;
	LPTSTR commandLine = NULL;
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	InitializeConfig(&curConfig);
	LoadSettingsFromRegistry(&curConfig);

	//We're going to check for the uninstall flag
	commandLine = GetCommandLine();
	if(commandLine != NULL)
	{
		CString commandLineCStr(commandLine);
		int uninstallFlag = commandLineCStr.Find("/uninstall");
		if(uninstallFlag != -1)
		{
			/*
			    OK - we've been given the uninstall flag.  We need to
				1) Stop any running instance
				2) uninstall services and auto start if currently installed
			 */
			//1) stop any running instances
			DWORD status = getWinNUTStatus(&curConfig);
			if(status & (RUNNING_SERVICE_MODE | RUNNING_APP_MODE))
			{
				while(stopWinNUT(&curConfig))
				{
					Sleep(50);
				}
			}

			//2) uninstall any services and auto start if currently installed
			//Do this by saving the default configuration 
			InitializeConfig(&curConfig);
			saveSettingsToRegistry(&curConfig);

			//now exit
			return FALSE;
		}
	}

	_getcwd(curConfig.curAppPath,MAXPATHLEN);
	pos = strlen(curConfig.curAppPath);
	curConfig.curAppPath[pos]='\\';
	curConfig.curAppPath[pos+1]='*';
	curConfig.curAppPath[pos+2]='.';
	curConfig.curAppPath[pos+3]='*';
	curConfig.curAppPath[pos+4]='\0';

	CConfigToolDlg dlg(NULL,&curConfig);
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		SaveSettingsToRegistry(&curConfig);
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
void CWinNUTConfigurationToolApp::LoadSettingsFromRegistry(WinNutConf *curConfig)
{
	loadSettingsFromRegistry(curConfig);
}

void CWinNUTConfigurationToolApp::SaveSettingsToRegistry(WinNutConf *curConfig)
{
	saveSettingsToRegistry(curConfig);
}

int CWinNUTConfigurationToolApp::GetOSLevel()
{
	return getOSLevel();
}



/**
 * This initializes the values in the structure so that if there are no registry settings
 * we have some defaults in here.
 */
void CWinNUTConfigurationToolApp::InitializeConfig(WinNutConf *curConfig)
{
	initializeConfig(curConfig);
}
