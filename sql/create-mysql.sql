/* This database schema is not good at all, see http://chessd.sf.net/olddb.html for comments.
 * The next version of chessd will feature a major database overhaul.
 */
/*
 * The server main administrator will be 'admin'.
 */

-- If you want constraints enforced, you need to use the InnoDB engine:
-- SET storage_engine=INNODB;

/*
Compatibility issue		PostgreSQL			MySQL
-------------------		----------			-----
BOOLEAN workaround		can use				use CHAR(1), BOOLEAN maps to TINYINT, can't be fetched as string
BOOLEAN reads as		string "f"/"t"		string "0"/"1"	(use GETBOOLEAN(str) macro to test)
BOOLEAN write as		TRUE/FALSE			TRUE/FALSE		(use PUTBOOLEAN(flag) macro, %s in libdbi template)
SERIAL workaround		can use (=INTEGER)	use INT AUTO_INCREMENT, SERIAL maps to BIGINT UNSIGNED and can't be fetched as int
TEXT reads as			string				binary
TEXT workaround			can use				use VARCHAR(N)
ILIKE					can use				n/a, try UPPER(a)=UPPER(b)
Subqueries				can use				not before v4.1
Constraints				can use				ignored unless using InnoDB engine

It would be nice if:
1. PostgreSQL BOOLEAN could be coercible to char/int
2. MySQL BIGINT could be coercible to narrower type
3. MySQL TEXT could be fetchable as string (and coercible to binary)
   I have raised this last issue on the libdbi-drivers mailing list.

libdbi seems to have a deliberate policy of not doing any coercions.
*/

CREATE TABLE Image (
	imageid 	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
--	data		BYTEA, /* FIXME: not a MySQL compatible type -- closest would be MEDIUMBLOB? */
	mimetype	VARCHAR(40),
	imgsize		INTEGER,
	width		INTEGER,
	height		INTEGER
);

CREATE TABLE ChessdUser (
	chessduserid	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	adminlevel		INTEGER,
	deleted			CHAR(1),
	username		VARCHAR(50) UNIQUE,
	passwd			VARCHAR(512),
	prompt			VARCHAR(256),
	emailaddress	VARCHAR(256),
	fullname		VARCHAR(128),
	lasthost		VARCHAR(16),
	lastopponent	VARCHAR(50),
	lasttell		VARCHAR(256),
	lastchannel		INTEGER,
	timeofreg		INTEGER,
	defaulttime		INTEGER,
	defaultinc		INTEGER,
	plan			VARCHAR(4096),
	formula			VARCHAR(512),
	formulalines	VARCHAR(4096),
	numformulalines INTEGER,
	availmax		INTEGER,
	availmin		INTEGER,
	defaultheight	INTEGER,
	defaultwidth	INTEGER,
	highlight		INTEGER,
	language		INTEGER,
	flags			INTEGER,
	numblack		INTEGER,
	numwhite		INTEGER,
	promote			INTEGER,
	totaltime		INTEGER,
	style			INTEGER,
	subscriptiondate TIMESTAMP,
	fotoid			INTEGER,
	status   		INTEGER,
	gender			CHAR,
	occupation		VARCHAR(255),
	birthday		TIMESTAMP,
	location		VARCHAR(255),
	whiteplayername	VARCHAR(50),
	blackplayername	VARCHAR(50),
	locale			VARCHAR(12),
	FOREIGN KEY (fotoid) REFERENCES Image(imageid)
);

CREATE TABLE GameType (
	gametypeid	INTEGER PRIMARY KEY,
	name		VARCHAR(50)
);

CREATE TABLE StatusType (
	statustypeid INTEGER PRIMARY KEY,
	name		VARCHAR(50)
);

CREATE TABLE ResultType (
	resulttypeid INTEGER PRIMARY KEY,
	name		VARCHAR(50)
);

CREATE TABLE Game (
	gameid 			INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	gametypeid  	INTEGER,
	gamestatusid	INTEGER,
	gameresultid	INTEGER,
	whiteplayerid	INTEGER,
	blackplayerid	INTEGER,
	whiterating		INTEGER,
	blackrating		INTEGER,
	winner			INTEGER,
	winittime		INTEGER,
	wincrement		INTEGER,
	binittime		INTEGER,
	bincrement		INTEGER,
	timeofstart		INTEGER,
	wtime			INTEGER,
	btime			INTEGER,
	rated			INTEGER,
	private			INTEGER,
	clockstopped	INTEGER,
	flag_check_time	INTEGER,
	passes			INTEGER,
	numhalfmoves	INTEGER,
	fenstartpos		VARCHAR(74),
	whiteplayername VARCHAR(50),
	blackplayername VARCHAR(50),
	adjudicatorname VARCHAR(50),
	adjudicatorid	INTEGER,

	FOREIGN KEY (whiteplayerid)	REFERENCES ChessdUser(chessduserid),
	FOREIGN KEY (blackplayerid)	REFERENCES ChessdUser(chessduserid),
	FOREIGN KEY (gametypeid)	REFERENCES GameType(gametypeid),
	FOREIGN KEY (gamestatusid)	REFERENCES StatusType(statustypeid),
	FOREIGN KEY (gameresultid)	REFERENCES ResultType(resulttypeid),
	FOREIGN KEY (adjudicatorid)	REFERENCES ChessdUser(chessduserid)
);

CREATE TABLE Move (
	moveid			INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	gameid			INTEGER,
	movorder		INTEGER,
	color			INTEGER,
	fromfile		INTEGER,
	fromrank		INTEGER,
	tofile			INTEGER,
	torank			INTEGER,
	doublepawn		INTEGER,
	movestring		VARCHAR(8),
	algstring		VARCHAR(8),
	fenpos			VARCHAR(64),
	attime			INTEGER,
	tooktime		INTEGER,

	FOREIGN KEY (GameID) REFERENCES Game(GameID)
);

CREATE TABLE UserCommandAlias (
	usercommandaliasid 	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ownerid				INTEGER,
	alias				VARCHAR(100),
	targetcommand		VARCHAR(100),

	FOREIGN KEY (OwnerID) REFERENCES ChessdUser(ChessdUserID)
);

CREATE TABLE UserCensor (
	usercensorid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ownerid				INTEGER,
	targetuser			VARCHAR(50),
	targetuserid		INTEGER,

	FOREIGN KEY (OwnerID) REFERENCES ChessdUser(ChessdUserID)
/*	FOREIGN KEY (targetUserID) REFERENCES ChessdUser(ChessdUserID) */
);

CREATE TABLE UserFollow (
	userfollowid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ownerid				INTEGER,
	targetuser			VARCHAR(50),
	targetuserid		INTEGER,

	FOREIGN KEY (OwnerID) REFERENCES ChessdUser(ChessdUserID)
/*	FOREIGN KEY (targetUserID) REFERENCES ChessdUser(ChessdUserID)*/
);

CREATE TABLE UserNoPlay (
	usernoplayid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ownerid				INTEGER,
	targetuser			VARCHAR(50),
	targetuserid		INTEGER,

	FOREIGN KEY (OwnerID) REFERENCES ChessdUser(ChessdUserID)
/*	FOREIGN KEY (targetUserID) REFERENCES ChessdUser(ChessdUserID)*/
);

CREATE TABLE UserGNotify (
	usergnotifyid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ownerid				INTEGER,
	targetuser			VARCHAR(50),
	targetuserid		INTEGER,

	FOREIGN KEY (OwnerID) REFERENCES ChessdUser(ChessdUserID)
/*	FOREIGN KEY (targetUserID) REFERENCES ChessdUser(ChessdUserID) */
);

CREATE TABLE UserNotify (
	usernotifyid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ownerid				INTEGER,
	targetuser			VARCHAR(50),
	targetuserid		INTEGER,

	FOREIGN KEY (OwnerID) REFERENCES ChessdUser(ChessdUserID)
/*	FOREIGN KEY (targetUserID) REFERENCES ChessdUser(ChessdUserID)*/
);

CREATE TABLE UserChannel (
	userchannelid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ownerid				INTEGER,
	channel				INTEGER,

	FOREIGN KEY (OwnerID) REFERENCES ChessdUser(ChessdUserID)
);

CREATE TABLE UserLogon (
	userlogonid			INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	userid				INTEGER,
	islogin				CHAR(1), /* if it is not a login, then it is a logout*/
	eventtime			TIMESTAMP,
	fromip				VARCHAR(16),
	whereat				INTEGER DEFAULT 1,

	FOREIGN KEY (UserID) REFERENCES ChessdUser(ChessdUserID)
);

CREATE TABLE UserStats (
	userstatsid			INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	userid				INTEGER,
	gametypeid			INTEGER,
	num					INTEGER,
	wins				INTEGER,
	losses				INTEGER,
	draws				INTEGER,
	rating				INTEGER,
	lasttime			INTEGER,	/* epoch */
	whenbest			INTEGER,	/* epoch */
	best				INTEGER,
	sterr				REAL,

	FOREIGN KEY (UserID) REFERENCES ChessdUser(ChessdUserID),
	FOREIGN KEY (GameTypeID) REFERENCES GameType(GameTypeID)
);

CREATE TABLE Message (
	messageid			INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	fromuserid			INTEGER,
	touserid			INTEGER,
	replytoid			INTEGER,
	replycount			INTEGER default 0,	/* for performance issues*/
	refertoid			INTEGER,
	subject				VARCHAR(200),
	status				SMALLINT,	/* READ(0)/NOT READ(1)*/
	messagetype			SMALLINT, 	/* news reply(3) /news(2)/comment(1)/message(0)*/
	content				VARCHAR(2048),
	posttime			TIMESTAMP,
	priority 			INTEGER DEFAULT 3,
	catid				INTEGER,
	fromname			VARCHAR(50),
	toname				VARCHAR(50),
	lastposterid		INTEGER,
	lastpostername		VARCHAR(50),
	lastposttime		TIMESTAMP /*WITHOUT TIME ZONE DEFAULT now()*/ /* MySQL incompatible syntax */
		/*	Field used mainly for news and articles.
			News priority 5 should be in a top article list. */
);

/*
	New table: MessageCategory
		Describes the category of a given message. The categories
		first specialized in type, been the type message it applies.
*/
CREATE TABLE Category (
	categoryid			INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	name				VARCHAR(50),
	description			VARCHAR(255),
	applytotype			INTEGER	/* to which type of message it
								   applies (e.g. help, news, faq, etc */
);
/* TODO: the above is still under evaluation  */

CREATE TABLE ConfigVariable (
	configvariableid	INTEGER AUTO_INCREMENT PRIMARY KEY,
	name				varchar(64),
	enumvalue			integer,
	vartype				integer,		/* 0=str, 1=int */
	intvalue			integer DEFAULT 0,
	strvalue			VARCHAR(200) DEFAULT ''
);

CREATE TABLE ActiveUser (
	activeuserid 	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	userid			INTEGER,
		/*	sometimes we have guests, so this cannot be linked as a
			foreign key; if userID == -1, then the user is a guest */
	username		varchar(255),
	sincewhen 		timestamp,
	activewhere		INTEGER,
		/* -1: WEBSITE, >=0 server and opens possibility of 'serverID'
			when (if) we have multiple servers */
	wherepage		varchar(255) DEFAULT NULL,
		/* which page the user is visiting */
	fromip			VARCHAR(16)
);

CREATE TABLE ServerEvent (
	servereventid	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	eventtype		INTEGER,
		/*
			0: server start
			1: server shutdown
			2: server reload
			3: user count change
			4: game count change
		 */
	int_parameter	INTEGER,
	eventtime		TIMESTAMP
);

CREATE TABLE ActiveGames (
	activegamesid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	timeofstart			TIMESTAMP,
	gametypeid			INTEGER,
	whiteplayerid		INTEGER,
	whiteplayername 	VARCHAR(50),
	blackplayerid		INTEGER,
	blackplayername 	VARCHAR(50)
);

CREATE TABLE ServerLists (
	itemid	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	listid	INTEGER,
	value	VARCHAR(255)
);

CREATE TABLE GameBookmark (
	userid	INTEGER,
	gameid	INTEGER,

	FOREIGN KEY (UserID) REFERENCES ChessdUser(ChessdUserID),
	FOREIGN KEY (GameID) REFERENCES Game(GameID)
);

CREATE TABLE ServerMessage (
    servermessageid	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name			character varying(32),
    locale			character varying(10),
    messagetext		VARCHAR(1024)
);

CREATE TABLE PageConfigVariable (
    pageconfigvariableid INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name				VARCHAR(128),
    value				VARCHAR(200)
);

CREATE TABLE AdminActionLog (
	adminactionlogid	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	action_type			INTEGER,
	fromuserid			INTEGER,
	fromname			VARCHAR(50),
	targetname			VARCHAR(50),
	eventtime			TIMESTAMP,
	xml_desc			VARCHAR(2048),
	fromip				VARCHAR(16)
);

CREATE TABLE TalkLog (
    talklogid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    action_type		INTEGER,
    fromuserid		INTEGER,
    fromname		VARCHAR(50),
    toname			VARCHAR(50),
    message			VARCHAR(2048),
    eventtime		TIMESTAMP,
    fromip			VARCHAR(16)
);


CREATE TABLE Variable (
    variableid		INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name			VARCHAR(50),
    tagtext			VARCHAR(8),
    readpermission	INTEGER,
    writepermission	INTEGER
);


CREATE TABLE UserVariable (
    uservaribleid	INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    varibleid		INTEGER,
    varname			VARCHAR(50),
    value			VARCHAR(50),
    accesslevel		INTEGER,
    displaytag		CHAR(1),
    expires			TIMESTAMP
);


CREATE INDEX chessduser_userName_idx ON ChessdUser(userName);

CREATE INDEX Game_WhitePlayerID_idx ON Game(WhitePlayerID);
CREATE INDEX Game_BlackPlayerID_idx ON Game(BlackPlayerID);

CREATE INDEX Game_WhitePlayerName_idx ON Game(WhitePlayerName);
CREATE INDEX Game_BlackPlayerName_idx ON Game(BlackPlayerName);


CREATE INDEX Game_GameResultID_idx ON Game(GameResultID);

CREATE INDEX Move_GameID_idx ON Move(GameID);

CREATE INDEX UserCommandAlias_OwnerID_idx ON UserCommandAlias(OwnerID);
CREATE INDEX UserCensor_OwnerID_idx ON UserCensor(OwnerID);
CREATE INDEX UserChannel_OwnerID_idx ON UserChannel(OwnerID);
CREATE INDEX UserFollow_OwnerID_idx ON UserFollow(OwnerID);
CREATE INDEX UserGNotify_OwnerID_idx ON UserGNotify(OwnerID);
CREATE INDEX UserNotify_OwnerID_idx ON UserNotify(OwnerID);
CREATE INDEX UserStats_UserID_idx ON UserStats(UserID);

CREATE INDEX UserLogon_UserID_idx ON UserLogon(UserID);
CREATE INDEX UserLogon_eventTime_idx ON UserLogon(eventTime);

CREATE INDEX serverEvent_eventTime_idx ON ServerEvent(eventTime);

CREATE INDEX serverLists_listID_idx ON ServerLists(listID);

CREATE INDEX gamebookmark_userID_idx ON GameBookmark(UserID);
CREATE INDEX gamebookmark_gameID_idx ON GameBookmark(GameID);

CREATE INDEX userstats_rating_idx ON UserStats(rating);
CREATE INDEX message_posttime_idx ON Message(posttime);

/*
 * vim: ft=sql
 * Emacs
 * Local variables:
 * mode: sql
 * End:
 */
