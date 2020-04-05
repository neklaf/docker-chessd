<html>
<head>
<?
	$title = "chessd registration at $_SERVER[SERVER_NAME]"; 
	echo "<title>$title</title>\n";
?>
</head>
<body>
<?
/*
	Minimal registration form/script for chessd.

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

	Initial version 7 May, 2006.
	
	A more sophisticated script would:
	- be prettier in the browser
	- require response to a validation email before activating account
	- look up server address from config variable(?)
	- allow user to specify more data, such as gender, full name, birthday
	
	Email address is SQL-escaped to prevent injection attack (not needed for
	user name or password, since they are untainted by other means).
	For even more peace of mind, you can restrict MySQL privileges as follows:
		mysql> grant select,insert on chessd.ChessdUser to chessdreg@localhost identified by 'PUT-CORRECT-PASSWORD-HERE';
	...and set database user to 'chessdreg' below.
*/
	include "funcs.php";
	
	if(preg_match("/^[A-Za-z]\w{2,16}$/", $_POST[user]))
	{
		if(dbconnect())
		{
			if( ($res = qry("SELECT COUNT(*) FROM ChessdUser WHERE username='$_POST[user]'"))
					&& oneresult($res) )
				echo "<h3>Sorry, can't register that name</h3>\n",
					"<b>$_POST[user]</b> is taken, ",
					"please go back and try a different name.";
			else{
				if($res = qry("SELECT strvalue FROM ConfigVariable WHERE name='head_admin_email'"))
					$admin = oneresult($res);
				$pass = substr(md5($_POST[user].microtime()),0,6);
				
				if( qry("INSERT INTO ChessdUser (username, passwd, deleted, fullname, emailaddress, subscriptiondate)"
						. " VALUES ('$_POST[user]', MD5('$pass'), FALSE, "
						. "'".esc($_POST[fname])."', '".esc($_POST[email])."', NOW())") )
				{
					$subj = "$_POST[user] registered for chessd at $_SERVER[SERVER_NAME]";
					mail($_POST[email], $subj,
						  "Welcome to chessd.\n\n"
						. " Your login: $_POST[user]\n"
						. "   Password: $pass\n"
						. " To connect: telnet $_SERVER[SERVER_NAME] 5000\n"
						. "             (or use graphical FICS client)\n\n"
						. "Please reply to this email if you have any questions/problems.\n",
						$admin ? "From: $admin\nCC: $admin\n" : "");
					echo "<h3>$subj</h3>\n",
						"A password has been mailed to $_POST[email].\n",
						"<p>To play, connect via FICS protocol, e.g.\n",
						"<blockquote><tt><a href=\"telnet://$_SERVER[SERVER_NAME]:5000\">telnet $_SERVER[SERVER_NAME] 5000</a></tt></blockquote>\n",
						"or connect with a <a href=\"http://www.freechess.org/cgi-bin/Download/FICS_Download_Interface.cgi\">graphical FICS client</a> ",
						"such as <a href=\"http://www.tim-mann.org/xboard.html\">XBoard or WinBoard</a>.\n",
						($admin ? "<p>Contact chessd admin <a href=\"mailto:$admin\">$admin</a> concerning any questions/problems.\n" : "");
				}
			}
		}else
			echo "Can't connect to database.";
	}else{
?>
	<h2><? echo $title; ?></h2>

	<form method=POST>
		User name: <input name=user type=text size=25 maxsize=17>
			(3-17 letters or digits)
		<p>Full name: <input name=fname type=text size=25 maxsize=40>
			(optional)
		<p>Email address: <input name=email type=text size=25 maxsize=40>
			(Where initial random password will be sent.)
		<p><input type=submit>
	</form>
<?
	}
?>
</body>
</html>
