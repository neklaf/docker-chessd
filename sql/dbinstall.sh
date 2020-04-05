#!/bin/bash

VERSION=0.2.2

function usage()
{
	echo "`basename $0`: setup database for chessd"
	echo "Usage: $0 [OPTIONS] <command>"
	echo "Available options, passed on to $DBCLIENT:"
	echo "	  -m	   your database server is MySQL (default PostgreSQL)"
	echo "	  -h HOST  connect to database on HOST instead of default localhost"
	echo "	  -p PORT  connect to port PORT"
	echo "	  -U USER  connect as user USERNAME (PostgreSQL only)"
	echo "	  -W	   force password prompt (PostgreSQL only)"
	echo "Please note that the above options refer to settings used to"
	echo "create the database, *not* during future chessd connections."
	echo "Further, TCP access as the newly-created user should be possible"
	echo "with the configuration in PostgreSQL's pg_hba.conf."
	echo
	echo "Available commands:"
	echo "    create:          create everything (db user, database and tables)"
	echo "    createdb:        create only the user and database"
	echo "    createtables:    create only the tables"
	echo "    droptables:      drop all tables"
	echo "    recreatetables:  drop and re-create all tables"
	echo
	echo "If PostgreSQL is running locally, the simplest way of using this"
	echo "script during install is to 'su' to a system user with"
	echo "PostgreSQL user/database creation powers (such as 'postgres' user)"
	echo "and run '$0 create'."
	exit 1
}

CREATETABLE=
DROPTABLE=
CREATEDB=

DBCLIENT=psql  # default, without -m option
while getopts "mh:p:WU:" option; do
	case $option in
	m) DBCLIENT=mysql ;;
	h) DBOPTS="$DBOPTS -h $OPTARG" ; DBHOST=$OPTARG ;;
	p) DBOPTS="$DBOPTS -p $OPTARG" ;;
	W) DBOPTS="$DBOPTS -W" ;;
	U) DBOPTS="$DBOPTS -U $OPTARG" ;;
	*) usage ;;
	esac
done
CREATESQL_IN="`dirname $0`/create-$DBCLIENT.sql"

shift $(($OPTIND - 1))
case $1 in
	create)			CREATEDB=yes; CREATETABLE=yes ;;
	createdb)		CREATEDB=yes ;;
	createtables)	CREATETABLE=yes ;;
	droptables)		DROPTABLE=yes ;;
	recreatetables)	CREATETABLE=yes; DROPTABLE=yes ;;
	*) usage;;
esac

if ! [ $DROPTABLE ] && ! [ $CREATETABLE ] && ! [ $CREATEDB ]; then
	usage
fi

if [ $DBCLIENT == mysql ] ; then
	echo --- chessd version $VERSION, MySQL install ---
	if [ $CREATEDB ] || [ $DROPTABLE ] ; then
		read -p "What's the name of an existing MySQL admin user? (enter for 'root') " MYADMINUSER
		let ${MYADMINUSER:=root}
		read -s -p "What's the password of MySQL user '$MYADMINUSER'? " MYADMINPASS
		echo
	fi
else
	echo --- chessd version $VERSION, PostgreSQL install ---
	which psql >/dev/null || echo "PostgreSQL doesn't seem to be installed, try the -m option to use MySQL."
fi

echo
read -p "What's the name of the chessd database? (enter for 'chessd') " CHESSDB
let ${CHESSDB:=chessd}
read -p "What is the chessd database user? (enter for 'chessd') " CHESSDUSER
let ${CHESSDUSER:=chessd}

if [ $CREATEDB ]; then
	while true; do
		echo
		read -s -p "What will be the initial password of database user '$CHESSDUSER'? " CHESSDPASS
		echo
		read -s -p "Please type the '$CHESSDUSER' password again, just to make sure: " TMP
		echo
		[ "$CHESSDPASS" == "$TMP" ] && break
	done
fi

if [ $CREATETABLE ]; then
	while true; do
		echo
		read -p "What is the chessd server name (any text, or hit enter to skip)? " SERVERNAME
		read -p "What is the server address/hostname (enter to skip)? " SERVERADDRESS
		read -p "What is the registration URL (enter to skip)? http://" REGADDRESS
		read -p "What is the admin's email (recommended)? " ADMINEMAIL
		read -p "What is the admin's full name (enter to skip)? " ADMINNAME
		echo
		read -s -p "What will be the initial password for chessd's admin user? " ADMINPASS
		echo
		read -s -p "Please type the admin password again, just to make sure: " TMP
		echo
		[ "$ADMINPASS" == "$TMP" ] && break
	done
fi

set -e

if [ $DROPTABLE ]; then
	echo
	echo "We'll now drop all chessd tables on database '$CHESSDB'."
	echo "All data, including user tables, will be permanently removed,"
	read -p "are you SURE?  Press y to continue, anything else to quit. " answer
	[ "$answer" == y ] || exit;
	awk '/^CREATE TABLE/ {print "DROP TABLE ",$3," CASCADE;"}' $CREATESQL_IN \
		| $DBCLIENT $DBOPTS ${MYADMINUSER:+ -u $MYADMINUSER} ${MYADMINPASS:+ -p$MYADMINPASS} "$CHESSDB"
	echo "...tables dropped."
fi

if [ $CREATEDB ]; then
	echo
	echo "We'll create the chessd user and database.  This step"
	echo "needs database and user creation permissions."

	if [ $DBCLIENT == "mysql" ] ; then
		mysqladmin create $CHESSDB $DBOPTS ${MYADMINUSER:+ -u $MYADMINUSER} ${MYADMINPASS:+ -p$MYADMINPASS}
		echo "...MySQL database $CHESSDB created."
		if [ $DBHOST ] ; then 
			echo "Warning: You specified the MySQL database server address ($DBHOST)."
			echo "         User '$CHESSDUSER' will be able to connect to that server from any host."
			FROMHOST="'%'"  # wildcard for connecting host
		else
			FROMHOST="localhost"
		fi
		mysql -e "GRANT ALL ON $CHESSDB.* TO $CHESSDUSER@$FROMHOST IDENTIFIED BY '$CHESSDPASS';" \
			$DBOPTS ${MYADMINUSER:+ -u $MYADMINUSER} ${MYADMINPASS:+ -p$MYADMINPASS}
		echo "...MySQL permissions granted for '$CHESSDUSER'."
	else
		[ $DBOPTS ] && echo "Options to psql will be: $DBOPTS"
		psql -c "CREATE USER $CHESSDUSER WITH NOCREATEDB NOCREATEUSER
					  ENCRYPTED PASSWORD '$CHESSDPASS';" $DBOPTS template1
		echo "...PostgreSQL user '$CHESSDUSER' created."
		psql -c "CREATE DATABASE $CHESSDB WITH OWNER $CHESSDUSER
					  ENCODING 'UNICODE';" $DBOPTS template1
		echo "...PostgreSQL database '$CHESSDB' created."
	fi
fi

if [ $CREATETABLE ]; then
	echo
	echo "We'll add the tables to the database next."

	echo "This step is done as database user '$CHESSDUSER',"
	echo "and you'll need to provide its password (twice)"
	[ $CREATEDB ] && echo "(which you just typed when you created the user)"
	if [ $DBCLIENT == "mysql" ] ; then
		DBOPTS="$DBOPTS -u $CHESSDUSER -p"
	else
		DBOPTS="$DBOPTS -U $CHESSDUSER -W"
		# force TCP, since ident will fail for chessd user
		[ $DBHOST ] || DBOPTS="$DBOPTS -h localhost"
	fi

	$DBCLIENT $DBOPTS $CHESSDB < $CREATESQL_IN
	echo "...tables created."
	
	# use '%' as regexp delimiter, since slashes may well occur in these strings.
	# (means '%' itself cannot be used; oh well)
	sed -e "s%@adminpass@%$ADMINPASS%" \
		-e "s%@servername@%$SERVERNAME%" \
		-e "s%@servername,@%${SERVERNAME:+$SERVERNAME, }%" \
		-e "s%@regaddress@%${REGADDRESS:+http://$REGADDRESS}%" \
		-e "s%@serveraddress@%$SERVERADDRESS%" \
		-e "s%@ATserveraddress@%${SERVERADDRESS:+running at $SERVERADDRESS}%" \
		-e "s%@adminemail@%$ADMINEMAIL%" \
		-e "s%@adminname@%$ADMINNAME%" \
		-e "s%@version@%$VERSION/$DBCLIENT%" \
		`dirname $0`/init.sql | $DBCLIENT $DBOPTS $CHESSDB
		
	echo "...initial data loaded."
	echo
	echo "Now edit the chessd configuration file (usually /usr/local/etc/chessd.conf)"
	echo "and start chessd (e.g. '/usr/local/bin/chessd')."
fi

echo "All done!"
