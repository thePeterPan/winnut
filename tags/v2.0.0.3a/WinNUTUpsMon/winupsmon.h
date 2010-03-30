#ifndef NUT_WINUPSMON_H
#define NUT_WINUPSMON_H

/* winupsmon.h - prototypes for important functions used when linking

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

/* 
   The following constants are used by the internal syslog function - originally from linux source, but
   the values have changed to allow for simple log filtering
 */

/* this is the amount of time the countdown dialog is shown */
#define SHUTDOWN_LEADTIME (15)

/* Define return codes for loadRegInfo */
#define REG_ERROR_LOG_FILE	(1)
#define REG_ERROR_CONF_FILE	(2)

#define DEFAULT_LOG_PATH "c:\\upsmonStartupErrors.log"

/* 
	these made up the main function - they have been split up to facilitate reconnecting to ups's after
	we get a network config change notification (at least in NT service mode for now)
*/
void doMonitorPreConfig();
int monitor();
void logoffAllUPSs();
int get_var(utype *ups, const char *var, char *buf, size_t bufsize);

#endif