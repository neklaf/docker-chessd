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
*/
	include "funcs.php";
	
	function config($v){
		if($res = qry("SELECT strvalue FROM ConfigVariable WHERE name='$v'"))
			return oneresult($res);
		return false;
	}
	
	if(dbconnect()){
		$admin = config("head_admin_email");
		$servername = config("server_name");
		$welcome = config("web_welcome_title");
		if($res = qry("SELECT COUNT(*) FROM ChessdUser WHERE NOT deleted"))
			$regusers = oneresult($res);
		if($res = qry("SELECT username FROM ActiveUser"))
			while($r = fetch($res))
				$actusers[] = $r[username];
	}else{
		$welcome = $servername = "chessd";
	}
?>
<html>

<head> <title><? echo $welcome; ?></title> </head>

<body>
	<h2><? echo $servername; ?></h2>
	<p>
	<?
		if($regusers) echo "$regusers registered users";
		if($actusers) echo "<br>Logged on: <b>", join(", ",$actusers), "</b>";
	?>
	<p>
	<li><a href="reg.php">Register</a> yourself to play on this server</a>
	<p>
	<li><a href="http://chessd.sourceforge.net/index-en.php">Project home page</a>
	<li><a href="http://sourceforge.net/tracker/?group_id=86389&atid=579210">Report bugs</a>
	<p>
	<? if($admin) echo "<li>Contact <a href=\"mailto:$admin\">administrator</a>"; ?>
</body>

</html>

