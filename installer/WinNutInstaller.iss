[_ISTool]
EnableISX=false

[Files]
Source: installFiles\WinNUTUpsMon.exe; DestDir: {app}; MinVersion: 4.00.950,4.00.1381; Flags: confirmoverwrite
Source: installFiles\AlertPopup.exe; DestDir: {app}
Source: installFiles\ChangeList.txt; DestDir: {app}
Source: installFiles\COPYING; DestDir: {app}
Source: installFiles\KnownBugs.txt; DestDir: {app}
Source: installFiles\QuickStart.txt; DestDir: {app}
Source: installFiles\Readme.txt; DestDir: {app}
Source: installFiles\upsmon.conf; DestDir: {app}; Flags: onlyifdoesntexist uninsneveruninstall
Source: installFiles\WinNUTConfigurationTool.exe; DestDir: {app}; MinVersion: 4.00.950,4.00.1381; Flags: confirmoverwrite
Source: installFiles\upsmon.conf; DestDir: {app}; DestName: upsmon-InstallerDefault.conf

[INI]
Filename: {app}\WinNUT.url; Section: InternetShortcut; Key: URL; String: http://csociety.ecn.purdue.edu/~delpha/winnut/
Filename: {app}\NutUPS.url; Section: InternetShortcut; Key: URL; String: http://random.networkupstools.org

[Registry]
Root: HKLM; Subkey: Software\WinNUT\upsmon; ValueType: string; ValueName: ConfigurationFilePath; ValueData: {app}\upsmon.conf; Flags: createvalueifdoesntexist uninsdeletekeyifempty uninsdeletevalue
Root: HKLM; Subkey: Software\WinNUT\upsmon; ValueType: string; ValueName: LogFilePath; ValueData: {app}\WinNUTUpsMon.log; Flags: createvalueifdoesntexist uninsdeletekeyifempty uninsdeletevalue
Root: HKLM; Subkey: Software\WinNUT\upsmon; ValueType: string; ValueName: ExeFilePath; ValueData: {app}\WinNUTUpsMon.exe; Flags: uninsdeletekey
Root: HKLM; Subkey: Software\WinNUT\upsmon; ValueType: dword; ValueName: LogMask; ValueData: 126; Flags: uninsdeletekeyifempty uninsdeletevalue createvalueifdoesntexist
Root: HKLM; Subkey: Software\WinNUT\upsmon; ValueType: dword; ValueName: Port; ValueData: 3493; Flags: uninsdeletekeyifempty uninsdeletevalue createvalueifdoesntexist
Root: HKLM; Subkey: Software\WinNUT\upsmon; ValueType: dword; ValueName: ShutdownAfterSeconds; ValueData: $FFFFFFFF; Flags: createvalueifdoesntexist uninsdeletekeyifempty uninsdeletevalue
Root: HKLM; Subkey: Software\WinNUT\upsmon; ValueType: dword; ValueName: ShutdownType; ValueData: 4; Flags: createvalueifdoesntexist uninsdeletekeyifempty uninsdeletevalue

[UninstallDelete]
Name: {app}\WinNut.url; Type: files
Name: {app}\NutUPS.url; Type: files

[Icons]
Name: {group}\WinNUT Configuration Tool; Filename: {app}\WinNUTConfigurationTool.exe; WorkingDir: {app}; IconFilename: {app}\WinNUTConfigurationTool.exe; Comment: Configure WinNUT; IconIndex: 0
Name: {group}\Start WinNUT Ups Monitor; Filename: {app}\WinNUTUpsMon.exe; Parameters: -c start; WorkingDir: {app}; IconFilename: {app}\WinNUTUpsMon.exe; Comment: Start the WinNUT UPS Monitor; IconIndex: 0
Name: {group}\Stop WinNUT Ups Monitor; Filename: {app}\WinNUTUpsMon.exe; Parameters: -c stop; WorkingDir: {app}; IconFilename: {app}\WinNUTUpsMon.exe; Comment: Stop the WinNUT Ups Monitor; IconIndex: 0
Name: {group}\WinNUT Homepage; Filename: {app}\WinNUT.url
Name: {group}\Network Ups Tools Homepage; Filename: {app}\NutUPS.url


[Components]
Name: WinNUT_Ups_Monitor; Description: The WinNUT UPS Monitor tool; Types: full custom compact

[Setup]
OutputDir=C:\myfiles\projects\winnut\trunk\installer\installerOutput
SourceDir=C:\myfiles\projects\winnut\trunk\installer
Compression=zip/9
AppCopyright=Copyright © 1999-2006 Andrew Delpha
AppName=WinNUT
AppVerName=WinNUT 2.0.0.4
InfoAfterFile=C:\myfiles\projects\winnut\trunk\installer\installFiles\QuickStart.txt
LicenseFile=C:\myfiles\projects\winnut\trunk\installer\installFiles\COPYING
PrivilegesRequired=admin
DefaultGroupName=WinNUT
DefaultDirName={pf}\WinNUT
RestartIfNeededByRun=false
AppPublisher=Andrew Delpha
AppPublisherURL=http://csociety.ecn.purdue.edu/~delpha/winnut/
AppSupportURL=http://csociety.ecn.purdue.edu/~delpha/winnut/
AppUpdatesURL=http://csociety.ecn.purdue.edu/~delpha/winnut/
AppVersion=WinNUT 2.0.0.4
AppID=WinNUT
UninstallDisplayName=WinNUT
OutputBaseFilename=WinNUT-Installer
AlwaysShowComponentsList=true
AlwaysShowDirOnReadyPage=true
AlwaysShowGroupOnReadyPage=true
ShowComponentSizes=true
FlatComponentsList=true


[UninstallRun]
Filename: {app}\WinNUTConfigurationTool.exe; Parameters: /uninstall; WorkingDir: {app}

[Run]
Filename: {app}\WinNUTConfigurationTool.exe; WorkingDir: {app}; Flags: postinstall

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nYou MUST stop any currently running instances of WinNUT for the install to work properly.%n%n[name] comes with ABSOLUTELY NO WARRANTY; See the license on the next page or the COPYING file in the installation directory for full details.%n
