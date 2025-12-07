#ifndef __afp__
#define __afp__

#include "afpGlobals.h"

class afp_session;
typedef int32 AFPERROR;

//
//AFP Error codes
//
enum {
	afpAccessDenied				= -5000,
	afpAuthContinue				= -5001,
	afpBadUAM					= -5002,
	afpBadVersNum				= -5003,
	afpBitmapErr				= -5004,
	afpCantMove					= -5005,
	afpDenyConflict				= -5006,
	afpDirNotEmpty				= -5007,
	afpDiskFull					= -5008,
	afpEofError					= -5009,
	afpFileBusy					= -5010,
	afpFlatVol					= -5011,
	afpItemNotFound				= -5012,
	afpLockErr					= -5013,
	afpMiscErr					= -5014,
	afpNoMoreLocks				= -5015,
	afpNoServer					= -5016,
	afpObjectExists				= -5017,
	afpObjectNotFound			= -5018,
	afpParmErr					= -5019,
	afpRangeNotLocked			= -5020,
	afpRangeOverlap				= -5021,
	afpSessClosed				= -5022,
	afpUserNotAuth				= -5023,
	afpCallNotSupported			= -5024,
	afpObjectTypeErr			= -5025,
	afpTooManyFilesOpen			= -5026,
	afpServerGoingDown			= -5027,
	afpCantRename				= -5028,
	afpDirNotFound				= -5029,
	afpIconTypeError			= -5030,
	afpVolLocked				= -5031,						/*Volume is Read-Only*/
	afpObjectLocked				= -5032,						/*Object is M/R/D/W inhibited*/
	afpContainsSharedErr		= -5033,						/*the folder being shared contains a shared folder*/
	afpIDNotFound				= -5034,
	afpIDExists					= -5035,
	afpDiffVolErr				= -5036,
	afpCatalogChanged			= -5037,
	afpSameObjectErr			= -5038,
	afpBadIDErr					= -5039,
	afpPwdSameErr				= -5040,						/*someone tried to change their passint16 to the same passint16 on a mantadory passint16 change*/
	afpPwdTooShortErr			= -5041,						/*the passint16 being set is too int16: there is a minimum length that must be met or exceeded*/
	afpPwdExpiredErr			= -5042,						/*the passint16 being used is too old: this requires the user to change the passint16 before log-in can continue*/
	afpInsideSharedErr			= -5043,						/*the folder being shared is inside a shared folder OR the folder contains a shared folder and is being moved into a shared folder */
																/*  OR the folder contains a shared folder and is being moved into the descendent of a shared folder.*/
	afpInsideTrashErr			= -5044,						/*the folder being shared is inside the trash folder */
																/*  OR the shared folder is being moved into the trash folder OR the folder is being moved to the trash and it contains a shared folder */
	afpBadDirIDType				= -5060,
	afpCantMountMoreSrvre		= -5061,
	afpAlreadyMounted			= -5062,
	afpSameNodeErr				= -5063,
};

//File flags.
#define kShortPathType			0
#define kLongPathType			1
#define kPathDelimeter			':'

//File name string types
#define kShortNames				1
#define kLongNames				2
#define kUnicodeNames			3

#define kRootDirID				2
#define kParentOfRoot			1
#define kNoParent				-1
#define kSoftCreate				0x1
#define kHardCreate				0x2
#define kReadMode				0x1
#define kWriteMode				0x2
#define kDenyRead				0x10
#define kDenyWrite				0x20
#define kReadWriteMode			0x03	
#define kResourceForkBit		0x80
#define kDataFork				0x00
#define kRsrcFork				1
#define kWriteStartEndFlag		0x01
#define kFromStart				0
#define kFromEnd				1

#define MAX_AFP_FILES_OPEN		24
#define MAX_AFP_OPEN_DIRS		24
#define MAX_AFP_OPEN_VOLUMES	16

#define AFP_VERSION_COUNT		5
#define AFP_21_VERSION_STR		"AFPVersion 2.1"
#define AFP_22_VERSION_STR		"AFP2.2"
#define AFP_30_VERSION_STR		"AFPX03"
#define AFP_31_VERSION_STR		"AFP3.1"
#define AFP_32_VERSION_STR		"AFP3.2"
#define AFP_33_VERSION_STR		"AFP3.3"

#define AFP_MAX_UINT32			0xFFFFFFFF
#define AFP_MAX_INT32			0x7FFFFFFF
#define AFP_MAX_UINT16			0xFFFF
#define AFP_MAX_INT16			0x7FFF

// Be (Unix) to AFP time conversion
#define AFP_TIME_DELTA			946684800
#define TO_AFP_TIME(t)			(t - AFP_TIME_DELTA)
#define FROM_AFP_TIME(t)		(t + AFP_TIME_DELTA)

//AFP version constants
enum {
	afpVersion22	= 1,
	afpVersion30,
	afpVersion31,
	afpVersion32,
	afpVersion33
};

//UAM constants
enum {
	afpUAMGuest		= 1,
	afpUAMClearText,
	afpUAMRandnumExchange,
	afpUAMDHCAST128
};

#define UAM_NONE_STR			"No User Authent"
#define UAM_CLEAR_TEXT			"Cleartxt passwrd"
#define UAM_RANDNUM_EXCHANGE	"Randnum exchange"
#define UAM_DHCAST128			"DHCAST128"
#define UAM_COUNT				3
#define AFP_MACHINE_TYPE		"Haiku"
#define MAX_UAM_STRING_LEN		24
#define MAX_AFP_VERSION_LEN		15

typedef unsigned char Password[8];

//
//Attention codes
//
#define ATTN_SERVER_SHUTDOWN        0x8000
#define ATTN_USER_DISCONNECT        0x9000
#define ATTN_SERVER_MESSAGE         0x2000
#define ATTN_SERVER_NOTIFY          0x3001
#define ATTN_TIME_MASK              0x0FFF

//
//Client ID types for FPGetSessionToken call
//
enum {
	kLoginWithoutID			= 0,
	kLoginWithID			= 1,
	kReconnWithID			= 2,
	kLoginWithTimeAndID		= 3,
	kReconnWithTimeAndID	= 4,
	kRecon1Login			= 5,
	kRecon1ReconnectLogin	= 6,
	kRecon1Refresh			= 7,
	kGetKerberosSessionKey	= 8
};
	
//
//Enumerate offsets
//
#define ENUM_FILE_BITMAP_OFFSET	0
#define ENUM_DIR_BITMAP_OFFSET	2
#define ENUM_NUM_ENTRIES		4
#define ENUM_BEGIN_LIST			6

#define UAM_USERNAMELEN							33
#define UAM_CLRTXTPWDLEN						8
#define UAM_RANDNUMPWDLEN						8
#define UAM_MAXPASSWORDLEN						64
#define AFP_ICONSIZE							128
#define MAX_VOLNAME_LEN							27
#define FINDERINFO_LEN							32
#define MAX_AFP_2_NAME							32
#define MAX_AFP_NAME							NAME_MAX
#define MAX_AFP_SHORT_NAME						12
#define MAX_DIR_NAME							(MAX_AFP_NAME+sizeof(char))
#define MAX_FILE_NAME							(MAX_AFP_NAME+sizeof(char))
#define MAX_DIR_SHORT_NAME						(MAX_AFP_SHORT_NAME+sizeof(char))
#define MAX_FILE_SHORT_NAME						(MAX_AFP_SHORT_NAME+sizeof(char))
#define PRODOS_LEN								6
#define APPLETALK_NAME_LEN						31
#define MAX_AFP_STRING_LEN						255
#define MAX_AFP_PATH							PATH_MAX
#define AFP_DIR_NEVER_BACKEDUP					0x80000000
#define AFP_GUEST_USER_ID						0
#define AFP_OWNER_USER_ID						1
#define AFP_HAIKU_GROUP_USERS_NAME				"Users"
#define AFP_HAIKU_GROUP_USERS_ID				32
#define AFP_HAIKU_GROUP_ADMINS_NAME				"Admins"
#define AFP_HAIKU_GROUP_ADMINS_ID				33
#define	AFP_HAIKU_GROUP_GUESTS_NAME				"Guests"
#define AFP_HAIKU_GROUP_GUESTS_ID				34

//
//Some constants for server messages
//
#define AFP_MSG_MAXLEN							199
#define AFP_MSG_LOGINTYPE						0
#define AFP_MSG_SERVERTYPE						1

enum {
	afpByteRangeLock			= 1,	//*						/*AFPCall command codes*/
	afpVolClose					= 2,	//*						/*AFPCall command codes*/
	afpDirClose					= 3,	//						/*AFPCall command codes*/
	afpForkClose				= 4,	//*						/*AFPCall command codes*/
	afpCopyFile					= 5,	//*						/*AFPCall command codes*/
	afpDirCreate				= 6,	//*						/*AFPCall command codes*/
	afpFileCreate				= 7,	//*						/*AFPCall command codes*/
	afpDelete					= 8,	//*						/*AFPCall command codes*/
	afpEnumerate				= 9,	//*						/*AFPCall command codes*/
	afpFlush					= 10,	//*						/*AFPCall command codes*/
	afpForkFlush				= 11,	//*						/*AFPCall command codes*/
	afpGetDirParms				= 12,	//						/*AFPCall command codes*/
	afpGetFileParms				= 13,	//						/*AFPCall command codes*/
	afpGetForkParms				= 14,	//*						/*AFPCall command codes*/
	afpGetSInfo					= 15,	//*						/*AFPCall command codes*/
	afpGetSParms				= 16,	//*						/*AFPCall command codes*/
	afpGetVolParms				= 17,	//*						/*AFPCall command codes*/
	afpLogin					= 18,	//*						/*AFPCall command codes*/
	afpContLogin				= 19,	//-						/*AFPCall command codes*/
	afpLogout					= 20,	//*						/*AFPCall command codes*/
	afpMapID					= 21,	//*						/*AFPCall command codes*/
	afpMapName					= 22,	//						/*AFPCall command codes*/
	afpMove						= 23,	//*						/*AFPCall command codes*/
	afpOpenVol					= 24,	//*						/*AFPCall command codes*/
	afpOpenDir					= 25,	//						/*AFPCall command codes*/
	afpOpenFork					= 26,	//*						/*AFPCall command codes*/
	afpRead						= 27,	//*						/*AFPCall command codes*/
	afpRename					= 28,	//*						/*AFPCall command codes*/
	afpSetDirParms				= 29,	//*						/*AFPCall command codes*/
	afpSetFileParms				= 30,	//*						/*AFPCall command codes*/
	afpSetForkParms				= 31,	//*						/*AFPCall command codes*/
	afpSetVolParms				= 32,	//						/*AFPCall command codes*/
	afpWrite					= 33,	//*						/*AFPCall command codes*/
	afpGetFlDrParms				= 34,	//*						/*AFPCall command codes*/
	afpSetFlDrParms				= 35,	//*						/*AFPCall command codes*/
	afpChangePwd				= 36,	//*
	afpGetUserInfo				= 37,	//*
	afpGetSrvrMsg				= 38,	//*
	afpCreateID					= 39,	//*
	afpDeleteID					= 40,	//-
	afpResolveID				= 41,
	afpExchangeFiles			= 42,
	afpDTOpen					= 48,	//*						/*AFPCall command codes*/
	afpDTClose					= 49,	//*						/*AFPCall command codes*/
	afpGetIcon					= 51,	//*						/*AFPCall command codes*/
	afpGtIcnInfo				= 52,	//*						/*AFPCall command codes*/
	afpAddAPPL					= 53,	//*						/*AFPCall command codes*/
	afpRmvAPPL					= 54,	//*						/*AFPCall command codes*/
	afpGetAPPL					= 55,	//*						/*AFPCall command codes*/
	afpAddCmt					= 56,	//*						/*AFPCall command codes*/
	afpRmvCmt					= 57,	//*						/*AFPCall command codes*/
	afpGetCmt					= 58,	//*						/*AFPCall command codes*/
	afpAddIcon					= 192,	//*						/*Special code for ASP Write commands*/
	
	//AFP3.0
	afpByteRangeLockExt			= 59,	//*
	afpReadExt					= 60,	//*
	afpWriteExt					= 61,	//*
	afpGetAuthMethods			= 62,
	afpLoginExt					= 63,	//*
	afpGetSessionToken			= 64,	//*
	afpDisconnectOldSession		= 65,	//*
	afpEnumerateExt				= 66,	//*
	afpCatSearchExt				= 67,	//-
	afpEnumerateExt2			= 68,	//*
	
	//AFP3.2
	afpGetExtAttr				= 69,	//*
	afpSetExtAttr				= 70,	//*
	afpRemoveExtAttr			= 71,	//*
	afpListExtAttr				= 72,	//*
	
	//AFP3.2+
	afpSyncDir					= 78,	//*
	afpSyncFork					= 79,	//*
	
	//AFP3.1
	afpZzzzz					= 122,	//*
	
	afpLastFunc					= afpSyncFork
};


//
//Types of messages from the server and max str size.
//
#define SRVR_MSG_LOGON		0
#define SRVR_MSG_ATTN		1
#define SRVR_MSG_MAXSIZE	200	//Max characters in string + length byte

//Msg bitmaps
enum {
	KFPMsgSrvrBit		=	0x1
};

// Volume bitmap
enum {
	kFPVolAttributeBit 	= 	0x1,
	kFPVolSignatureBit 	= 	0x2,
	kFPVolCreateDateBit	= 	0x4,
	kFPVolModDateBit 	= 	0x8,
	kFPVolBackupDateBit	= 	0x10,
	kFPVolIDBit 		= 	0x20,
	kFPVolBytesFreeBit	= 	0x40,
	kFPVolBytesTotalBit	= 	0x80,
	kFPVolNameBit		= 	0x100,
	kFPVolExtBytesFree	=	0x200,
	kFPVolExtBytesTotal	=	0x400,
	kFPVolBlockSize		=	0x800
};

//
//Volume types (set in kFPVolSignatureBit)
//
enum {
	kFlatVolumeType			= 1,
	kFixedDirectoryID		= 2,
	kVariableDirectoryID	= 3	//Deprecated
};

//
//Volume attribute bits
//
enum {
	kFPVolReadOnly						= 0x01,
	kFPVolHasVolumePassword				= 0x02,
	kFPVolSupportsFileIDs				= 0x04,
	kFPVolSupportsCatSearch				= 0x08,
	kFPVolSupportsBlankAccessPrivileges	= 0x10,
	kFPVolSupportsUnixPrivs				= 0x20,
	kFPVolSupportsUnicodeNames			= 0x40,
	kFPVolNoNetworkUserIDs				= 0x80,
	kDefaultPrivsFromParent 			= 0x100, //Below here added in AFP3.2
   	kNoExchangeFiles 					= 0x200, 
   	kSupportsExtAttrs 					= 0x400,
   	kSupportsACLs 						= 0x800,
   	kCaseSensitive						= 0x1000,
   	kSupportsTMLockSteal				= 0x2000
};

//Position in reply buffer where parameters requested start
#define FileDirParmsStart	6
#define FILEPARMS_ALL		kFPFileAttributes | kFPParentID | kFPCreateDate |		\
							kFPModDate | kFPBackupDate | kFPFinderInfo | 			\
							kFPLongName | kFPShortName | kFPFileNum | kFPDFLen |	\
							kFPRFLen | kFPProDos

#define DIRPARMS_ALL		kFPDirAttribute | kFPDirParentID | kFPDirCreateDate |	\
							kFPDirModDate | kFPDirBackupDate | kFPDirFinderInfo	|	\
							kFPDirLongName | kFPDirShortName | kFPDirID	|			\
							kFPDirOffCount | kFPDirOwnerID | kFPDirGroupID |		\
							kFPDirAccess | kFPDirProDOS

#define kAccess_OALL		(kAccess_OS  | kAccess_OR  | kAccess_OW )
#define kAccess_GALL		(kAccess_GS  | kAccess_GR  | kAccess_GW )
#define kAccess_WALL		(kAccess_WS  | kAccess_WR  | kAccess_WW )
#define kAccess_UALL		(kAccess_UAS | kAccess_UAR | kAccess_UAW)

//Access rights bits
enum {
	kAccess_OS		= 0x00000001,
	kAccess_OR		= 0x00000002,
	kAccess_OW		= 0x00000004,
	kAccess_GS		= 0x00000100,
	kAccess_GR		= 0x00000200,
	kAccess_GW		= 0x00000400,
	kAccess_WS		= 0x00010000,
	kAccess_WR		= 0x00020000,
	kAccess_WW		= 0x00040000,
	kAccess_UAS		= 0x01000000,
	kAccess_UAR		= 0x02000000,
	kAccess_UAW		= 0x04000000,
	kAccess_Owner	= (1 << 31) //0x80000000
};

//File bitmaps
enum {
	kFPFileNone			= 0x0000,
	kFPFileAttributes	= 0x0001,
	kFPParentID			= 0x0002,
	kFPCreateDate		= 0x0004,
	kFPModDate			= 0x0008,
	kFPBackupDate		= 0x0010,
	kFPFinderInfo		= 0x0020,
	kFPLongName			= 0x0040,
	kFPShortName		= 0x0080,
	kFPFileNum			= 0x0100,
	kFPDFLen			= 0x0200,
	kFPRFLen			= 0x0400,
	kFPProDos			= 0x2000,
	
	//AFP3.0
	kFPExtDataForkLen	= 0x800,
	kFPLaunchLimit		= 0x1000,
	kFPUnicodeName		= 0x2000,
	kFPExtRsrcForkLen	= 0x4000,
	kFPUnixPrivs		= 0x8000
};

//Dir bitmaps
enum {
	kFPDirNone			= 0x0000,
	kFPDirAttribute		= 0x0001,
	kFPDirParentID		= 0x0002,
	kFPDirCreateDate	= 0x0004,
	kFPDirModDate		= 0x0008,
	kFPDirBackupDate	= 0x0010,
	kFPDirFinderInfo	= 0x0020,
	kFPDirLongName		= 0x0040,
	kFPDirShortName		= 0x0080,
	kFPDirID			= 0x0100,
	kFPDirOffCount		= 0x0200,
	kFPDirOwnerID		= 0x0400,
	kFPDirGroupID		= 0x0800,
	kFPDirAccess		= 0x1000,
	kFPDirProDOS		= 0x2000,
	
	//AFP3.0
	kFPDirUnicodeName	= 0x2000,
	kFPDirUnixPrivs		= 0x8000
};

//File attribute flags
enum {
	kFileInvisible		= 0x00001,
	kFileMultiUser		= 0x00002,
	kFileSystem			= 0x00004,
	kFileDataForkOpen	= 0x00008,
	kFileRsrcForkOpen	= 0x00010,
	kFileWriteInhibit	= 0x00020,
	kFileNeedsBackup	= 0x00040,
	kFileRenameInhibit	= 0x00080,
	kFileDeleteInhibit	= 0x00100,
	kFileCopyProtected	= 0x00200,
	
	kFileSetClearFlag	= 0x08000
};

//Directory attribut flags
enum {
	kDirInvisible		= 0x00001,
	kDirIsExpFolder		= 0x00002,
	kDirSystem			= 0x00004,
	kDirMounted			= 0x00008,
	kDirInExpFolder		= 0x00010,
	kDirWriteInhibit	= 0x00020, //WriteInhibit not supported for directories
	kDirBackupNeeded	= 0x00040,
	kDirRenameInhibit	= 0x00080,
	kDirDeleteInhibit	= 0x00100,
	
	kDirSetClearFlag	= 0x04000,
	
	kFileDirIsDir		= 0x80
};

//UserInfo bitmaps
enum {
	kUserID		= 0x1,
	kGroupID	= 0x2
};

typedef struct
{	
	int16			wAttributes;
	uint32			dwParentDirId;
	uint32			lwCreateDate;
	uint32			lwModDate;
	uint32			lwBackupdate;
	char			finderInfo[FINDERINFO_LEN];
	char			longName[MAX_DIR_NAME];
	char			shortName[MAX_DIR_SHORT_NAME];
	uint32			dwDirID;
	int16			wOffspring;
	uint32			dwOwnerId;
	uint32			dwGroupId;
	uint32			dwAccessRights;
	char			proDOSinfo[PRODOS_LEN];
}DIRPARMS, *PDIRPARMS;

typedef struct
{
	int16			wAttributes;
	uint32			dwParentDirId;
	uint32			lwCreateDate;
	uint32			lwModDate;
	uint32			lwBackupDate;
	char			finderInfo[FINDERINFO_LEN];
	char			int32Name[MAX_DIR_NAME];
	char			int16Name[MAX_DIR_SHORT_NAME];
	uint32			dwFileNum;
	uint32			dwDForkLen; 
	uint32			dwRForkLen;
	char			proDOSinfo[PRODOS_LEN];
}FILEPARMS, *PFILEPARMS;

typedef struct
{
	int16						wFileBitmap;
	int16						wDirBitmap;
	int8						bFDPAttributes;
	int16						wVolID;
	uint32						dwDirID;
	uint32						beAttribData;
	
	union
	{
		FILEPARMS	fp;
		DIRPARMS	dp;
	}parms;
}FILEDIRPARMS, *PFILEDIRPARMS;

typedef struct
{
	int16	x;
	int16	y;
}MAC_POINT;

//Finder Info struct
typedef struct
{
	char		fdType[4];
	char		fdCreator[4];
	int16		fdFlags;
	MAC_POINT	fdLocation;
	int16		fdFldr;
	char		fdFinderData[16];
}FINDER_INFO, *PFINDER_INFO;

//
//Finder flags (fdFlags) in FINDER_INFO
//
enum
{
	kFDIsShared			= 0x0040,
	kFDHasNoInits		= 0x0080,
	kFDHasBeenInited	= 0x0100,
	kFDReserved			= 0x0200,
	kFDHasCustomIcon	= 0x0400,
	kFDIsStationary		= 0x0800,
	kFDNameLocked		= 0x1000,
	kFDHasBundle		= 0x2000,
	kFDInvisible		= 0x4000,
	kFDIsAlias			= 0x8000
};

//FPGetSrvrInfo() server flags bit values
enum {
	kSupportsCopyFile			= 0x1,
	kSupportsChngPswd			= 0x2,
	kDontAllowSavePwd			= 0x4,
	kSupportsSrvrMsgs			= 0x8,
	kSupportsSrvrSig			= 0x10,
	kSupportsTCPIP				= 0x20,
	kSupportsSrvrNotification	= 0x40,
	
	//AFP3.0
	kSupportsReconnect			= 0x80,
	kSupportsDirectorySvcs		= 0x100,
	kSupportsUTF8Name			= 0x200,
	kSupportsExtSleep			= 0x800,
	kSuperClient				= 0x8000
};

//FPGetSrvrInfo() offsets
#define SRVRINFO_OFFSET_MACHTYPE		0
#define SRVRINFO_OFFSET_AFPVERSCOUNT	2
#define SRVRINFO_OFFSET_UAMCOUNT		4
#define SRVRINFO_OFFSET_VOLUMEICON		6
#define SRVRINFO_OFFSET_FLAGS			8
#define SRVRINFO_OFFSET_SRVRNAME		10

//FPByteRangeLock flag bits
enum {
	kUnlockFlag					= 0x01,
	kStartEndFlag				= 0x80,
	
	kLock						= TRUE,
	kUnlock						= FALSE,
	kSelFromStart				= TRUE,
	kSelFromEnd					= FALSE
};

//FPGetSrvrMsg bitmap flags
enum {
	kSendMessageAsUnicode		= 0x02
};

AFPERROR FPUnimplemented(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPDispatchCommand(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetSrvrInfo(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetSessionToken(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPDisconnectOldSession(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetSrvrParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPOpenVol(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPCloseVol(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetVolParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPResolveID(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPGetFileDirParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPEnumerate(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPSetFileDirParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPCreateDir(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPFlush(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPFlushFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPCloseFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPOpenFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPSetForkParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetForkParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPCreateFile(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPDelete(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPRead(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPWrite(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPMoveAndRename(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPRename(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPMapID(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPMapName(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetUserInfo(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPByteRangeLock(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPCopyFile(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPCreateID(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPZzzz(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

#endif //__afp__
