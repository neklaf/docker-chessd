INSERT INTO ChessdUser (adminlevel, deleted, username, fullname, passwd, emailaddress, subscriptiondate)
				VALUES (100, false, 'admin', '@adminname@', md5('@adminpass@'), '@adminemail@', NOW());

insert into GameType values (0,'TYPE_UNTIMED');
insert into GameType values	(1,'TYPE_BLITZ');
insert into GameType values	(2,'TYPE_STAND');
insert into GameType values	(3,'TYPE_NONSTANDARD');
insert into GameType values	(4,'TYPE_WILD');
insert into GameType values	(5,'TYPE_LIGHT');
insert into GameType values	(6,'TYPE_BUGHOUSE');

INSERT INTO StatusType VALUES (0,'GAME_EMPTY');
INSERT INTO StatusType VALUES (1,'GAME_NEW');
INSERT INTO StatusType VALUES (2,'GAME_ACTIVE');
INSERT INTO StatusType VALUES (3,'GAME_EXAMINE');
INSERT INTO StatusType VALUES (4,'GAME_SETUP');

INSERT INTO ResultType VALUES (0,'END_CHECKMATE');
INSERT INTO ResultType VALUES (1,'END_RESIGN');
INSERT INTO ResultType VALUES (2,'END_FLAG');
INSERT INTO ResultType VALUES (3,'END_AGREEDDRAW');
INSERT INTO ResultType VALUES (4,'END_REPETITION');
INSERT INTO ResultType VALUES (5,'END_50MOVERULE');
INSERT INTO ResultType VALUES (6,'END_ADJOURN');
INSERT INTO ResultType VALUES (7,'END_LOSTCONNECTION');
INSERT INTO ResultType VALUES (8,'END_ABORT');
INSERT INTO ResultType VALUES (9,'END_STALEMATE');
INSERT INTO ResultType VALUES (10,'END_NOTENDED');
INSERT INTO ResultType VALUES (11,'END_COURTESY');
INSERT INTO ResultType VALUES (12,'END_BOTHFLAG');
INSERT INTO ResultType VALUES (13,'END_NOMATERIAL');
INSERT INTO ResultType VALUES (14,'END_FLAGNOMATERIAL');
INSERT INTO ResultType VALUES (15,'END_ADJDRAW');
INSERT INTO ResultType VALUES (16,'END_ADJWIN');
INSERT INTO ResultType VALUES (17,'END_ADJABORT');
INSERT INTO ResultType VALUES (18,'END_COURTESYADJOURN');

INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('default_prompt', 0, 0, 'chessd2%', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('guest_login', 1, 0, 'guest', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('head_admin', 2, 0, 'admin', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('head_admin_email', 3, 0, '@adminemail@', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('registration_address', 4, 0, '@regaddress@', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('server_hostname', 5, 0, '@serveraddress@', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('server_location', 6, 0, '', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('server_name', 7, 0, '@servername@', 0);

INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('default_time', 0, 1, '', 2);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('default_rating', 1, 1, '', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('default_rd', 2, 1, '', 350);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('default_increment', 3, 1, '', 12);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('idle_timeout', 4, 1, '', 3600);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('guest_prefix_only', 5, 1, '', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('login_timeout', 6, 1, '', 300);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('max_aliases', 7, 1, '', 27);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('max_players', 8, 1, '', 5000);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('max_players_unreg', 9, 1, '', 4000);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('max_sought', 10, 1, '', 1000);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('max_user_list_size', 11, 1, '', 10);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue)
					VALUES ('web_welcome_title', 22, 0, 'Welcome to @servername@', 0);
INSERT INTO ConfigVariable (Name, EnumValue, VarType, StrValue, IntValue) 
					VALUES ('web_welcome_text', 23, 0, 
		'Welcome to @servername,@chessd-chrysallis server @version@ @ATserveraddress@', 0);
