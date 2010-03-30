* WinNutUpsMon Documentation
* by Andrew Delpha <delpha@computer.org>
*
* Network UPS Tools maintained by Arnaud Quette, of MGE UPS SYSTEMS.
* Many thanks to Russell Kroll, the original author and maintainer, for 
*   his excellent work and years of dedication to the project
* Windows WinNutUPSMon monitor ported and maintained by 
*   Andrew Delpha <delpha@computer.org>
*    see CREDITS file in the nut package for more details
*
* Released under the GNU GPL - see COPYING for details.
*
* Program support page: http://random.networkupstools.org/
*            WinNUT at: http://csociety.ecn.purdue.edu/~delpha/winnut/

DESCRIPTION
===========
This is a windows port of the upsmon client provided with the 
Network UPS Tools package.  It provides a background 
application/service to monitor a ups, display the desired
notifications, and shutdown the windows computer if necessary.

FILES INCLUDED IN BINARY PACKAGE (after installation)
================================
> alertPopup.exe - This takes a message provided on the command 
	line and displays it in a modal popup window.
> WinNutConfigurationTool.exe - This utility configures the location 
	of key files as well as other options.  It also allows you to
	start and stop WinNutUpsMon
> WinNutUpsMon.exe - This is the actual monitor utility.  See instructions
	below for how to start the monitor.
> upsmon.conf - The configuration file (with windows specific comments, 
	but same format as the unix version)
> QuickStart.txt - Quick guide to installing the WinNut Ups Monitor
> ReadMe.txt - This file
> ChangeLog.txt - List of changes between versions
> COPYING - copy GNU General Public License

INSTALLATION:
=============
1) You must have a nut ups daemon (v1.4.x and greater supported) running 
on a another system (windows version is currently the monitor client only) 
that's monitoring the desired ups.

2) Run the WinNutConfigurationTool.exe
   NOTE: NT/2000 users: you need to be logged in as an administrative user
   A) WinNut Executable Path should be the full path to WinNutUpsMon.exe
   B) Configuration file path should be the full path to the upsmon.conf 
      (you will edit this file in step 3)
   C) Log File path should be the full path to where you want the log file 
      created
   D) Select which log channels you wish to log 
	Checking Debug creates a VERY large file over time, but selecting
	everything but debug is very reasonable.
   E) Select the appropriate options for the NT service / automatic startup 
      as you desire.
   F) Select your default UPSD port (can be overridden per MONITOR line in 
	the upsmon.conf file) 
      NOTE: All versions of nut since Version 0.50.0 use 3493 by default, 
        earlier versions used 3305.  Your server may have been setup with 
        a different port.  You can override this value in the upsmon.conf
	file per server being monitored.
   G) Timed Shutdown - If enabled, WinNUTUpsMon will shut the system down after
      the ups is on battery for the specified number of seconds.  If the battery
      reaches the critical point before this time is up, WinNUTUpsMon shuts the
      machine down immediately**.  WARNING - don't set this too small, or UPS
      self tests may cause it to shutdown when you don't really want it to.
      Note that there is a 5 second window around the number of seconds specified
      where the shutdown may start due to when polls of the server relative to when
      the power failed.

      **=Note as of release 0.45.2b, I've ported over the new NUT upsmon feature of
	 waiting up to HOSTSYNC (specified in upsmon.conf) seconds for the master 
         UPS to send out the FSD (force shut down command) and then shuts down.  
         If this time expires, WinNUT gives up and shuts down anyway.  This helps 
         alieviate race conditions where the slave systems shutdown as power is 
         restored and the master doesn't shutdown.
   H) Shutdown Method - This is a very important setting.  I would recommend
      Forced unless you have problems.
	* Normal - This is a normal shutdown, where applications get a chance
	  to prompt the user to save a changed document, etc.  Things like
	  active desktop don't like the forced shutdown.
 	  See Normal footnote below for more details.
	* Forced - Using this shutdown method, applications don't get a chance
	  to ask the user to save their work. (Note, all shutdown methods have
	  a warning dialog with a count down that is displayed for x seconds
	  where x is specified with the FINALDELAY directive in the conf file.
	* Force If Hung - (Only available in 2000 and later) - it is presently
	  the same as forced.  NOTE: I haven't had reports of problems under
	  NT / 2000 with the forced shutdowns.  I use a different shutdown call
	  under NT / 2000 that only has forced and normal modes (it provides the
	  countdown dialog).

	  Normal footnote - This can cause
	  a machine that is not manned at the time of the power failure to 
	  not shut down properly.  Also, if an application gets hung, it will
	  prevent proper shutdown.  There is another problem that a 
	  user can cancel the shutdown by hitting the cancel button on one of
	  the save dialogs.  When this happens WinNut will remain running (but
	  won't try to shutdown the computer again), so that the Master upsmon 
	  process won't shut off the ups power until this computer shuts down
          (at which point we notify it we're powering off) or it gives up on
	  us and powers down anyway (this case also runs the risk of the master
	  not getting a good shutdown if it's timeout waiting for us is too 
	  long).  
	  Some programs (most notably ActiveDesktop) don't like forced 
	  shutdowns.  Active Desktop will come back up in recovery mode if
	  it the machine is shutdown with the Forced option.  I've also heard
	  of some computers that require a 3rd party app to power off the 
	  machine, and it gets killed with the force option and doesn't power
 	  off the machine - I'm guessing it would need the normal shutdown to
	  work.  Anyway, I definately recommend forced with Windows NT/2000, 
	  and try forced with windows 9x, and if you have trouble then switch
	  to normal.
	  
3) Edit the upsmon.conf to
   A) Setup the ups the machine is monitoring (see the MONITOR section)
   B) By default the notify command is set to "c:\\Program Files\\WinNUT\\alertPopup.exe"
        Fill in the path to the notify command in the NOTIFYCMD section if this is not where 
        you installed WinNUT
      Note: This should work with any program that will take a message as 
        a double quoted command line arg.  The alertPopup.exe is provide for
        this purpose as noted in the example.
      NOTE: You must escape backslashes in paths in the configuration file
        ie: c:\bin\WinNUT needs to be in the config file as c:\\bin\\WinNUT
      NOTE: surround file names double quotes if they contain spaces
   C) Any other settings as you see appropriate - note that some settings 
      are ignored in the windows world.

4) Start up the daemon
   -> The Configuration tool can stop and start WinNUT in both service and application
      mode (it starts it in whatever mode your current configuration indicates).
   OR
   -> Use the program icons installed under Start->Programs->WinNUT 
   OR
   -> Run at the run prompt or command line, or setup a shortcut link to 
        "WinNutUpsMon.exe -c start"
   OR
   -> Under win9x and NT/2000 (when not configured as service) you can start
      WinNUT simply by double clicking WinNutUpsMon.exe.
   OR
   -> Under NT/2000 (configured to run as a service):
      If it is in automatic mode, it will automatically start after your next reboot. To start it
      now, you can use the manual start instructions:
      If you opted to start it manually, do the following
      Select Start->Run
        Type (without quotes) "net start WinNutUpsMon" and press enter
      or use the GUI services manager from the control panel.

5) Shutting down the daemon:
   -> Launch the Configuration tool and press the stop button
   OR
   -> Use the program icons installed under Start->Programs->WinNUT 
   OR
   -> Run at the run prompt or command line, or setup a shortcut link to 
        "WinNutUpsMon.exe -c stop"
   OR
   -> If under NT/2000 it is running as a service
      Select Start->Run
        Type (without quotes) "net stop WinNutUpsMon" and press enter
      or use the GUI services manager from the control panel.

OTHER NOTES:
============
-> Under Windows 9x, the WinNUTUpsMon will not show up as a running process when you press 
   <CTRL><ALT><DEL> as it is registered as a background process so it doesn't get killed
   when a user logs out.  Under NT/2000, if WinNUT is run in application mode, it will get
   killed when the user who started it logs out (I don't think there's any way around this
   under NT).  Use the service version if you want it to run all the time.

There are several configuration variables that WinNUT ignores.  
The following are ignored
SHUTDOWNCMD
POWERDOWNFLAG

Using net send
  If you would like to use net send to send messages under NT/2000:
  I would suggest using a batch file as the command and placing the
  net send command in it, as I don't think the current launch command
  can deal with passing the parameters.  Note: the net command is a 
  console command, so for every message it sends, it will briefly 
  popup a command window on the screen that may take focus away from 
  the current app - I haven't found any way around this (if you know,
  please tell me).

UNINSTALLING:
=============
Use the uninstaller in the Start->Programs->WinNUT menu
OR
Use the Add/Remove programs tool under the control panel

The upsmon.conf will be left in the program folder in case you are 
uninstalling for an upgrade

If for any reason this should fail, here's the old info on manual removal

Launch the configuration tool
  ->Stop WinNut
  - Under Windows 9x/ME
    -> uncheck the automatic startup box
  - Under Windows NT/2000/XP(whistler)
    -> uncheck the run as service
Note the path to the log file, config file, and the executables
Close the configuration tool

Using regedit remove \HKEY_LOCAL_MACHINE\Software\WinNUT and all it's subkeys

If you are under windows NT/2000 Use regedit to remove the following key
  \\HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog\Application\WinNutUPSMon
  NOTE: Removing this key will cause the application event logger to not recognize WinNUT 
  events in the application log.  I've not seen this cause any real problems thus far though.

Delete the WinNUT program directory (if you unzipped the WinNUT stuff in a directory by itself)
Delete the logfile and config file (if they were not in the WinNUT directory)
This should remove all traces of WinNUT from your system
