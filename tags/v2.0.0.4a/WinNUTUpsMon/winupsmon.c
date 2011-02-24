/* upsmon - monitor power status over the 'net (talks to upsd via TCP)

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define WIN32BUILD
#define NOTIFY_LIST_MESSAGES_INCLUDE

#include "common.h"

#include "upsclient.h"
#include "upsmon.h"
#include "parseconf.h"

#include "..\winupscommon.h"
#include "winupsmon.h"
#include "WinCompatabilityFunctions.h"
#include "WinNUTMain.h"



static	char	*shutdowncmd = NULL, *notifycmd = NULL;
static	char	*powerdownflag = NULL;

static	int	minsupplies = 1, sleepval = 5, deadtime = 15;

	/* default polling interval = 5 sec */
static	int	pollfreq = 5, pollfreqalert = 5;

	/* slave hosts are given 15 sec by default to logout from upsd */
static	int	hostsync = 15;  

	/* sum of all power values from config file */
static	int	totalpv = 0;

	/* default replace battery warning interval (seconds) */
static	int	rbwarntime = 43200;

	/* default "all communications down" warning interval (seconds) */
static	int	nocommwarntime = 300;

	/* default interval between the shutdown warning and the shutdown */
static	int	finaldelay = 5;

	/* set by SIGHUP handler, cleared after reload finishes */
static	int	reload_flag = 0;

	/* set after SIGINT, SIGQUIT, or SIGTERM */
static	int	exit_flag = 0;

	/* userid for unprivileged process when using fork mode */
static	char	*run_as_user = NULL;

	/* SSL details - where to find certs, whether to use them */
static	char	*certpath = NULL;
static	int	certverify = 0;		/* don't verify by default */
static	int	forcessl = 0;		/* don't require ssl by default */

static	int	debuglevel = 0, userfsd = 0, use_pipe = 1, pipefd[2];

	/* WinNUT
	    This flag indicates that all loops should be broken to allow exit code to run to 
		nicely shutdown our process so windows doesn't get hung up trying to kill us */
	int s_poweringDownFlag = 0;

	/* the global config include */
	extern WinNutConf s_curConfig;
	extern int syserrno;


static	utype	*firstups = NULL;


#ifdef SHUT_RDWR
#define	shutdown_how SHUT_RDWR
#else
#define	shutdown_how 2
#endif

static void setflag(int *val, int flag)
{
	*val = (*val |= flag);
}

static void clearflag(int *val, int flag)  
{
	*val = (*val ^= (*val & flag));
}

static int flag_isset(int num, int flag)
{
	return ((num & flag) == flag);
}

static void wall(char *text)
{
	//WNTODO: check to make sure this doesn't block
	char	exec[LARGEBUF]="";
	STARTUPINFO startInfo;
	PROCESS_INFORMATION processInfo;

	if(notifycmd != NULL)
	{
		ZeroMemory(&startInfo,sizeof(startInfo));
		startInfo.cb=sizeof(startInfo);
		snprintf(exec, sizeof(exec), "%s \"%s\"", notifycmd, text);
		CreateProcess(NULL,exec,NULL,NULL,FALSE,0,NULL,NULL,&startInfo,&processInfo);
	}
}

static void notify (char *notice, int flags, char *ntype, char *upsname)
{
	char	exec[LARGEBUF]="";
	STARTUPINFO startInfo;
	PROCESS_INFORMATION processInfo;

	if (flag_isset(flags, NOTIFY_SYSLOG))
		syslog (LOG_NOTICE, notice);

	if (flag_isset(flags, NOTIFY_WALL))
		wall (notice);

	if (flag_isset (flags, NOTIFY_EXEC)) {
		if (notifycmd != NULL) {
			if (upsname)
				SetEnvironmentVariable ("UPSNAME", upsname);
			else
				SetEnvironmentVariable ("UPSNAME", "");
			snprintf(exec, sizeof(exec), "%s \"%s\"", notifycmd, notice);

			SetEnvironmentVariable ("NOTIFYTYPE", ntype);
			ZeroMemory(&startInfo,sizeof(startInfo));
			startInfo.cb=sizeof(startInfo);
			CreateProcess(NULL,exec,NULL,NULL,FALSE,0,NULL,NULL,&startInfo,&processInfo);

		}
	}
}

static void do_notify(const utype *ups, int ntype)
{
	int	i;
	char	msg[SMALLBUF], *upsname = NULL;

	/* grab this for later */
	if (ups)
		upsname = ups->sys;

	for (i = 0; notifylist[i].name != NULL; i++) {
		if (notifylist[i].type == ntype) {
			debug("do_notify: ntype 0x%04x (%s)\n", ntype, 
				notifylist[i].name);
			snprintf(msg, sizeof(msg), notifylist[i].msg, 
				ups ? ups->sys : "");
			notify(msg, notifylist[i].flags, (char *)notifylist[i].name, 
				upsname);
			return;
		}
	}

	/* not found ?! */
}

/* check for master permissions on the server for this ups */
static int checkmaster(utype *ups)
{
	char	buf[SMALLBUF];

	/* don't bother if we're not configured as a master for this ups */
	if (!flag_isset(ups->status, NUT_ST_MASTER))
		return 1;

	/* this shouldn't happen (LOGIN checks it earlier) */
	if ((ups->upsname == NULL) || (strlen(ups->upsname) == 0)) {
		upslogx(LOG_ERR, "Set master on UPS [%s] failed: empty upsname");
		return 0;
	}

	snprintf(buf, sizeof(buf), "MASTER %s\n", ups->upsname);

	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ALERT, "Can't set master mode on UPS [%s] - %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) == 0) {
		if (!strncmp(buf, "OK", 2))
			return 1;

		/* not ERR, but not caught by readline either? */

		upslogx(LOG_ALERT, "Master privileges unavailable on UPS [%s]", 
			ups->sys);
		upslogx(LOG_ALERT, "Response: [%s]", buf);
	}
	else {	/* something caught by readraw's parsing call */
		upslogx(LOG_ALERT, "Master privileges unavailable on UPS [%s]", 
			ups->sys);
		upslogx(LOG_ALERT, "Reason: %s", upscli_strerror(&ups->conn));
	}

	return 0;
}

/* authenticate to upsd, plus do LOGIN and MASTER if applicable */
static int do_upsd_auth(utype *ups)
{
	char	buf[SMALLBUF];

	if (!ups->un) {
		upslogx(LOG_ERR, "UPS [%s]: no username defined!", ups->sys);
		return 0;
	}

	if (ups->pv == 0)	/* monitor only, no need to login */
		return 1;

	snprintf(buf, sizeof(buf), "USERNAME %s\n", ups->un);
	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ERR, "Can't set username on [%s]: %s",
			ups->sys, upscli_strerror(&ups->conn));
			return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) < 0) {
		upslogx(LOG_ERR, "Set username on [%s] failed: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	/* authenticate first */
	snprintf(buf, sizeof(buf), "PASSWORD %s\n", ups->pw);

	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ERR, "Can't set password on [%s]: %s",
			ups->sys, upscli_strerror(&ups->conn));
			return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) < 0) {
		upslogx(LOG_ERR, "Set password on [%s] failed: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	/* catch insanity from the server - not ERR and not OK either */
	if (strncmp(buf, "OK", 2) != 0) {
		upslogx(LOG_ERR, "Set password on [%s] failed - got [%s]",
			ups->sys, buf);
		return 0;
	}

	/* we require a upsname now */
	if ((ups->upsname == NULL) || (strlen(ups->upsname) == 0)) {
		upslogx(LOG_ERR, "Login to UPS [%s] failed: empty upsname");
		return 0;
	}

	/* password is set, let's login */
	snprintf(buf, sizeof(buf), "LOGIN %s\n", ups->upsname);

	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ERR, "Login to UPS [%s] failed: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) < 0) {
		upslogx(LOG_ERR, "Can't login to UPS [%s]: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	/* catch insanity from the server - not ERR and not OK either */
	if (strncmp(buf, "OK", 2) != 0) {
		upslogx(LOG_ERR, "Login on UPS [%s] failed - got [%s]",
			ups->sys, buf);
		return 0;
	}

	/* finally - everything is OK */
	debug("Logged into UPS %s\n", ups->sys);
	setflag(&ups->status, NUT_ST_LOGIN);

	/* now see if we also need to test master permissions */
	return checkmaster(ups);
}

/* set flags and make announcements when a UPS has been checked successfully */
static void ups_is_alive(utype *ups)
{
	time_t	now;

	time(&now);
	ups->lastpoll = now;

	if (ups->commstate == 1)		/* already known */
		return;

	/* only notify for 0->1 transitions (to ignore the first connect) */
	if (ups->commstate == 0)
		do_notify(ups, NOTIFY_COMMOK);

	ups->commstate = 1;
}

/* handle all the notifications for a missing UPS in one place */
static void ups_is_gone(utype *ups)
{
	time_t	now;

	/* first time: clear the flag and throw the first notifier */
	if (ups->commstate != 0) {
		ups->commstate = 0;

		/* COMMBAD is the initial loss of communications */
		do_notify(ups, NOTIFY_COMMBAD);
		return;
	}

	time(&now);

	/* first only act if we're <nocommtime> seconds past the last poll */
	if ((now - ups->lastpoll) < nocommwarntime)
		return;

	/* now only complain if we haven't lately */
	if ((now - ups->lastncwarn) > nocommwarntime) {

		/* NOCOMM indicates a persistent condition */
		do_notify(ups, NOTIFY_NOCOMM);
		ups->lastncwarn = now;
	}
}

static void ups_on_batt(utype *ups)
{
	sleepval = pollfreqalert;	/* bump up polling frequency */

	if (flag_isset(ups->status, NUT_ST_ONBATT)) { 	/* no change */
		debug("ups_on_batt(%s) (no change)\n", ups->sys);
		return;
	}

	ups->linestate = 0;	

	debug("ups_on_batt(%s) (first time)\n", ups->sys);

	/* must have changed from OL to OB, so notify */

	do_notify(ups, NOTIFY_ONBATT);
	setflag(&ups->status, NUT_ST_ONBATT);
	clearflag(&ups->status, NUT_ST_ONLINE);
}

static void ups_on_line(utype *ups)
{
	if (flag_isset(ups->status, NUT_ST_ONLINE)) { 	/* no change */
		debug("ups_on_line(%s) (no change)\n", ups->sys);
		return;
	}

	debug("ups_on_line(%s) (first time)\n", ups->sys);

	/* ignore the first OL at startup, otherwise send the notifier */
	if (ups->linestate != -1)
		do_notify(ups, NOTIFY_ONLINE);

	ups->linestate = 1;

	setflag(&ups->status, NUT_ST_ONLINE);
	clearflag(&ups->status, NUT_ST_ONBATT);
}

/* create the flag file if necessary */
static void set_pdflag(void)
{
	FILE	*pdf;

	if (!powerdownflag)
		return;

	pdf = fopen(powerdownflag, "w");
	if (!pdf) {
		upslogx(LOG_ERR, "Failed to create power down flag!");
		return;
	}

	fprintf(pdf, "%s", SDMAGIC);
	fclose(pdf);
}

/* the actual shutdown procedure */
static void doshutdown(void)
{
	syslog (LOG_CRIT, "Executing automatic power-fail shutdown");

	do_notify (NULL, NOTIFY_SHUTDOWN);

	doWinShutdown(finaldelay);
	/*exit(EXIT_SUCCESS);*/
}

/* set forced shutdown flag so other upsmons know what's going on here */
static void setfsd(utype *ups)
{
	char	buf[SMALLBUF];
	int	ret;

	/* this shouldn't happen */
	if (!ups->upsname) {
		upslogx(LOG_ERR, "setfsd: programming error: no UPS name set [%s]",
			ups->sys);
		return;
	}

	debug("Setting FSD on UPS %s\n", ups->sys);

	snprintf(buf, sizeof(buf), "FSD %s\n", ups->upsname);

	ret = upscli_sendline(&ups->conn, buf, strlen(buf));

	if (ret < 0) {
		upslogx(LOG_ERR, "FSD set on UPS %s failed: %s", ups->sys,
			upscli_strerror(&ups->conn));
		return;
	}

	ret = upscli_readline(&ups->conn, buf, sizeof(buf));

	if (ret < 0) {
		upslogx(LOG_ERR, "FSD set on UPS %s failed: %s", ups->sys,
			upscli_strerror(&ups->conn));
		return;
	}

	if (!strncmp(buf, "OK", 2))
		return;

	/* protocol error: upsd said something other than "OK" */
	upslogx(LOG_ERR, "FSD set on UPS %s failed: %s", ups->sys, buf);
}


int get_var(utype *ups, const char *var, char *buf, size_t bufsize)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[4];
	char	**answer;

	/* this shouldn't happen */
	if (!ups->upsname) {
		upslogx(LOG_ERR, "get_var: programming error: no UPS name set [%s]",
			ups->sys);
		return -1;
	}

	numq = 0;

	if (!strcmp(var, "numlogins")) {
		query[0] = "NUMLOGINS";
		query[1] = ups->upsname;
		numq = 2;
	}

	if (!strcmp(var, "status")) {
		query[0] = "VAR";
		query[1] = ups->upsname;
		query[2] = "ups.status";
		numq = 3;
	}

	if (numq == 0) {
		upslogx(LOG_ERR, "get_var: programming error: var=%s", var);
		return -1;
	}

	debug("get_var: %s / %s\n", ups->sys, var);

	ret = upscli_get(&ups->conn, numq, query, &numa, &answer);

	if (ret < 0) {

		/* detect old upsd */
		if (upscli_upserror(&ups->conn) == UPSCLI_ERR_UNKCOMMAND) {

			upslogx(LOG_ERR, "UPS [%s]: Too old to monitor",
				ups->sys);
			return -1;
		}

		/* some other error */
		return -1;
	}

	if (numa < numq) {
		upslogx(LOG_ERR, "%s: Error: insufficient data "
			"(got %d args, need at least %d)\n", 
			var, numa, numq);
		return -1;
	}

	snprintf(buf, bufsize, "%s", answer[numq]);
	return 0;
}

/* WinNUT - this method should never be called, because we are never master - not porting over alarms*/
static void slavesync(void)
{
	utype	*ups;
	char	temp[SMALLBUF];
	time_t	start, now;
	int	maxlogins, logins;

	time(&start);

	for (;;) {
		maxlogins = 0;

		for (ups = firstups; ups != NULL; ups = ups->next) {

			/* only check login count on our master(s) */
			if (!flag_isset(ups->status, NUT_ST_MASTER))
				continue;

			//set_alarm();

			if (get_var(ups, "numlogins", temp, sizeof(temp)) >= 0) {
				logins = strtol(temp, (char **)NULL, 10);

				if (logins > maxlogins)
					maxlogins = logins;
			}

			//clear_alarm();
		}

		/* if no UPS has more than 1 login (us), then slaves are gone */
		if (maxlogins <= 1)
			return;

		/* after HOSTSYNC seconds, assume slaves are stuck and bail */
		time(&now);

		if ((now - start) > hostsync) {
			upslogx(LOG_INFO, "Host sync timer expired, forcing shutdown");
			return;
		}

		usleep(250000);
	}
}

static void forceshutdown(void)
{
	utype	*ups;
	int	isamaster = 0;

	debug("Shutting down any UPSes in MASTER mode...\n");

	/* set FSD on any "master" UPS entries (forced shutdown in progress) */
	for (ups = firstups; ups != NULL; ups = ups->next)
		if (flag_isset(ups->status, NUT_ST_MASTER)) {
			isamaster = 1;
			setfsd(ups);
		}

	/* if we're not a master on anything, we should shut down now */
	if (!isamaster)
	{
		doshutdown();
		if(!isShutdownTypeHibernate())
			s_poweringDownFlag = 1; /*WinNUT add powerdown flag*/
		return;
	}

	/* must be the master now */
	debug("This system is a master... waiting for slave logout...\n");

	/* wait up to HOSTSYNC seconds for slaves to logout */
	slavesync();

	/* time expired or all the slaves are gone, so shutdown */
	doshutdown();
	s_poweringDownFlag = 1;/*WinNUT add powerdown flag*/
}

static int is_ups_critical(utype *ups)
{
	time_t	now;
	static int giveup=0;

	/* FSD = the master is forcing a shutdown */
	if (flag_isset(ups->status, NUT_ST_FSD))
		return 1;

	time(&now);

	/*
	  ups isn't yet critical, but we've been requested to shutdown after being on batt 
	  for s_curConfig->shutdownAfterSeconds seconds
	 */
	 if(   (s_curConfig.shutdownAfterSeconds != -1)
	    && (flag_isset(ups->status, NUT_ST_ONBATT))
	    && (now - ups->lastonlinepower > s_curConfig.shutdownAfterSeconds))
	 {
        upslogx(LOG_WARNING, "UPS [%s] on battery for more than %d seconds. Shutting down system",
				  ups->sys, s_curConfig.shutdownAfterSeconds);
        return 1;
	 }

	/* not OB or not LB = not critical yet */
	if ((!flag_isset(ups->status, NUT_ST_ONBATT)) ||
		(!flag_isset(ups->status, NUT_ST_LOWBATT)))
	{
		giveup=0; /* clear giveup flag if we go back to normal */
		return 0;
	}

	/* must be OB+LB now */

	/* if we're a master, declare it critical so we set FSD on it */
	if (flag_isset(ups->status, NUT_ST_MASTER))
		return 1;

	/* must be a slave now */

	/* FSD isn't set, so the master hasn't seen it yet */

	/* give the master up to HOSTSYNC seconds before shutting down */
	if ((now - ups->lastnoncrit) > hostsync) {
		if(giveup == 0) /* only report it the first time though */
		{
			upslogx(LOG_WARNING, "Giving up on the master for UPS [%s]",
				ups->sys);
			giveup = 1;
		}
		return 1;
	}
	else
	{
		giveup=0;
	}

	/* there's still time left */
	return 0;
}

/* recalculate the online power value and see if things are still OK */
static void recalc(void)
{
	utype	*ups;
	int	val_ol = 0;
	time_t	now;

	time(&now);
	ups = firstups;
	while (ups != NULL) {
		/* promote dead UPSes that were last known OB to OB+LB */
		if ((now - ups->lastpoll) > deadtime)
			if (flag_isset(ups->status, NUT_ST_ONBATT)) {
				debug ("Promoting dead UPS: %s\n", ups->sys);
				setflag(&ups->status, NUT_ST_LOWBATT);
			}

		/* note: we assume that a UPS that isn't critical must be OK *
		 *							     *
		 * this means a UPS we've never heard from is assumed OL     *
		 * whether this is really the best thing to do is undecided  */

		/* crit = (FSD) || (OB & LB) > HOSTSYNC seconds */
		if (is_ups_critical(ups))
			debug("Critical UPS: %s\n", ups->sys);
		else
			val_ol += ups->pv;

		ups = ups->next;
	}

	/* debug("Current power value: %d\n", val_ol);
	debug("Minimum power value: %d\n", minsupplies); */

	if (val_ol < minsupplies)
		forceshutdown();
}		

static void ups_low_batt(utype *ups)
{
	if (flag_isset(ups->status, NUT_ST_LOWBATT)) { 	/* no change */
		debug("ups_low_batt(%s) (no change)\n", ups->sys);
		return;
	}

	debug("ups_low_batt(%s) (first time)\n", ups->sys);

	/* must have changed from !LB to LB, so notify */

	do_notify(ups, NOTIFY_LOWBATT);
	setflag(&ups->status, NUT_ST_LOWBATT);
}

static void upsreplbatt(utype *ups)
{
	time_t	now;

	time(&now);

	if ((now - ups->lastrbwarn) > rbwarntime) {
		do_notify(ups, NOTIFY_REPLBATT);
		ups->lastrbwarn = now;
	}
}

static void ups_fsd(utype *ups)
{
	if (flag_isset(ups->status, NUT_ST_FSD)) {		/* no change */
		debug("ups_fsd(%s) (no change)\n", ups->sys);
		return;
	}

	debug("ups_fsd(%s) (first time)\n", ups->sys);

	/* must have changed from !FSD to FSD, so notify */

	do_notify(ups, NOTIFY_FSD);
	setflag(&ups->status, NUT_ST_FSD);
}

/* cleanly close the connection to a given UPS */
static void drop_connection(utype *ups)
{
	debug("Dropping connection to UPS [%s]\n", ups->sys);

	ups->commstate = 0;
	ups->linestate = 0;
	clearflag(&ups->status, NUT_ST_LOGIN);
	clearflag(&ups->status, NUT_ST_CONNECTED);

	upscli_disconnect(&ups->conn);
}

/* change some UPS parameters during reloading */
static void redefine_ups(utype *ups, int pv, const char *un, 
		const char *pw, const char *master)
{
	ups->retain = 1;

	if (ups->pv != pv) {
		upslogx(LOG_INFO, "UPS [%s]: redefined power value to %d", 
			ups->sys, pv);
		ups->pv = pv;
	}

	totalpv += ups->pv;

	if (ups->un) {
		if (strcmp(ups->un, un) != 0) {
			upslogx(LOG_INFO, "UPS [%s]: redefined username",
				ups->sys);

			free(ups->un);

			if (un)
				ups->un = xstrdup(un);
			else
				ups->un = NULL;

			/* 
			 * if not logged in force a reconnection since this
			 * may have been redefined to make a login work
			 */

			if (!flag_isset(ups->status, NUT_ST_LOGIN)) {
				upslogx(LOG_INFO, "UPS [%s]: retrying connection\n",
					ups->sys);	

				drop_connection(ups);
			}

		}	/* if (strcmp(ups->un, un) != 0) { */

	} else {

		/* adding a username? (going to new style MONITOR line) */

		if (un) {
			upslogx(LOG_INFO, "UPS [%s]: defined username",
				ups->sys);

			ups->un = xstrdup(un);

			/* possibly force reconnection - see above */

			if (!flag_isset(ups->status, NUT_ST_LOGIN)) {
				upslogx(LOG_INFO, "UPS [%s]: retrying connection\n",
					ups->sys);	

				drop_connection(ups);
			}

		}	/* if (un) */
	}

	/* paranoia */
	if (!ups->pw)
		ups->pw = xstrdup("");	/* give it a bogus, but non-NULL one */

	/* obviously don't put the new password in the syslog... */
	if (strcmp(ups->pw, pw) != 0) {
		upslogx(LOG_INFO, "UPS [%s]: redefined password", ups->sys);

		if (ups->pw)
			free(ups->pw);

		ups->pw = xstrdup(pw);

		/* possibly force reconnection - see above */

		if (!flag_isset(ups->status, NUT_ST_LOGIN)) {
			upslogx(LOG_INFO, "UPS [%s]: retrying connection\n",
				ups->sys);

			drop_connection(ups);
		}
	}

	/* slave -> master */
	if ((!strcasecmp(master, "master")) && (!flag_isset(ups->status, NUT_ST_MASTER))) {
		upslogx(LOG_INFO, "UPS [%s]: redefined as master", ups->sys);
		setflag(&ups->status, NUT_ST_MASTER);

		/* reset connection to ensure master mode gets checked */
		drop_connection(ups);
		return;
	}

	/* master -> slave */
	if ((!strcasecmp(master, "slave")) && (flag_isset(ups->status, NUT_ST_MASTER))) {
		upslogx(LOG_INFO, "UPS [%s]: redefined as slave", ups->sys);
		clearflag(&ups->status, NUT_ST_MASTER);
		return;
	}
}

static void addups(int reloading, const char *sys, const char *pvs, 
		const char *un, const char *pw, const char *master)
{
	int	pv;
	utype	*tmp, *last;

	/* the username is now required - no more host-based auth */

	if ((!sys) || (!pvs) || (!pw) || (!master) || (!un)) {
		upslogx(LOG_WARNING, "Ignoring invalid MONITOR line in upsmon.conf!");
		upslogx(LOG_WARNING, "MONITOR configuration directives require five arguments.");
		return;
	}

	/* deal with sys without a upsname - refuse it */
	if (!strchr(sys, '@')) {
		upslogx(LOG_WARNING, "Ignoring invalid MONITOR line in ups.conf (%s)",
			sys);
		upslogx(LOG_WARNING, "UPS directives now require a UPS name "
			"(MONITOR upsname@hostname ...)");
		return;
	}

	pv = strtol(pvs, (char **) NULL, 10);

	if (pv < 0) {
		upslogx(LOG_WARNING, "UPS [%s]: ignoring invalid power value [%s]", 
			sys, pvs);
		return;
	}

	last = tmp = firstups;

	while (tmp) {
		last = tmp;

		/* check for duplicates */
		if (!strcmp(tmp->sys, sys)) {
			if (reloading)
				redefine_ups(tmp, pv, un, pw, master);
			else
				upslogx(LOG_WARNING, "Warning: ignoring duplicate"
					" UPS [%s]", sys);
			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(utype));
	tmp->sys = xstrdup(sys);
	tmp->pv = pv;

	/* build this up so the user doesn't run with bad settings */
	totalpv += tmp->pv;

	if (un)
		tmp->un = xstrdup(un);
	else
		tmp->un = NULL;

	tmp->pw = xstrdup(pw);
	tmp->status = 0;
	tmp->retain = 1;

	/* ignore initial COMMOK and ONLINE by default */
	tmp->commstate = -1;
	tmp->linestate = -1;

	tmp->lastpoll = 0;
	tmp->lastnoncrit = 0;
	tmp->lastrbwarn = 0;
	tmp->lastncwarn = 0;
	tmp->lastonlinepower = 0;

	if (!strcasecmp(master, "master"))
		setflag(&tmp->status, NUT_ST_MASTER);

	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		firstups = tmp;

	if (tmp->pv)
		upslogx(LOG_INFO, "UPS: %s (%s) (power value %d)", tmp->sys, 
			flag_isset(tmp->status, NUT_ST_MASTER) ? "master" : "slave",
			tmp->pv);
	else
		upslogx(LOG_INFO, "UPS: %s (monitoring only)", tmp->sys);

	tmp->upsname = tmp->hostname = NULL;	

	if (upscli_splitname(tmp->sys, &tmp->upsname, &tmp->hostname, 
		&tmp->port) != 0) {
		upslogx(LOG_ERR, "Error: unable to split UPS name [%s]",
			tmp->sys);
	}

	if (!tmp->upsname)
		upslogx(LOG_WARNING, "Warning: UPS [%s]: no upsname set!",
			tmp->sys);
}		

static void set_notifymsg(const char *name, const char *msg)
{
	int	i;

	for (i = 0; notifylist[i].name != NULL; i++) {
		if (!strcasecmp(notifylist[i].name, name)) {

			/* only free if it's not the stock msg */
			if (notifylist[i].msg != notifylist[i].stockmsg)
				free(notifylist[i].msg);

			notifylist[i].msg = xstrdup(msg);
			return;
		}
	}

	upslogx(LOG_WARNING, "'%s' is not a valid notify event name\n", name);
}

static void set_notifyflag(const char *ntype, char *flags)
{
	int	i, pos;
	char	*ptr, *tmp;

	/* find ntype */

	pos = -1;
	for (i = 0; notifylist[i].name != NULL; i++) {
		if (!strcasecmp(notifylist[i].name, ntype)) {
			pos = i;
			break;
		}
	}

	if (pos == -1) {
		upslogx(LOG_WARNING, "Warning: invalid notify type [%s]\n", ntype);
		return;
	}

	ptr = flags;

	/* zero existing flags */
	notifylist[pos].flags = 0;

	while (ptr) {
		int	newflag;

		tmp = strchr(ptr, '+');
		if (tmp)
			*tmp++ = '\0';

		newflag = 0;

		if (!strcmp(ptr, "SYSLOG"))
			newflag = NOTIFY_SYSLOG;
		if (!strcmp(ptr, "WALL"))
			newflag = NOTIFY_WALL;
		if (!strcmp(ptr, "EXEC"))
			newflag = NOTIFY_EXEC;
		if (!strcmp(ptr, "IGNORE"))
			newflag = NOTIFY_IGNORE;

		if (newflag)
			notifylist[pos].flags |= newflag;
		else
			upslogx(LOG_WARNING, "Invalid notify flag: [%s]\n", ptr);

		ptr = tmp;
	}
}

/* in split mode, the parent doesn't hear about reloads */
static void checkmode(char *cfgentry, char *oldvalue, char *newvalue, 
			int reloading)
{
	/* nothing to do if in "all as root" mode */
	if (use_pipe == 0)
		return;

	/* it's ok if we're not reloading yet */
	if (reloading == 0)
		return;

	/* also nothing to do if it didn't change */
	if ((oldvalue) && (newvalue)) {
		if (!strcmp(oldvalue, newvalue))
			return;
	}

	/* otherwise, yell at them */
	upslogx(LOG_WARNING, "Warning: %s redefined in split-process mode!",
		cfgentry);
	upslogx(LOG_WARNING, "You must restart upsmon for this change to work");
}

/* returns 1 if used, 0 if not, so we can complain about bogus configs */
static int parse_conf_arg(int numargs, char **arg)
{
	/* using up to arg[1] below */
	if (numargs < 2)
		return 0;

	/* SHUTDOWNCMD <cmd> */
	if (!strcmp(arg[0], "SHUTDOWNCMD")) {
		checkmode(arg[0], shutdowncmd, arg[1], reload_flag);

		if (shutdowncmd)
			free(shutdowncmd);
		shutdowncmd = xstrdup(arg[1]);
		return 1;
	}

	/* POWERDOWNFLAG <fn> */
	if (!strcmp(arg[0], "POWERDOWNFLAG")) {
		checkmode(arg[0], powerdownflag, arg[1], reload_flag);

		if (powerdownflag)
			free(powerdownflag);

		powerdownflag = xstrdup(arg[1]);

		if (!reload_flag)
			upslogx(LOG_INFO, "Using power down flag file %s\n",
				arg[1]);

		return 1;
	}		

	/* NOTIFYCMD <cmd> */
	if (!strcmp(arg[0], "NOTIFYCMD")) {
		if (notifycmd)
			free(notifycmd);

		notifycmd = xstrdup(arg[1]);
		syslog(LOG_INFO,"Config Load: NOTIFYCMD set to %s", arg[1]);
		return 1;
	}

	/* POLLFREQ <num> */
	if (!strcmp(arg[0], "POLLFREQ")) {
		pollfreq = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: POLLFREQ set to %s", arg[1]);
		return 1;
	}

	/* POLLFREQALERT <num> */
	if (!strcmp(arg[0], "POLLFREQALERT")) {
		pollfreqalert = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: POLLFREQALERT set to %s", arg[1]);
		return 1;
	}

	/* HOSTSYNC <num> */
	if (!strcmp(arg[0], "HOSTSYNC")) {
		hostsync = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: HOSTSYNC set to %s", arg[1]);
		return 1;
	}

	/* DEADTIME <num> */
	if (!strcmp(arg[0], "DEADTIME")) {
		deadtime = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: DEADTIME set to %s", arg[1]);
		return 1;
	}

	/* MINSUPPLIES <num> */
	if (!strcmp(arg[0], "MINSUPPLIES")) {
		minsupplies = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: MINSUPPLIES set to %s", arg[1]);
		return 1;
	}

	/* RBWARNTIME <num> */
	if (!strcmp(arg[0], "RBWARNTIME")) {
		rbwarntime = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: RBWARNTIME set to %s", arg[1]);
		return 1;
	}

	/* NOCOMMWARNTIME <num> */
	if (!strcmp(arg[0], "NOCOMMWARNTIME")) {
		nocommwarntime = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: NOCOMMWARNTIME set to %s", arg[1]);
		return 1;
	}

	/* FINALDELAY <num> */
	if (!strcmp(arg[0], "FINALDELAY")) {
		finaldelay = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: FINALDELAY set to %s", arg[1]);
		return 1;
	}

	/* RUN_AS_USER <userid> */
 	if (!strcmp(arg[0], "RUN_AS_USER")) {
		if (run_as_user)
			free(run_as_user);

		run_as_user = xstrdup(arg[1]);
		syslog(LOG_INFO,"Config Load: RUN_AS_USER set to %s", arg[1]);
		return 1;
	}

	/* CERTPATH <path> */
	if (!strcmp(arg[0], "CERTPATH")) {
		if (certpath)
			free(certpath);

		certpath = xstrdup(arg[1]);
		syslog(LOG_INFO,"Config Load: CERTPATH set to %s", arg[1]);
		return 1;
	}

	/* CERTVERIFY (0|1) */
	if (!strcmp(arg[0], "CERTVERIFY")) {
		certverify = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: CERTVERIFY set to %s", arg[1]);
		return 1;
	}

	/* FORCESSL (0|1) */
	if (!strcmp(arg[0], "FORCESSL")) {
		forcessl = atoi(arg[1]);
		syslog(LOG_INFO,"Config Load: FORCESSL set to %s", arg[1]);
		return 1;
	}

	/* using up to arg[2] below */
	if (numargs < 3)
		return 0;

	/* NOTIFYMSG <notify type> <replacement message> */
	if (!strcmp(arg[0], "NOTIFYMSG")) {
		set_notifymsg(arg[1], arg[2]);
		syslog(LOG_INFO,"Config Load: NOTIFYMSG set to %s : %s", arg[1],arg[2]);
		return 1;
	}

	/* NOTIFYFLAG <notify type> <flags> */
	if (!strcmp(arg[0], "NOTIFYFLAG")) {
		set_notifyflag(arg[1], arg[2]);
		syslog(LOG_INFO,"Config Load: NOTIFYFLAG set to %s : %s", arg[1],arg[2]);
		return 1;
	}	

	/* using up to arg[4] below */
	if (numargs < 5)
		return 0;

	if (!strcmp(arg[0], "MONITOR")) {

		/* original style: no username (only 5 args) */
		if (numargs == 5) {
			upslogx(LOG_ERR, "Unable to use old-style MONITOR line without a username");
			upslogx(LOG_ERR, "Convert it and add a username to upsd.users - see the documentation");

			fatalx("Fatal error: unusable configuration");
		}

		/* <sys> <pwrval> <user> <pw> ("master" | "slave") */
		addups(reload_flag, arg[1], arg[2], arg[3], arg[4], arg[5]);
		return 1;
	}

	/* didn't parse it at all */
	return 0;
}

/* called for fatal errors in parseconf like malloc failures */
static void upsmon_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(upsmon.conf): %s", errmsg);
}

static void loadconfig(void)
{
	char	fn[SMALLBUF];
	PCONF_CTX	ctx;

	strncpy(fn,s_curConfig.confPath,sizeof(fn));

	pconf_init(&ctx, upsmon_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		if (reload_flag == 1) {
			upslog(LOG_ERR, "Reload failed: %s", ctx.errmsg);
			return;
		}

		fatalx("%s", ctx.errmsg);
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_conf_arg(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg), 
				"upsmon.conf line %d: invalid directive",
				ctx.linenum);

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s", 
					ctx.arglist[i]);

			upslogx(LOG_WARNING, errmsg);
		}
	}

	pconf_finish(&ctx);		
}

static void ups_free(utype *ups)
{
	if (ups->sys)
		free(ups->sys);
	if (ups->upsname)
		free(ups->upsname);
	if (ups->hostname)
		free(ups->hostname);
	if (ups->un)
		free(ups->un);
	if (ups->pw)
		free(ups->pw);
	free(ups);
}

static void upsmon_cleanup(void)
{
	int	i;
	utype	*utmp, *unext;

	/* close all fds */
	utmp = firstups;

	while (utmp) {
		unext = utmp->next;

		upscli_disconnect(&utmp->conn);
		ups_free(utmp);

		utmp = unext;
	}

	if (run_as_user)
		free(run_as_user);
	if (shutdowncmd)
		free(shutdowncmd);
	if (notifycmd)
		free(notifycmd);
	if (powerdownflag)
		free(powerdownflag);

	for (i = 0; notifylist[i].name != NULL; i++)
		if (notifylist[i].msg != notifylist[i].stockmsg)
			free(notifylist[i].msg);
}

static void user_fsd(int sig)
{
	upslogx(LOG_INFO, "Signal %d: User requested FSD", sig);
	userfsd = 1;
}

static void set_reload_flag(int sig)
{
	reload_flag = 1;
}

/* remember the last time the ups was not critical (OB + LB) */
static void update_crittimer(utype *ups)
{
	/* if !OB, then it's on line power, so log the time */
    if (!flag_isset(ups->status, NUT_ST_ONBATT)) 
	{
		time(&ups->lastonlinepower);
    }

	/* if !OB or !LB, then it's not critical, so log the time */
	if ((!flag_isset(ups->status, NUT_ST_ONBATT)) || 
		(!flag_isset(ups->status, NUT_ST_LOWBATT))) {

		time(&ups->lastnoncrit);
		return;
	}

	/* fallthrough: let the timer age */
}

static int try_ssl(utype *ups)
{
	int	ret;

	/* if not doing SSL, we're done */
	if (!upscli_ssl(&ups->conn))
		return 1;

	if (!certpath) {
		if (certverify == 1) {
			upslogx(LOG_ERR, "Configuration error: "
				"CERTVERIFY is set, but CERTPATH isn't");
			upslogx(LOG_ERR, "UPS [%s]: Connection impossible, "
				"dropping link", ups->sys);

			ups_is_gone(ups);
			drop_connection(ups);

			return 0;	/* failed */
		}

		/* certverify is 0, so just warn them and return */
		upslogx(LOG_WARNING, "Certificate verification is disabled");
		return 1;
	}

	/* you REALLY should set CERTVERIFY to 1 if using SSL... */
	if (certverify == 0)
		upslogx(LOG_WARNING, "Certificate verification is disabled");

	ret = upscli_sslcert(&ups->conn, NULL, certpath, certverify);

	if (ret < 0) {
		upslogx(LOG_ERR, "UPS [%s]: SSL certificate set failed: %s",
			ups->sys, upscli_strerror(&ups->conn));

		ups_is_gone(ups);
		drop_connection(ups);

		return 0;
	}

	return 1;
}

/* handle connecting to upsd, plus get SSL going too if possible */
static int try_connect(utype *ups)
{
	int	flags, ret;

	debug("Trying to connect to UPS [%s]\n", ups->sys);

	clearflag(&ups->status, NUT_ST_CONNECTED);

	/* force it if configured that way, just try it otherwise */
	if (forcessl == 1) 
		flags = UPSCLI_CONN_REQSSL;
	else
		flags = UPSCLI_CONN_TRYSSL;

	ret = upscli_connect(&ups->conn, ups->hostname, ups->port, flags);

	if (ret < 0) {
		upslogx(LOG_ERR, "UPS [%s]: connect failed: %s",
			ups->sys, upscli_strerror(&ups->conn));

		ups_is_gone(ups);
		return 0;
	}

	ret = try_ssl(ups);

	if (ret == 0)
		return 0;	/* something broke while trying SSL */

	/* we're definitely connected now */
	setflag(&ups->status, NUT_ST_CONNECTED);

	/* now try to authenticate to upsd */

	ret = do_upsd_auth(ups);

	if (ret == 1)
		return 1;		/* everything is happy */

	/* something failed in the auth so we may not be completely logged in */

	/* FUTURE: do something beyond the error msgs from do_upsd_auth? */

	return 0;
}

/* deal with the contents of STATUS or ups.status for this ups */
static void parse_status(utype *ups, char *status)
{
	char	*statword, *ptr;

	debug("     status: [%s]\n", status);

	/* empty response is the same as a dead ups */
	if (!strcmp(status, "")) {
		ups_is_gone(ups);
		return;
	}

	ups_is_alive(ups);

	/* clear these out early if they disappear */
	if (!strstr(status, "LB"))
		clearflag(&ups->status, NUT_ST_LOWBATT);
	if (!strstr(status, "FSD"))
		clearflag(&ups->status, NUT_ST_FSD);

	statword = status;

	/* split up the status words and parse each one separately */
	while (statword != NULL) {
		ptr = strchr(statword, ' ');
		if (ptr)
			*ptr++ = '\0';

		debug("    parsing: [%s]: ", statword);

		if (!strcasecmp(statword, "OL"))
			ups_on_line(ups);
		if (!strcasecmp(statword, "OB"))
			ups_on_batt(ups);
		if (!strcasecmp(statword, "LB"))
			ups_low_batt(ups);
		if (!strcasecmp(statword, "RB"))
			upsreplbatt(ups);

		/* do it last to override any possible OL */
		if (!strcasecmp(statword, "FSD"))
			ups_fsd(ups);

		update_crittimer(ups);

		statword = ptr;
	} 

	debug("\n");
}

/* see what the status of the UPS is and handle any changes */
static void pollups(utype *ups)
{
	char	status[SMALLBUF];

	HANDLE hThread = NULL;
	DWORD threadID = 0;
	HANDLE hTimeoutEvent = NULL;
	upscli_getvarThreadArgs callargs;
	DWORD waitRetVal = 0;


	/* try a reconnect here */
	if (!flag_isset(ups->status, NUT_ST_CONNECTED))
		if (try_connect(ups) != 1)
			return;

	if (upscli_ssl(&ups->conn) == 1)
		debug("polling ups: %s [SSL]\n", ups->sys);
	else
		debug("polling ups: %s\n", ups->sys);

	hTimeoutEvent = CreateEvent(NULL,TRUE,FALSE,UPSCLI_GETVAR_EVENT_NAME);
	ResetEvent(hTimeoutEvent);

	/* setup the structure with the params to pass to the thread */
	callargs.ups = ups;
	callargs.varname = "status";
	callargs.buf = status;
	callargs.buflen = sizeof(status);
	callargs.hTimeoutEvent = hTimeoutEvent;
	syslog(LOG_DEBUG,"pollups: creating thread");
	hThread = CreateThread(NULL,0,upscli_getvarThread,&callargs,0,&threadID);
	syslog(LOG_DEBUG,"pollups: waiting for event");
	waitRetVal = WaitForSingleObject(hTimeoutEvent,NET_TIMEOUT_LEN*1000);
	syslog(LOG_DEBUG,"UPS Status [%s]",status);  //this will make your logs quite large...
	syslog(LOG_DEBUG,"pollups: event occured");
	if (   (waitRetVal != WAIT_TIMEOUT) 
		&& (callargs.retval >= 0))
	{
		parse_status(ups, status);
	}
	else
	{					/* fetch failed */
		if(waitRetVal == WAIT_TIMEOUT)
		{
			syslog(LOG_DEBUG,"  Fetch Failed: upscli_getvarThread thread timed out");
		}
		else
		{
			/* try to make some of these a little friendlier */

			switch (upscli_upserror(&ups->conn)) {

				case UPSCLI_ERR_UNKNOWNUPS:
					upslogx(LOG_ERR, "Poll UPS [%s] failed - [%s] "
					"does not exist on server %s", 
					ups->sys, ups->upsname,	ups->hostname);

					break;
				default:
					upslogx(LOG_ERR, "Poll UPS [%s] failed - %s", 
						ups->sys, upscli_strerror(&ups->conn));
					break;
			}

			/* throw COMMBAD or NOCOMM as conditions may warrant */
			ups_is_gone(ups);

			/* if upsclient lost the connection, clean up things on our side */
			if (upscli_fd(&ups->conn) == -1) {
				drop_connection(ups);
			}
		}
	}
	CloseHandle(hTimeoutEvent);
	CloseHandle(hThread);
}

/* see if the powerdownflag file is there and proper */
static int pdflag_status(void)
{
	FILE	*pdf;
	char	buf[SMALLBUF];

	if (!powerdownflag)
		return 0;	/* unusable */

	pdf = fopen(powerdownflag, "r");

	if (pdf == NULL)
		return 0;	/* not there */

	/* if it exists, see if it has the right text in it */

	fgets(buf, sizeof(buf), pdf);
	fclose(pdf);

	/* reasoning: say upsmon.conf is world-writable (!) and some nasty
	 * user puts something "important" as the power flag file.  This 
	 * keeps upsmon from utterly trashing it when starting up or powering
	 * down at the expense of not shutting down the UPS.
	 *
	 * solution: don't let mere mortals edit that configuration file.
	 */

	if (!strncmp(buf, SDMAGIC, strlen(SDMAGIC)))
		return 1;	/* exists and looks good */

	return -1;	/* error: something else is in there */
}	

/* only remove the flag file if it's actually from us */
static void clear_pdflag(void)
{
	int	ret;

	ret = pdflag_status();

	if (ret == -1)  {
		upslogx(LOG_ERR, "POWERDOWNFLAG (%s) does not contain"
			"the upsmon magic string - disabling!", powerdownflag);
		powerdownflag = NULL;
		return;
	}

	/* it's from us, so we can remove it */
	if (ret == 1)
		unlink(powerdownflag);
}

/* exit with success only if it exists and is proper */
static int check_pdflag(void)
{
	int	ret;

	ret = pdflag_status();

	if (ret == -1) {
		upslogx(LOG_ERR, "POWERDOWNFLAG (%s) does not contain "
			"the upsmon magic string");
		return EXIT_FAILURE;
	}

	if (ret == 0) {
		/* not there - this is not a shutdown event */
		printf("Power down flag is not set\n");
		return EXIT_FAILURE;
	}

	if (ret != 1) {
		upslogx(LOG_ERR, "Programming error: pdflag_status returned %d",
			ret);
		return EXIT_FAILURE;
	}

	/* only thing left - must be time for a shutdown */
	printf("Power down flag is set\n");
	return EXIT_SUCCESS;
}

/* set all the notify values to a default */
static void initnotify(void)
{
	int	i;

	for (i = 0; notifylist[i].name != NULL; i++) {
		notifylist[i].flags = NOTIFY_SYSLOG | NOTIFY_WALL;
		notifylist[i].msg = notifylist[i].stockmsg;
	}
}


static void delete_ups(utype *target)
{
	utype	*ptr, *last;

	if (!target)
		return;

	ptr = last = firstups;

	while (ptr) {
		if (ptr == target) {
			upslogx(LOG_NOTICE, "No longer monitoring UPS [%s]\n",
				target->sys);

			/* disconnect cleanly */
			drop_connection(ptr);

			/* about to delete the first ups? */
			if (ptr == last)
				firstups = ptr->next;
			else
				last->next = ptr->next;

			/* release memory */

			ups_free(ptr);

			return;
		}

		last = ptr;
		ptr = ptr->next;
	}

	/* shouldn't happen */
	upslogx(LOG_ERR, "delete_ups: UPS not found\n");
}	

/* see if we can open a file */
static int check_file(const char *fn)
{
	char	chkfn[SMALLBUF];
	FILE	*f;

	//snprintf(chkfn, sizeof(chkfn), "%s/%s", confpath(), fn);
	strncpy(chkfn,s_curConfig.confPath,sizeof(chkfn));

	f = fopen(chkfn, "r");

	if (!f) {
		upslog(LOG_ERR, "Reload failed: can't open %s", chkfn);
		return 0;	/* failed */
	}

	fclose(f);
	return 1;	/* OK */
}

static void reload_conf(void)
{
	utype	*tmp, *next;

	upslogx(LOG_INFO, "Reloading configuration");

	/* sanity check */
	if (!check_file("upsmon.conf")) {
		reload_flag = 0;
		return;
	}

	/* flip through ups list, clear retain value */
	tmp = firstups;

	while (tmp) {
		tmp->retain = 0;
		tmp = tmp->next;
	}

	/* reset paranoia checker */
	totalpv = 0;

	/* reread upsmon.conf */
	loadconfig();

	/* go through the utype struct again */
	tmp = firstups;

	while (tmp) {
		next = tmp->next;

		/* !retain means it wasn't in the .conf this time around */
		if (tmp->retain == 0)
			delete_ups(tmp);

		tmp = next;
	}

	/* see if the user just blew off a foot */
	if (totalpv < minsupplies) {
		upslogx(LOG_CRIT, "Fatal error: total power value (%d) less "
			"than MINSUPPLIES (%d)", totalpv, minsupplies);

		fatalx("Impossible power configuation, unable to continue");
	}

	/* finally clear the flag */
	reload_flag = 0;
}

void doMonitorPreConfig()
{
	syslog(LOG_NOTICE,"Network UPS Tools upsmon %s", UPS_VERSION);

	initnotify();
	loadconfig();

	syslog(LOG_DEBUG,"Finished Loading Configuration files");
	if (totalpv < minsupplies) {
		syslog(LOG_CRIT,"Fatal error: insufficient power configured!");
		syslog(LOG_CRIT,"Sum of power values........: %d", totalpv);
		syslog(LOG_CRIT,"Minimum value (MINSUPPLIES): %d", minsupplies);
		syslog(LOG_CRIT,"\nEdit your upsmon.conf and change the values.");
		MessageBox(NULL,"Fatal error: insufficient power configured! Edit your configuration and change the values","WinNUT Ups Monitor Critical Error",MB_OK);
		exit(-1);
	}
}

int monitor()
{
	int eventCode = EVENT_NONE;

	for (;;) {
		utype	*ups;

		/* check flags from signal handlers */
		if (userfsd)
			forceshutdown();

		if (reload_flag)
			reload_conf();


		sleepval = pollfreq;
		for (ups = firstups; ups != NULL; ups = ups->next)
			pollups (ups);

		recalc();
		eventCode = handleEvents();
		//This polls to see if the service / application is being shutdown more often to prevent us dying without closing connections
		if(eventCode != EVENT_STOP)
		{
			int timer = 0;
			for(timer = 0; timer < sleepval*1000; timer +=200)
			{
				eventCode = handleEvents(); /* this sleeps for 100 ms also */
				if(eventCode == EVENT_STOP)
					break;
				Sleep (100);
			}
		}

		if(   (eventCode == EVENT_STOP)
		   || (s_poweringDownFlag != 0))
		{
			/* we've been requested to stop the service or shutdown the application*/
			break;
		}
	}
	/* we need to continue polling UPSs to let them know we're still alive to prevent premature power off, 
	 * When we receive the EVENT_STOP, this means that either we are in service mode and it has been 
	 * requested to stop (probably due to NT shutting down services for power off) or if we are running
	 * in application mode, we've received the WM_DESTROY message which means we should be shutting down
	 * and thusly we stop polling at which 
	 * point upsd should recognise we've disconnected.  We don't recalc because there's no
	 * turning back at this point.
	 * IF RUNNING IN APPLICATION MODE: this will loop forever until NT kills it as part of the
	 * shutdown process, again this will keep upsd informed of us until we're pretty close to 
	 * being done with shutdown
	 */
	while(eventCode != EVENT_STOP)
	{
		int timer = 0;
		utype	*ups;
		for (ups = firstups; ups != NULL; ups = ups->next)
			pollups (ups);
		eventCode = handleEvents();
		if(eventCode != EVENT_STOP)
		{
			for(timer = 0; timer < sleepval*1000; timer+=200)
			{
				eventCode = handleEvents(); /* this sleeps for 100 ms also */
				if(eventCode == EVENT_STOP)
					break;
				Sleep (100);
			}
		}
	}

	return (1);
}

void logoffAllUPSs()
{
	utype *ups = firstups;
	syslog(LOG_INFO,"WinNUT received STOP command");
	/*createNTEventLogEntry(MSG_GENERAL_INFORMATION,"Stopping monitoring service");*/
	while(ups)
	{
		/* drop the connection if we had ever polled it */
		if (flag_isset(ups->status, NUT_ST_CONNECTED)) {
			drop_connection(ups);
		}

		ups = ups->next;
	}
}

