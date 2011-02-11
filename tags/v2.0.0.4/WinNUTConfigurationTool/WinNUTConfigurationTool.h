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
// WinNUTConfigurationTool.h : main header file for the WINNUTCONFIGURATIONTOOL application
//

#if !defined(AFX_WINNUTCONFIGURATIONTOOL_H__33C66EB2_832B_4A1F_B068_5FFA71F14305__INCLUDED_)
#define AFX_WINNUTCONFIGURATIONTOOL_H__33C66EB2_832B_4A1F_B068_5FFA71F14305__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "ConfigToolDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CWinNUTConfigurationToolApp:
// See WinNUTConfigurationTool.cpp for the implementation of this class
//

class CWinNUTConfigurationToolApp : public CWinApp
{
protected:
	void InitializeConfig(WinNutConf *curConfig);
	void LoadSettingsFromRegistry(WinNutConf *curConfig);
	void SaveSettingsToRegistry(WinNutConf *curConfig);
	int GetOSLevel();


public:
	CWinNUTConfigurationToolApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWinNUTConfigurationToolApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CWinNUTConfigurationToolApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WINNUTCONFIGURATIONTOOL_H__33C66EB2_832B_4A1F_B068_5FFA71F14305__INCLUDED_)
