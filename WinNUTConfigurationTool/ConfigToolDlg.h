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
// ConfigToolDlg.h : header file
//

#if !defined(AFX_CONFIGTOOLDLG_H__D54BA966_32EE_41CB_A229_0D26056398A2__INCLUDED_)
#define AFX_CONFIGTOOLDLG_H__D54BA966_32EE_41CB_A229_0D26056398A2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CConfigToolDlg dialog

class CConfigToolDlg : public CDialog
{
// Construction
public:
	CConfigToolDlg(CWnd* pParent = NULL);	// standard constructor
	CConfigToolDlg(CWnd* pParent /*=NULL*/, WinNutConf *conf);
	void UpdateWinNUTStatus();
// Dialog Data
	//{{AFX_DATA(CConfigToolDlg)
	enum { IDD = IDD_WINNUTCONFIGURATIONTOOL_DIALOG };
	CButton	m_UseTimedShutdown;
	CEdit	m_shutdownDelay;
	CEdit	m_UpsMonStatus;
	CComboBox	m_ShutdownType;
	CEdit	m_UpsdPort;
	CButton	m_Win9XAutoStartup;
	CStatic	m_ServiceDescLabel;
	CButton	m_ServiceSettingManual;
	CButton	m_ServiceSettingDisabled;
	CButton	m_LogWarning;
	CButton	m_LogNotice;
	CButton	m_LogInfo;
	CButton	m_LogError;
	CButton	m_LogDebug;
	CButton	m_LogCritical;
	CButton	m_LogAlert;
	CButton	m_ServiceSettingAuto;
	CEdit	m_InstallDirPath;
	CEdit	m_LogFilePath;
	CEdit	m_ConfigFilePath;
	CButton	m_ServiceStartupSetting;
	CButton	m_InstallAsService;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigToolDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
	WinNutConf *m_conf;
	char m_lastPath[MAXPATHLEN+1];
	const char* m_STNormal;
	const char* m_STForce;
	const char* m_STForceIfHung;
    const char* m_STHibernate;

	void doServiceDepCheck();
	void doShutdownDelayDepCheck();
	void LoadDefaultsFromConfObject();
	void StoreToConfObject();

	// Generated message map functions
	//{{AFX_MSG(CConfigToolDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnConfBrowse();
	afx_msg void OnInstallBrowse();
	afx_msg void OnLogBrowse();
	afx_msg void OnEnableServiceChange();
	virtual void OnOK();
	afx_msg void OnEditConf();
	afx_msg void OnViewLog();
	afx_msg void OnApplyAndStart();
	afx_msg void OnStop();
	afx_msg void OnEditShutdownType();
	afx_msg void OnUseTimedShutdown();
	afx_msg void OnKillfocusUpsdShutdownDelay();
	afx_msg void OnKillfocusUpsdPort();
	afx_msg void OnKillfocusInstalldir();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGTOOLDLG_H__D54BA966_32EE_41CB_A229_0D26056398A2__INCLUDED_)
