#ifndef __afpcommands__
#define __afpcommands__

#define LO_LONG(i64)	((uint32)i64)
#define HI_LONG(i64)	((uint32)(((int64)(i64) >> 32) & 0xFFFFFFFF))

#define AFPServerSignature 		"application/x-vnd.afp_server"

//
//The server version, should be updated with each release.
//
#define AFP_SERVER_VERSION		0x01080600	//version 1.8.6

//
//This is the version of the command protocol for communicating
//with the server (mainly from the config app).
//
#define AFP_CMD_VERSION			0x02020000	//version 2.2

//
//This is the version of the user database. It is tracked only
//for upgrade purposes when changing schema between releases.
//
#define AFP_USERDB_VERSION		0x02000000	//version 2.0

//
//Preference file coordinates and attribute ID's
//
#define AFP_PREFS_FILE_NAME			"afpServerPrefs"
#define AFP_LOGONMSG_FILE_NAME		"afpLogonMessage"
#define AFP_VOLUMES_TYPE			"AFP:"
#define AFP_USERS_TYPE				"USR:"
#define AFP_USERS_SCHEMA_VERSION	"UVER"				//uint32 version of user info

#define AFP_HOSTNAME_FILE_NAME	"afpHostname"
#ifndef MAX_HOSTNAME_LEN
#define MAX_HOSTNAME_LEN		128
#endif

//
//Flags used for user options/control
//
enum
{
	kUserEnabled		= 0x01,		//Can user login?
	kDontDisplay		= 0x02,		//Prevent showing in UI, used for Guest account
	kMustChngPswd		= 0x04,		//Force user to change password next login
	kIsAdmin			= 0x08,		//User has admin rights
	kTempAuthenticated	= 0x10,		//Used to allow access temporarily when pswd is expired
	kCanChngPswd		= 0x20		//Set if the user is allowed to change pswd
};

//
//This is the name of the "guest" account. Guest accounts are stored
//like regular user accounts but hold some special properties.
//
#define AFP_GUEST_NAME			"Guest"

#define AFP_PARAM_INT64			"afp-int64"
#define AFP_PARAM_INT32			"afp-int32"
#define AFP_PARAM_INT16			"afp-int16"
#define AFP_PARAM_BOOL			"afp-bool"
#define AFP_PARAM_STRING		"afp-string"
#define AFP_PARAM_PATHSTRING	AFP_PARAM_STRING


//
//Error codes returned by the afp_server during API operations.
//
enum {
	be_afp_usernotfound			= -4000,
	be_afp_useralreadyexists	= -4001,
	be_afp_fileoperationfailed	= -4002,
	be_afp_nomorerecords		= -4003,
	be_afp_invalidvolume		= -4004,
	be_afp_sharealreadyexits	= -4005,
	be_afp_paramerr				= -4006,
	be_afp_notadirectory		= -4007,
	be_afp_isrootdir			= -4008,
	be_afp_blankentry			= -4009,
	be_afp_invlidehostnamelen	= -4010,
	be_afp_userdbnotfound		= -4011,
	be_afp_baduserdbversion		= -4012,
	be_afp_userdbupgraded		= -4013,

	be_afp_success				= 0,
	be_afp_failure				= -1
};

//*********************Versioning
#define CMD_AFP_SERVER_VERSION				'vers'	//Version of the afp_server binary
#define CMD_AFP_COMMAND_VERSION				'cver'	//Version of the command api

//*********************Messages
#define CMD_AFP_SENDMSG						'send'
#define CMD_AFP_UPDATELOGINMSG				'updt'

//*********************Manage Volumes
#define CMD_AFP_ADDSHARE					'adds'	//AddShare(BString path, int32 volFlags)
#define CMD_AFP_RMVSHARE					'rems'	//RemoveShare(BString path)
#define CMD_AFP_GETVOLUMENAME				'gvol'	//GetVolumeName(int16 volIndex, BString* name)
#define CMD_AFP_GETVOLUMEPATH				'gvnm'	//GetVolumePath(int16 volIndex, BString* path)
#define CMD_AFP_SETVOLFLAGS					'sflg'	//SetVolumeFlags(BString* path, int32 flags)
#define CMD_AFP_GETVOLFLAGS					'gflg'	//GetVolumeFlags(BString* path, int32* flags)

//*********************Managing users
#define CMD_AFP_ADDUSER						'addu'
#define CMD_AFP_DELUSER						'delu'
#define CMD_AFP_GETINDUSER					'getu'
#define CMD_AFP_GETUSERINFO					'geti'
#define CMD_AFP_UPDATEUSERINFO				'updu'

#define AFP_PARAM_STRING_USERNAME			"user-string"
#define AFP_PARAM_STRING_PASSWORD			"pswd-string"

//*********************Getting Server Statistics
#define CMD_AFP_GETBYTESPERSECOND			'gbps'
#define CMD_AFP_GETDSIPACKETS				'gdsi'
#define CMD_AFP_GETUSERSLOGGEDIN			'gusr'
#define CMD_AFP_GETRECVBYTES				'grcv'
#define CMD_AFP_GETSENTBYTES				'gsnt'

//*********************Hostname
//NOTE: This sets the AFP-specific server name that Mac clients will see. It
//		does not set the computer's network hostname or computer name.
#define CMD_AFP_GETHOSTNAME					'ghst'	//Returns the current hostname for the afp server
#define CMD_AFP_SETHOSTNAME					'shst'	//Sets the hostname for the afp server

#endif //__afpcommands__
