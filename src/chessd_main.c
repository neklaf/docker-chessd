
/*
   Copyright (c) 1993 Richard V. Nash.
   Copyright (c) 2000 Dan Papasian
   Copyright (C) Andrew Tridgell 2002
   Copyright (c) 2003 Federal University of Parana

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
  chessd_main.c is a minimal wrapper around chessd.so. By keeping this module to a
  minimum size we can reload nearly all the chess server functionality on a 'areload'
  command.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <string.h>
#include <ltdl.h>

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "utils.h"

#include "chessd_main.h"
#include "configuration.h"
#include "chessd_locale.h"
#include "command.h"
#include "common.h"
#include "timeseal.h"
#include "utils.h"
#include "globals.h"

#include "dbdbi.h"
#include "config_read.h"

#define DEBUG
/* handle used to talk to chessd.so */
static lt_dlhandle chessd_so_handle;

static void usage();
static void main_event_loop(tdatabase_info);

unsigned chessd_reload_flag;
char *current_locale;

/*
   load a function from chessd.so
   this is the core of the runtime reload functionality
*/
static lt_ptr *chessd_function(const char *name)
{
	lt_ptr *sym;

	if (chessd_so_handle == 0) {
		/* chessd_so_handle = dlopen(LIB_DIR "/chessd.so", RTLD_LAZY); */
		if (lt_dlinit() != 0) {
			fprintf(stderr, _("CHESSD: lt_dlinit() failed (%s)\n"),
					lt_dlerror());
			exit(1);
		}
		chessd_so_handle = lt_dlopen(LIB_DIR "/chessd.so");
		if (chessd_so_handle == NULL) {
			fprintf(stderr, _("CHESSD: Failed to load chessd.so !! (%s)\n"),
					lt_dlerror());
			exit(1);
		}
	}

	sym = lt_dlsym(chessd_so_handle, name);
	if (!sym) {
		fprintf(stderr, _("CHESSD: Failed to find symbol %s in chessd.so !!\n"),
				name);
		exit(1);
	}

	return sym;
}

static void usage()
{
	fprintf(stderr, _("Usage: chessd <options>\n"
			"    -p port     listen port for FICS protocol (default %d)\n"
			"    -b port     listen port for webnet protocol (default %d, 0 to disable)\n"
			"    -f          run in foreground (as opposed to daemon)\n"
			"    -T path     location of timeseal decoder binary\n"
			"    -R rootdir  run chroot'ed (chessd binary must be setuid root)\n"
			"    -t driver   libdbi driver type: 'pgsql', 'mysql', etc (default " DEFAULT_DB_DRIVER ")\n"
			"    -h host     database server host (default " DEFAULT_DB_HOST ")\n"
			"    -q port     database server port (default " DEFAULT_DB_PORT " for PgSQL; MySQL usually 3306)\n"
			"    -d name     database name (default " DEFAULT_DATABASE ")\n"
			"    -u user     database user (default " DEFAULT_DB_USER ")\n"
			"    -s passwd   database user password\n"
			"If given, options override parameters in chessd.conf.\n" ),
			DEFAULT_PORT, DEFAULT_WN_PORT);
	exit(1);
}

static int daemonize(void)
{
	pid_t pid;
	if ((pid = fork()) == -1)
		return -1;
	else if (pid != 0)
		exit(0);
	if (setsid() == -1)
		return -1;
	return 0;
}

static void main_event_loop(tdatabase_info dbinfo)
{
	void (*select_loop)(void) = (void*) chessd_function("select_loop");
	void (*wn_select_loop)(void) = (void*) chessd_function("wn_select_loop");
	int (*sql_connect_host)(tdatabase_info) = (void*) chessd_function("sql_connect_host");
	int (*sql_con_status)(void) = (void*) chessd_function("sql_con_status");
	int (*sys_announce)(int,int) = (void*) chessd_function("sys_announce");

	int justDisconnected = 1;

	for(;;) {
		select_loop();
		if(webnet_globals.sockfd != -1)
			wn_select_loop();

		// check the connection
		if (!sql_con_status()) {
			// lost connection, try to reconnect
			if (justDisconnected) {
				justDisconnected = 0;
				// send a message to everyone telling the server is not
				// connected to the database
				sys_announce(SYS_EVENT_DBDOWN, ADMIN_USER);
				sys_announce(SYS_EVENT_DBDOWN_ADM, ADMIN_ADMIN);
			}
			sql_connect_host(dbinfo);
			if (sql_con_status()) {
				justDisconnected = 1;
				// send a message to everyone telling the connection has
				// been restored
				sys_announce(SYS_EVENT_DBUP, ADMIN_USER);
			}
		}

		usleep(300000);

		// gabrielsan: (22/01/2004)
		// reload has been corrected to read from database
		/* check the reload flag */
		if (chessd_reload_flag) {
			void (*reload_close)(void) = (void*) chessd_function("reload_close");
			void (*reload_open)(void);

			chessd_reload_flag = 0;

			fprintf(stderr, _("CHESSD: Reloading server code!\n"));

			/* free up local vars */
			reload_close();

			/* close the handle to the shared lib */
			lt_dlclose(chessd_so_handle);

			chessd_so_handle = NULL;
            lt_dlexit();


			// reload the db shared functions
			sql_connect_host = (void*) chessd_function("sql_connect_host");
			sql_con_status = (void*) chessd_function("sql_con_status");

			/* reconnect database */
			sql_connect_host(dbinfo);

			/* re-initialise local variables */
			reload_open = (void*) chessd_function("reload_open");
			reload_open();


			// reload loop control shared functions
			select_loop = (void*) chessd_function("select_loop");
			wn_select_loop = (void*) chessd_function("wn_select_loop");
			sys_announce = (void*) chessd_function("sys_announce");
		}
	}
}

static void TerminateServer(int sig)
{
	void (*output_shut_mess)(void) = (void*) chessd_function("output_shut_mess");
	void (*TerminateCleanup)(void) = (void*) chessd_function("TerminateCleanup");
	void (*net_close)(void) = (void*) chessd_function("net_close");
	void (*sql_disconnect)(void) = (void*) chessd_function("sql_disconnect");

	// gabrielsan: function to shutdown the backdoor network
	void (*wn_net_close)(void) = (void*) chessd_function("wn_net_close");
	void (*InsertServerEvent)(int,int) = (void*) chessd_function("InsertServerEvent");
	void (*ClearServerActiveUsersAndGames)(void) =
		(void*) chessd_function("ClearServerActiveUsersAndGames");

	InsertServerEvent( seSERVER_SHUTDOWN, 0 );
	ClearServerActiveUsersAndGames();

	fprintf(stderr, _("CHESSD: Received signal %d\n"), sig);
	output_shut_mess();
	TerminateCleanup();
	sql_disconnect();
	net_close();
	if(webnet_globals.sockfd != -1)
		wn_net_close();
	exit(1);
}

static void BrokenPipe(int sig)
{
	int (*setPipeError)(int ) = (void*)chessd_function("setPipeError");

	signal(SIGPIPE, BrokenPipe);
	fprintf(stderr, _("CHESSD: Pipe signal\n"));

	setPipeError(-1);
}

/* this assumes we are setuid root to start */
static void do_chroot(const char *dir)
{
	uid_t uid = getuid(), euid = geteuid();
	struct rlimit rlp;

	if (euid != 0 || setuid(0) != 0) {
		fprintf(stderr, _("Must be setuid root to use -R\n"));
		exit(1);
	}
	if (chroot(dir) != 0 || chdir("/") != 0) {
		perror("chroot");
		exit(1);
	}
	if (setuid(uid) != 0) {
		perror("setuid");
		exit(1);
	}

	/* close all extraneous file descriptors */
	close(0);
	close(1);
	close(2);
	open("stdin.log", O_RDWR|O_CREAT|O_TRUNC, 0644);
	open("stdout.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
	open("stderr.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);

	/* reestabish core limits */
	getrlimit(RLIMIT_CORE, &rlp );
	rlp.rlim_cur = MAX(64*1024*1024, rlp.rlim_max);
	setrlimit( RLIMIT_CORE, &rlp );
}

/*
  give a decent backtrace on segv
*/
static void segv_handler(int sig)
{
        /* This is EVIL! */
	/*char cmd[100];*/

	/*snprintf(cmd, sizeof(cmd), "/home/fics/bin/backtrace %d > /home/fics/chessd/segv_%d 2>&1",
		 (int)getpid(), (int)getpid());
                 system(cmd);*/
	_exit(1);
}

// gabrielsan: Functions used to setup server configuration

// server configuration structure
typedef struct TServerConfig {
	int port, wn_port;
	int foreground;
	tdatabase_info dbinfo;
} TServerConfig;

// configuration key codes
enum config_key_number
	{CkPort,
	 CkWN_Port,
	 CkForeground,
	 CkDBName,
	 CkDBUser,
	 CkDBDriver,
	 CkDBHost,
	 CkDBPort,
	 CkDBPass,
	 CkNOP};

// list that relates config key codes to config key names
typedef struct {
	int configKNo;
	char *KName;
} tconfig_key_code;

static const tconfig_key_code config_key_name[] =
	{{CkPort, 		"port"},
	 {CkWN_Port, 	"wn_port"},
	 {CkForeground, "foreground"},
	 {CkDBName, 	"dbname"},
	 {CkDBUser, 	"dbuser"},
	 {CkDBDriver, 	"dbdriver"},
	 {CkDBHost, 	"dbhost"},
	 {CkDBPort,     "dbport"},
	 {CkDBPass, 	"dbpassword"},
	 {CkNOP,		NULL}};

int getKeyCode(char* keyname)
{
	const tconfig_key_code *p;

	for( p = config_key_name ; p->KName && strcasecmp(p->KName, keyname) ; ++p )
		;
	return p->configKNo;
}

/* read the server configuration from a file */
int readConfigFile(TServerConfig *SC)
{
	int (*numberstring)(char*) = (void*)chessd_function("numberstring");
	int (*read_config)(char*,config_key**) = (void*)chessd_function("read_config");
	config_key* conf_data;
	int nk, i;

	nk = read_config(DEFAULT_CONFIG, &conf_data);
    if (nk == -1)
        return -1;

	for (i=0; i < nk; i++) {
		char *value = conf_data[i].value;
		switch(getKeyCode(conf_data[i].name)) {
		case CkPort:
			if (numberstring(value))
				SC->port = atoi(value);
			break;
		case CkWN_Port:
			if (numberstring(value))
				SC->wn_port = atoi(value);
			break;
		case CkForeground:
			SC->foreground = GETBOOLEAN(value); // groks 0/1, true/false
			break;
		case CkDBName:
			free(SC->dbinfo.dbname);
			SC->dbinfo.dbname = strdup(value);
			break;
		case CkDBUser:
			free(SC->dbinfo.username);
			SC->dbinfo.username = strdup(value);
			break;
		case CkDBDriver:
			free(SC->dbinfo.driver);
			SC->dbinfo.driver = strdup(value);
			break;
		case CkDBHost:
			free(SC->dbinfo.host);
			SC->dbinfo.host = strdup(value);
			break;
        case CkDBPort:
            free(SC->dbinfo.port);
            SC->dbinfo.port = strdup(value);
            break;
		case CkDBPass:
			free(SC->dbinfo.password);
			SC->dbinfo.password = strdup(value);
			break;
		}
	}
	if (nk > 0)
		free(conf_data);
	return 0;
}

int ProcessCommandLine(int argc, char *argv[], TServerConfig *SC)
{
	int i;
	void (*timeseal_setpath)(const char*) =
		(void*) chessd_function("timeseal_setpath");
	void (*timeseal_init)() = (void*) chessd_function("timeseal_init");

	while((i = getopt(argc, argv, "p:b:fT:R:t:h:q:d:u:s:")) != -1) {
		switch(i) {
		case 'p': // server port
			SC->port = atoi(optarg);
			break;
		case 'b':
			SC->wn_port = atoi(optarg);
			break;
		case 'f': // whether it should run on foreground or not
			SC->foreground = 1;
			break;
		case 'T': // starts timeseal
			timeseal_setpath(optarg);
			timeseal_init();
			break;
		case 'R':
			do_chroot(optarg);
			break;

		// database configuration
		case 't': // database driver type
			free(SC->dbinfo.driver);
			SC->dbinfo.driver = strdup(optarg);
			break;
		case 'h': // host where database is located
			free(SC->dbinfo.host);
			SC->dbinfo.host = strdup(optarg);
			break;
		case 'q': // database server port
			free(SC->dbinfo.port);
			SC->dbinfo.port = strdup(optarg);
			break;
		case 'd': // database name
			free(SC->dbinfo.dbname);
			SC->dbinfo.dbname = strdup(optarg);
			break;
		case 'u': // dbuser
			free(SC->dbinfo.username);
			SC->dbinfo.username = strdup(optarg);
			break;
		case 's': // dbuser password
			free(SC->dbinfo.password);
			SC->dbinfo.password = strdup(optarg);
			break;

		default:
			usage();
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int (*net_init)(int) = (void*) chessd_function("net_init");
	int (*wn_net_init)(int) = (void*) chessd_function("wn_net_init");

	// gabrielsan: (22/01/2004)
	// reload fixed to read config from database
	void (*initial_load)(void) = (void*) chessd_function("initial_load");
	int (*sql_connect_host)(tdatabase_info) =
		(void*) chessd_function("sql_connect_host");

	// server configurations... now all in one structure! (well, almost)
	TServerConfig SC;

	setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

	/* enable malloc checking in libc */
	setenv("MALLOC_CHECK_", "2", 1);

	// fill in some defaults
	SC.port = DEFAULT_PORT;
	SC.wn_port = DEFAULT_WN_PORT;

	SC.dbinfo.driver = strdup(DEFAULT_DB_DRIVER);
	SC.dbinfo.host = strdup(DEFAULT_DB_HOST);
    SC.dbinfo.port = strdup(DEFAULT_DB_PORT);
	SC.dbinfo.dbname = strdup(DEFAULT_DATABASE);
	SC.dbinfo.username = strdup(DEFAULT_DB_USER);
	SC.dbinfo.password = strdup(DEFAULT_DB_USER_PWD);

	SC.foreground = 0;
	/* read configurations from file, if possible */
	if (readConfigFile(&SC) == -1) {
        fprintf(stderr, "Could not read config file %s\n", DEFAULT_CONFIG);
        fprintf(stderr, "Going with defaults (will probably not work)\n");
    }

	// process the command line.. the cmd line configuration
	// overrides the file configs
	ProcessCommandLine(argc, argv, &SC);

	fprintf(stderr, "Loaded Parameters:\n");
	fprintf(stderr, "  PORT: %d\n", SC.port );
	fprintf(stderr, "  WN_PORT: %d\n", SC.wn_port );
	fprintf(stderr, "  DBDRIVER: %s\n", SC.dbinfo.driver);
	fprintf(stderr, "  DBHOST: %s\n", SC.dbinfo.host);
	fprintf(stderr, "  DBPORT: %s\n", SC.dbinfo.port);
	fprintf(stderr, "  DB: %s\n", SC.dbinfo.dbname );
	fprintf(stderr, "  DBUSER: %s\n", SC.dbinfo.username);
	fprintf(stderr, "  DBPASS: %s\n", SC.dbinfo.password);

	if (!SC.foreground && daemonize()){
		printf(_("Problem with Daemonization - Aborting\n"));
		exit(1);
	}

	signal(SIGTERM, TerminateServer);
	signal(SIGSEGV, segv_handler);
	signal(SIGBUS,  segv_handler);
	signal(SIGINT,  TerminateServer);
	signal(SIGPIPE, BrokenPipe);

	if (net_init(SC.port)) {
		fprintf(stderr, _("CHESSD: Network initialize failed on port %d.\n"),
				SC.port);
		exit(1);
	}
	fprintf(stderr, _("CHESSD: Initialized FICS port %d\n"), SC.port);

	// Initializes the 'backdoor' network, used to implement a new protocol
	if(SC.wn_port){
		if(wn_net_init(SC.wn_port)) {
			fprintf(stderr, _("CHESSD: Webnet initialize failed on port %d.\n"),
					SC.wn_port);
			exit(1);
		}
		fprintf(stderr, _("CHESSD: Initialized Webnet port %d\n"), SC.wn_port);
	}else
		webnet_globals.sockfd = -1;

	/*
	 * We need connection to database to load configuration
	 */
	if(sql_connect_host(SC.dbinfo)){
		fprintf(stderr, _("CHESSD: Connected to %s database %s\n"), 
				SC.dbinfo.driver, SC.dbinfo.dbname);
	
		// gabrielsan: (22/01/2004)
		// reload corrected to read config from database
		initial_load();
	
		main_event_loop(SC.dbinfo);
		/* will never get here - uses TerminateServer */
	}
	return 1;
}
