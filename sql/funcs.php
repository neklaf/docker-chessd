<?
/*
	Copyright (C) 2006 Toby Thain <toby@smartgames.ca>

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


	These definitions are in a separate file, to allow re-use by different
	scripts needing access to the chessd database.
*/

define(PGSQL, false); // set to false if using MySQL

define(DBHOST, 'localhost');
define(DBNAME, 'chessd');
define(DBUSER, 'chessd');
define(DBPASS, 'xxx');

// just enough abstraction to get us by (sigh...)

function dbconnect(){
	$conn = PGSQL ? pg_connect("host=".DBHOST." dbname=".DBNAME
							   ." user=".DBUSER." password=".DBPASS)
				  : mysql_connect(DBHOST,DBUSER,DBPASS);
	if(!PGSQL && !mysql_select_db(DBNAME))
		return false;
	return $conn;
}

function esc($s){
	return PGSQL ? pg_escape_string($s) : mysql_real_escape_string($s);
}

function qry($s){
	return PGSQL ? pg_query($s) : mysql_query($s);
}

function fetch($s){
	return PGSQL ? pg_fetch_array($s) : mysql_fetch_array($s);
}

function oneresult($s){
	return PGSQL ? pg_fetch_result($s,0,0) : mysql_result($s,0,0);
}

?>
