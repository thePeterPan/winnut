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

// ConfigToolDlg.cpp : implementation file
//

#include "stdafx.h"
#include "windows.h"
#include "winsvc.h"

extern "C"
{
#include "..\winupscommon.h"
}
#include "WinNUTConfigurationTool.h"
#include "ConfigToolDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/* forward declare poll thread */
UINT PollWinNUTStatusThread(LPVOID lpvParam);

/////////////////////////////////////////////////////////////////////////////
// CConfigToolDlg dialog
/*
CConfigToolDlg::CConfigToolDlg(CWnd* pParent )
	: CDialog(CConfigToolDlg::IDD, pParent)
{
	m_conf = NULL;
	m_LastDir = NULL;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}
*/
CConfigToolDlg::CConfigToolDlg(CWnd* pParent /*=NULL*/,
							   WinNutConf *conf)
	: CDialog(CConfigToolDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigToolDlg)
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_conf = NULL;

	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	strcpy(m_lastPath,"");
	m_conf = conf;
	m_STNormal = "Normal";
	m_STForce = "Forced";
	m_STForceIfHung = "Force If Hung";
	m_STHibernate = "Hibernate";
}

void CConfigToolDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigToolDlg)
	DDX_Control(pDX, IDC_USE_TIMED_SHUTDOWN, m_UseTimedShutdown);
	DDX_Control(pDX, IDC_UPSD_SHUTDOWN_DELAY, m_shutdownDelay);
	DDX_Control(pDX, IDC_UPSMON_STATUS, m_UpsMonStatus);
	DDX_Control(pDX, IDC_SHUTDOWN_TYPE, m_ShutdownType);
	DDX_Control(pDX, IDC_UPSD_PORT, m_UpsdPort);
	DDX_Control(pDX, IDC_WIN98_STARTUP_CHECKBOX, m_Win9XAutoStartup);
	DDX_Control(pDX, IDC_SERVICE_DESC_LABEL, m_ServiceDescLabel);
	DDX_Control(pDX, IDC_SERVICE_SETTING_STARTUP_MANUAL, m_ServiceSettingManual);
	DDX_Control(pDX, IDC_SERVICE_SETTING_STARTUP_DISABLED, m_ServiceSettingDisabled);
	DDX_Control(pDX, IDC_LOG_WARNING, m_LogWarning);
	DDX_Control(pDX, IDC_LOG_NOTICE, m_LogNotice);
	DDX_Control(pDX, IDC_LOG_INFO, m_LogInfo);
	DDX_Control(pDX, IDC_LOG_ERROR, m_LogError);
	DDX_Control(pDX, IDC_LOG_DEBUG, m_LogDebug);
	DDX_Control(pDX, IDC_LOG_CRITICAL, m_LogCritical);
	DDX_Control(pDX, IDC_LOG_ALERT, m_LogAlert);
	DDX_Control(pDX, IDC_SERVICE_SETTING_STARTUP_AUTO, m_ServiceSettingAuto);
	DDX_Control(pDX, IDC_INSTALLDIR, m_InstallDirPath);
	DDX_Control(pDX, ICD_LOGFILE_PATH, m_LogFilePath);
	DDX_Control(pDX, ICD_CONFIGFILEPATH, m_ConfigFilePath);
	DDX_Control(pDX, IDC_SERVICE_SETTING_STARTUP, m_ServiceStartupSetting);
	DDX_Control(pDX, IDC_ENABLE_SERVICE, m_InstallAsService);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CConfigToolDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigToolDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_CONF_BROWSE, OnConfBrowse)
	ON_BN_CLICKED(IDC_INSTALL_BROWSE, OnInstallBrowse)
	ON_BN_CLICKED(IDC_LOG_BROWSE, OnLogBrowse)
	ON_BN_CLICKED(IDC_ENABLE_SERVICE, OnEnableServiceChange)
	ON_BN_CLICKED(IDC_EDIT_CONF, OnEditConf)
	ON_BN_CLICKED(IDC_VIEW_LOG, OnViewLog)
	ON_BN_CLICKED(IDC_APPLY_AND_START, OnApplyAndStart)
	ON_BN_CLICKED(IDC_STOP, OnStop)
	ON_CBN_EDITCHANGE(IDC_SHUTDOWN_TYPE, OnEditShutdownType)
	ON_BN_CLICKED(IDC_USE_TIMED_SHUTDOWN, OnUseTimedShutdown)
	ON_EN_KILLFOCUS(IDC_UPSD_SHUTDOWN_DELAY, OnKillfocusUpsdShutdownDelay)
	ON_EN_KILLFOCUS(IDC_UPSD_PORT, OnKillfocusUpsdPort)
	ON_EN_KILLFOCUS(IDC_INSTALLDIR, OnKillfocusInstalldir)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigToolDlg message handlers

BOOL CConfigToolDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	//Add shutdown options for all OS levels
	m_ShutdownType.InsertString(-1,m_STNormal);
	m_ShutdownType.SetItemData(0,SHUTDOWN_TYPE_NORMAL);
	m_ShutdownType.InsertString(-1,m_STForce);
	m_ShutdownType.SetItemData(1,SHUTDOWN_TYPE_FORCE);
	//Add shutdown options for specific OS levels
	if(   (m_conf->oslevel == OSLEVEL_WIN2K)
	   || (m_conf->oslevel == OSLEVEL_WINXP)
	   || (m_conf->oslevel == OSLEVEL_WIN2K3)
	   || (m_conf->oslevel == OSLEVEL_WINVISTA)
	   || (m_conf->oslevel == OSLEVEL_WIN7))
	{
		m_ShutdownType.InsertString(-1,m_STForceIfHung);
		m_ShutdownType.SetItemData(2,SHUTDOWN_TYPE_FORCEIFHUNG);
		m_ShutdownType.InsertString(-1, m_STHibernate);
		m_ShutdownType.SetItemData(3, SHUTDOWN_TYPE_HIBERNATE);
	}
	
	LoadDefaultsFromConfObject();
	doServiceDepCheck();
	doShutdownDelayDepCheck();
	UpdateWinNUTStatus();
	AfxBeginThread(PollWinNUTStatusThread,(LPVOID*)this);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CConfigToolDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CConfigToolDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CConfigToolDlg::OnConfBrowse() 
{
	char *confPath = new char[MAXPATHLEN+1];
	int ret = 0;
	int i;
	ret = m_ConfigFilePath.GetLine(0,confPath,MAXPATHLEN);
	confPath[ret]='\0';
	if(strlen(confPath) == 0)
	{
		if(strlen(m_lastPath) == 0)
		{
			strcpy(confPath,"c:\\*.*");
		}
		else
		{
			strncpy(confPath,m_lastPath,MAXPATHLEN);
		}
	}
	CFileDialog dlg(TRUE,NULL,confPath,OFN_LONGNAMES|OFN_HIDEREADONLY,"All Files (*.*)|*.*||",NULL);
	ret = dlg.DoModal();
	if(ret == IDOK)
	{
		m_ConfigFilePath.SetWindowText(dlg.GetPathName());
	}
	//get the current path to use as a last path
	strncpy(m_lastPath,dlg.GetPathName(),MAXPATHLEN);
	for(i = strlen(m_lastPath); i > 0; i--)
	{
		if(   (m_lastPath[i] == '\\')
		   || (m_lastPath[i] == '/'))
		{
			m_lastPath[i+1] = '*';
			m_lastPath[i+2] = '.';
			m_lastPath[i+3] = '*';
			m_lastPath[i+4] = '\0';
			break;
		}
	}
}

/**
 This is actually now the location of the WinNUT executable
 */
void CConfigToolDlg::OnInstallBrowse() 
{
	char *installPath = new char[MAXPATHLEN+1];
	int ret = 0;
	int i;
	ret=m_InstallDirPath.GetLine(0,installPath,MAXPATHLEN);
	installPath[ret]='\0';
	if(ret == 0)
	{
		if(strlen(m_lastPath) == 0)
		{
			strcpy(installPath,"c:\\*.*");
		}
		else
		{
			strncpy(installPath,m_lastPath,MAXPATHLEN);
		}
	}
	CFileDialog dlg(TRUE,NULL,installPath,OFN_LONGNAMES|OFN_HIDEREADONLY,"Executable (*.exe)|*.exe|All Files (*.*)|*.*||",NULL);
	ret = dlg.DoModal();
	if(ret == IDOK)
	{
		m_InstallDirPath.SetWindowText(dlg.GetPathName());
	}
	//get the current path to use as a last path
	strncpy(m_lastPath,dlg.GetPathName(),MAXPATHLEN);
	for(i = strlen(m_lastPath); i > 0; i--)
	{
		if(   (m_lastPath[i] == '\\')
		   || (m_lastPath[i] == '/'))
		{
			m_lastPath[i+1] = '*';
			m_lastPath[i+2] = '.';
			m_lastPath[i+3] = '*';
			m_lastPath[i+4] = '\0';
			break;
		}
	}
}

void CConfigToolDlg::OnLogBrowse() 
{
	char *logPath = new char[MAXPATHLEN+1];
	int ret = 0;
	int i;
	ret=m_LogFilePath.GetLine(0,logPath,MAXPATHLEN);
	logPath[ret]='\0';
	if(strlen(logPath) == 0)
	{
		if(strlen(m_lastPath) == 0)
		{
			strcpy(logPath,"c:\\*.*");
		}
		else
		{
			strncpy(logPath,m_lastPath,MAXPATHLEN);
		}
	}
	CFileDialog dlg(TRUE,NULL,logPath,OFN_LONGNAMES|OFN_HIDEREADONLY,"All Files (*.*)|*.*||",NULL);
	ret = dlg.DoModal();
	if(ret == IDOK)
	{
		m_LogFilePath.SetWindowText(dlg.GetPathName());
	}
	//get the current path to use as a last path
	strncpy(m_lastPath,dlg.GetPathName(),MAXPATHLEN);
	for(i = strlen(m_lastPath); i > 0; i--)
	{
		if(   (m_lastPath[i] == '\\')
		   || (m_lastPath[i] == '/'))
		{
			m_lastPath[i+1] = '*';
			m_lastPath[i+2] = '.';
			m_lastPath[i+3] = '*';
			m_lastPath[i+4] = '\0';
			break;
		}
	}
}

void CConfigToolDlg::OnEnableServiceChange() 
{
	doServiceDepCheck();
}

void CConfigToolDlg::OnOK() 
{
	StoreToConfObject();
	CDialog::OnOK();
}

/*
	This sets the associated service radio buttons and groupbox as enabled if winnut is to be installed 
	as a service, and disabled if not.

 */
void CConfigToolDlg::doServiceDepCheck()
{
	if(   (m_conf->oslevel >= OSLEVEL_BASE_WIN9X)
	   && (m_conf->oslevel < OSLEVEL_BASE_NT))
	{
		m_InstallAsService.SetCheck(0);
		m_InstallAsService.EnableWindow(false);
		m_InstallAsService.ShowWindow(SW_HIDE);
		m_ServiceStartupSetting.EnableWindow(false);
		m_ServiceSettingAuto.EnableWindow(false);
		m_ServiceSettingManual.EnableWindow(false);
		m_ServiceSettingDisabled.EnableWindow(false);
		m_ServiceDescLabel.EnableWindow(false);
	}
	else
	{
		m_Win9XAutoStartup.SetCheck(0);
		m_Win9XAutoStartup.EnableWindow(false);;
		m_Win9XAutoStartup.ShowWindow(SW_HIDE);
		if(m_InstallAsService.GetCheck() != 1)
		{
			m_ServiceStartupSetting.EnableWindow(false);
			m_ServiceSettingAuto.EnableWindow(false);
			m_ServiceSettingManual.EnableWindow(false);
			m_ServiceSettingDisabled.EnableWindow(false);
			m_ServiceDescLabel.EnableWindow(false);
		}
		else
		{
			m_ServiceStartupSetting.EnableWindow(true);
			m_ServiceSettingAuto.EnableWindow(true);
			m_ServiceSettingManual.EnableWindow(true);
			m_ServiceSettingDisabled.EnableWindow(true);
			m_ServiceDescLabel.EnableWindow(true);
		}
	}
}

void CConfigToolDlg::OnEditShutdownType() 
{
	// do nothing - don't need now
}

void CConfigToolDlg::OnUseTimedShutdown() 
{
	doShutdownDelayDepCheck();
}

void CConfigToolDlg::doShutdownDelayDepCheck()
{
	if(m_UseTimedShutdown.GetCheck() == 1)
	{
		m_shutdownDelay.EnableWindow(true);
	}
	else
	{
		m_shutdownDelay.EnableWindow(false);
	}
}

/*
	This pulls the data to initialize the dialog box from the WinNutConf structure and loads it into the controls
 */
void CConfigToolDlg::LoadDefaultsFromConfObject()
{
	char buf[MAXPATHLEN+1]="";
	if(m_conf != NULL)
	{
		m_InstallDirPath.SetWindowText(m_conf->execPath);
		m_LogFilePath.SetWindowText(m_conf->logPath);
		m_ConfigFilePath.SetWindowText(m_conf->confPath);
		m_LogAlert.SetCheck((m_conf->logLevel & 0x00000002)?1:0);
		m_LogCritical.SetCheck((m_conf->logLevel & 0x00000004)?1:0);
		m_LogError.SetCheck((m_conf->logLevel & 0x00000008)?1:0);
		m_LogWarning.SetCheck((m_conf->logLevel & 0x00000010)?1:0);
		m_LogNotice.SetCheck((m_conf->logLevel & 0x00000020)?1:0);
		m_LogInfo.SetCheck((m_conf->logLevel & 0x00000040)?1:0);
		m_LogDebug.SetCheck((m_conf->logLevel & 0x00000080)?1:0);
		_snprintf(buf,sizeof(buf),"%d",m_conf->upsdPort);
		m_UpsdPort.SetWindowText(buf);
		if(m_conf->shutdownAfterSeconds >= 0)
		{
			m_UseTimedShutdown.SetCheck(1);
			_snprintf(buf,sizeof(buf),"%d",m_conf->shutdownAfterSeconds);
			m_shutdownDelay.SetWindowText(buf);
		}
		else
		{
			m_UseTimedShutdown.SetCheck(0);
			m_shutdownDelay.SetWindowText("0");
		}
		m_InstallAsService.SetCheck(m_conf->runAsService);
		m_Win9XAutoStartup.SetCheck(m_conf->win9XAutoStart);
		m_ServiceSettingDisabled.SetCheck((m_conf->serviceStartup==4)?1:0);
		m_ServiceSettingManual.SetCheck((m_conf->serviceStartup==3)?1:0);
		m_ServiceSettingAuto.SetCheck((m_conf->serviceStartup==2)?1:0);
		strncpy(m_lastPath,m_conf->curAppPath,MAXPATHLEN);
		if(m_conf->shutdownType == SHUTDOWN_TYPE_NORMAL)
		{
			m_ShutdownType.SetCurSel(0);
		}
		else if(m_conf->shutdownType == SHUTDOWN_TYPE_FORCE)
		{
			m_ShutdownType.SetCurSel(1);
		}
		else if(m_conf->shutdownType == SHUTDOWN_TYPE_FORCEIFHUNG)
		{
			m_ShutdownType.SetCurSel(2);
		}
		else if(m_conf->shutdownType == SHUTDOWN_TYPE_HIBERNATE)
		{
			m_ShutdownType.SetCurSel(3);
		}
		else
		{
			//default to force
			m_ShutdownType.SetCurSel(1);
		}
	}
}

/*
	This stores the settings from the controls back into the WinNutConf structure
 */
void CConfigToolDlg::StoreToConfObject()
{
	int logLevel = 0;
	int ret = 0;
	char buf[MAXPATHLEN+1] = "";
	int tmpint=0;

	if(m_conf != NULL)
	{
		ret=m_InstallDirPath.GetLine(0,m_conf->execPath,MAXPATHLEN);
		m_conf->execPath[ret]='\0';
		ret=m_ConfigFilePath.GetLine(0,m_conf->confPath,MAXPATHLEN);
		m_conf->confPath[ret]='\0';
		ret=m_LogFilePath.GetLine(0,m_conf->logPath,MAXPATHLEN);
		m_conf->logPath[ret]='\0';
		ret=m_UpsdPort.GetLine(0,buf,MAXPATHLEN);
		buf[ret]='\0';
		m_conf->upsdPort = atoi(buf);

		if(m_UseTimedShutdown.GetCheck() == 1)
		{
			ret=m_shutdownDelay.GetLine(0,buf,MAXPATHLEN);
			buf[ret]='\0';
			tmpint=atoi(buf);
			if(tmpint < 0)
			{
				m_conf->shutdownAfterSeconds = 60; //default to sixty seconds
			}
			else
			{
				m_conf->shutdownAfterSeconds = tmpint;
			}
		}
		else
		{
			m_conf->shutdownAfterSeconds = -1;
		}
		if(m_LogAlert.GetCheck() == 1) logLevel |= 0x00000002;
		if(m_LogCritical.GetCheck() == 1) logLevel |= 0x00000004;
		if(m_LogError.GetCheck() == 1) logLevel |= 0x00000008;
		if(m_LogWarning.GetCheck() == 1) logLevel |= 0x00000010;
		if(m_LogNotice.GetCheck() == 1) logLevel |= 0x00000020;
		if(m_LogInfo.GetCheck() == 1) logLevel |= 0x00000040;
		if(m_LogDebug.GetCheck() == 1) logLevel |= 0x00000080;
		m_conf->logLevel = logLevel;

		if(m_InstallAsService.GetCheck() == 1)
		{
			m_conf->runAsService = TRUE;
		}
		else
		{
			m_conf->runAsService = FALSE;
		}

		if(m_Win9XAutoStartup.GetCheck() == 1)
		{
			m_conf->win9XAutoStart = TRUE;
		}
		else
		{
			m_conf->win9XAutoStart = FALSE;
		}

		if(m_ServiceSettingDisabled.GetCheck() == 1) m_conf->serviceStartup = 4;
		if(m_ServiceSettingManual.GetCheck() == 1) m_conf->serviceStartup = 3;
		if(m_ServiceSettingAuto.GetCheck() == 1) m_conf->serviceStartup = 2;

		m_conf->shutdownType = m_ShutdownType.GetItemData(m_ShutdownType.GetCurSel());
	}
}

void CConfigToolDlg::OnEditConf() 
{
	int ret = 0;
	char	exec[MAXPATHLEN+1]="";
	char	filepath[MAXPATHLEN+1]="";
	STARTUPINFO startInfo;
	PROCESS_INFORMATION processInfo;

	ZeroMemory(&startInfo,sizeof(startInfo));
	startInfo.cb=sizeof(startInfo);
	ret=m_ConfigFilePath.GetLine(0,filepath,MAXPATHLEN);
	filepath[ret]='\0';
	_snprintf (exec, MAXPATHLEN, "notepad \"%s\"", filepath);
	CreateProcess(NULL,exec,NULL,NULL,FALSE,0,NULL,NULL,&startInfo,&processInfo);

}

void CConfigToolDlg::OnViewLog() 
{
	int ret =0;
	char	exec[MAXPATHLEN+1]="";
	char	filepath[MAXPATHLEN+1]="";
	STARTUPINFO startInfo;
	PROCESS_INFORMATION processInfo;

	ZeroMemory(&startInfo,sizeof(startInfo));
	startInfo.cb=sizeof(startInfo);
	ret=m_LogFilePath.GetLine(0,filepath,MAXPATHLEN);
	filepath[ret]='\0';
	_snprintf (exec, MAXPATHLEN, "notepad \"%s\"", filepath);
	CreateProcess(NULL,exec,NULL,NULL,FALSE,0,NULL,NULL,&startInfo,&processInfo);
	
}

void CConfigToolDlg::OnApplyAndStart() 
{
	//Stop the service first so we don't accidentally start two copies or start an already running service
	//also make sure that it has really stopped before we try to start another copy.
	while(stopWinNUT(m_conf))
	{
		Sleep(50);
	}
	
	
	//update the settings in the registry
	StoreToConfObject();
	saveSettingsToRegistry(m_conf);
	/* some problems - possibly caused by the service config changing - closing service and then immediately trying to reopen */
	Sleep(500);
	//now start it up / back up
	if(startWinNUT(m_conf) == FALSE)
	{
		DWORD error = ::GetLastError();
		char buf[512]="";
		_snprintf(buf,sizeof(buf),"ERROR: Failed to start WinNUT UPS Monitor\n Error #%d",error);
		MessageBox(buf,"Startup Error",MB_OK);
	}
	/* Give winnut a chance to start up so we can properly display it's status */
	Sleep(500);
	UpdateWinNUTStatus();
}

void CConfigToolDlg::OnStop() 
{
	//stop until we get a failure - this takes care of multiple instances running as service and/or applications
	while(stopWinNUT(m_conf))
	{
		Sleep(50);
	}
	UpdateWinNUTStatus();
}

void CConfigToolDlg::UpdateWinNUTStatus()
{
	char buf[512]="";
	DWORD status = getWinNUTStatus(m_conf);
	_snprintf(buf,sizeof(buf),"%s%s%s%s",
		(status & RUNNING_SERVICE_MODE || status & RUNNING_APP_MODE)?"Running as ":"Not Running",
		(status & RUNNING_SERVICE_MODE)?"Service":"",
		(status & RUNNING_SERVICE_MODE && status & RUNNING_APP_MODE)?" & ":"",
		(status & RUNNING_APP_MODE)?"Application":"");
	m_UpsMonStatus.SetWindowText(buf);
}

UINT PollWinNUTStatusThread(LPVOID lpvParam)
{
	CConfigToolDlg *dlg = (CConfigToolDlg*)lpvParam;
	while(TRUE)
	{
		Sleep(5000);
		dlg->UpdateWinNUTStatus();
	}
	ExitThread(0);
	return 0;
}




void CConfigToolDlg::OnKillfocusUpsdShutdownDelay() 
{
	int tmpint=0;
	int ret = 0;
	char buf[MAXPATHLEN+1];

	if(m_UseTimedShutdown.GetCheck() == 1)
	{
		ret=m_shutdownDelay.GetLine(0,buf,MAXPATHLEN);
		buf[ret]='\0';
		tmpint=atoi(buf);

		if(tmpint <= 0)
		{
			MessageBox("The shutdown delay must be a number greater than zero","Error",MB_OK);
		}
	}
	
}

void CConfigToolDlg::OnKillfocusUpsdPort() 
{
	int tmpint=0;
	int ret = 0;
	char buf[MAXPATHLEN+1];

	ret=m_UpsdPort.GetLine(0,buf,MAXPATHLEN);
	buf[ret]='\0';
	tmpint=atoi(buf);

	if(   (tmpint <= 0)
	   || (tmpint > 65536))
	{
		MessageBox("The upsd port must be a number greater than zero and less than 65536","Error",MB_OK);
	}
}

void CConfigToolDlg::OnKillfocusInstalldir() 
{
	int ret = 0;
	char buf[MAXPATHLEN+1];

	ret=m_InstallDirPath.GetLine(0,buf,MAXPATHLEN);
	buf[ret]='\0';

	if(   (strlen(buf) != 0)
	   && (   (buf[ret-4] != '.')
	       || (   (buf[ret-3] != 'e')
	           && (buf[ret-3] != 'E'))
	       || (   (buf[ret-2] != 'x')
	           && (buf[ret-2] != 'X'))
	       || (   (buf[ret-1] != 'e')
	           && (buf[ret-1] != 'E'))))
	{
		MessageBox("The WinNUT Executable Path should be the full path to the executable. (ie should usually end with the WinNUTUpsMon.exe)","Error",MB_OK);
	}
}
