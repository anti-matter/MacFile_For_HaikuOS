#include <memory>
#include <Path.h>

#include "debug.h"

#include "afp.h"
#include "afp_session.h"
#include "afpuser.h"
#include "afplogon.h"
#include "afpdesk.h"
#include "afpmsg.h"
#include "afpextattr.h"
#include "afpreplay.h"
#include "commands.h"
#include "dsi_stats.h"
#include "dsi_scavenger.h"

extern std::unique_ptr<BList> volume_blist;

int16					gMaxAFPSessions = 0;
extern dsi_scavenger*	gAFPSessionMgr;
extern dsi_stats		gAFPStats;

typedef AFPERROR (*afp_func)(afp_session* sess, int8* reqBuf, int8* replyBuf, int32* afpDataSize);

typedef struct
{
	afp_func	func;
	char		func_name[24];
}AFP_TABLE;

AFP_TABLE afpTable[] = {
	{FPUnimplemented, 			""},
	{FPByteRangeLock, 			"FPByteRangeLock"},
	{FPCloseVol, 				"FPCloseVol"},
	{FPUnimplemented, 			""},					// afpDirClose
	{FPCloseFork, 				"FPCloseFork"},
	{FPCopyFile, 				"FPCopyFile"},
	{FPCreateDir,				"FPCreateDir"},
	{FPCreateFile,				"FPCreateFile"},
	{FPDelete,					"FPDelete"},
	{FPEnumerate,				"FPEnumerate"},
	{FPFlush,					"FPFlush"},
	{FPFlushFork,				"FPFlushFork"},
	{FPUnimplemented,			""},					// afpGetDirParms
	{FPUnimplemented,			""},					// afpGetFileParms
	{FPGetForkParms,			"FPGetForkParms"},
	{FPGetSrvrInfo,				"FPGetSrvrInfo"},
	{FPGetSrvrParms,			"FPGetSrvrParms"},
	{FPGetVolParms,				"FPGetVolParms"},
	{FPLogin,					"FPLogin"},
	{FPContLogin,				"FPContLogin"},
	{FPLogout,					"FPLogout"},
	{FPMapID,					"FPMapID"},
	{FPMapName,					"FPMapName"},
	{FPMoveAndRename,			"FPMoveAndRename"},
	{FPOpenVol,					"FPOpenVol"},
	{FPUnimplemented,			""},					// afpOpenDir
	{FPOpenFork,				"FPOpenFork"},
	{FPRead,					"FPRead"},
	{FPRename,					"FPRename"},
	{FPSetFileDirParms,			"FPSetFileDirParms"},
	{FPSetFileDirParms,			"FPSetFileDirParms"},
	{FPSetForkParms,			"FPSetForkParms"},
	{FPUnimplemented,			""},					// afpSetVolParms
	{FPWrite,					"FPWrite"},
	{FPGetFileDirParms,			"FPGetFileDirParms"},
	{FPSetFileDirParms,			"FPSetFileDirParms"},
	{FPChangePswd,				"FPChangePswd"},
	{FPGetUserInfo,				"FPGetUserInfo"},
	{FPGetServerMessage,		"FPGetServerMessage"},
	{FPCreateID,				"FPCreateID"},
	{FPUnimplemented,			""},					// afpDeleteID
	{FPResolveID,			    "FPResolveID"},			// afpResolveID
	{FPUnimplemented,			""},					// afpExchangeFiles
	{FPUnimplemented,			""},					// 43
	{FPUnimplemented,			""},					// 44
	{FPUnimplemented,			""},					// 45
	{FPUnimplemented,			""},					// 46
	{FPUnimplemented,			""},					// 47
	{FPOpenDT,					"FPOpenDT"},
	{FPCloseDT,					"FPCloseDT"},
	{FPUnimplemented,			""},					// 50
	{FPGetIcon,					"FPGetIcon"},
	{FPGetIconInfo,				"FPGetIconInfo"},
	{FPAddAPPL,					"FPAddAPPL"},
	{FPRemoveAPPL,				"FPRemoveAPPL"},
	{FPGetAPPL,					"FPGetAPPL"},
	{FPAddComment,				"FPAddComment"},
	{FPRemoveComment,			"FPRemoveComment"},
	{FPGetComment,				"FPGetComment"},
	{FPByteRangeLock,			"FPByteRangeLock"},
	{FPRead,					"FPRead"},
	{FPWrite,					"FPWrite"},
	{FPUnimplemented,			""},					// afpGetAuthMethods
	{FPLogin,					"FPLogin"},
	{FPGetSessionToken,			"FPGetSessionToken"},
	{FPDisconnectOldSession,	"FPDisconnectOldSession"},
	{FPEnumerate,				"FPEnumerateExt"},
	{FPUnimplemented,			""},					// afpCatSearchExt
	{FPEnumerate,				"FPEnumerateExt2"},
	{FPGetExtAttribute,			"FPGetExtAttribute"},
	{FPSetExtAttribute,			"FPSetExtAttribute"},
	{FPRemoveExtAttribute,		"FPRemoveExtAttribute"},
	{FPListExtAttribute,		"FPListExtAttribute"},
	{FPUnimplemented,			""},					// 73
	{FPUnimplemented,			""},					// 74
	{FPUnimplemented,			""},					// 75
	{FPUnimplemented,			""},					// 76
	{FPUnimplemented,			""},					// 77
	{FPSyncDir,					""},
	{FPSyncFork,				""}
};

// AFP timestamps count seconds from 1904-01-01.
// Unix timestamps count seconds from 1970-01-01.
constexpr int64 kAFPToUnixEpochOffset = 2082844800LL;

uint32 ToAFPTime(time_t unixTime)
{
	const int64 afpTime =
		static_cast<int64>(unixTime) + kAFPToUnixEpochOffset;

	return static_cast<uint32>(afpTime);
}

time_t FromAFPTime(uint32_t afpTime)
{
	const int64 unixTime =
		static_cast<int64>(afpTime) - kAFPToUnixEpochOffset;

	return static_cast<time_t>(unixTime);
}

/*
 * IsResFile()
 *
 * Description:
 *		Returns TRUE if this is a special-case resource file.
 *
 * Returns: None
 */

bool IsResFile(BEntry* entry)
{
	BPath path;
	entry->GetPath(&path);
	std::filesystem::path spath = path.Path();

	return spath.extension() == res_file_extension;
}

/*
 * FPUnimplemented()
 *
 * Description:
 *		Function used to catch unimplemented afp functions.
 *
 * Returns: None
 */

AFPERROR FPUnimplemented(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)
	#pragma unused(afpReqBuffer)
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	DBGWRITE(dbg_level_warning, "Call not implemented (%d)\n", *afpReqBuffer);
	return( afpCallNotSupported );
}

/*
 * FPDispatchCommand()
 *
 * Description:
 *		Dispatch the incoming afp command to the correct FPXXXXX API
 *		implementation function.
 *
 * Returns: None
 */

AFPERROR FPDispatchCommand(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	uint8		afpCommand	= *afpReqBuffer;
	AFPERROR	afpError	= AFP_OK;

	*afpDataSize = 0;

	// Whether we're authenticated or not is how we determine
	// what afp commands we allow the client to execute.
	if (afpSession->IsAuthenticated())
	{
		if (afpCommand <= afpLastFunc)
		{
			DBGWRITE(dbg_level_trace, "Function call: %s\n", afpTable[afpCommand].func_name);

			afpError = afpTable[afpCommand].func(
									afpSession,
									afpReqBuffer,
									afpReplyBuffer,
									afpDataSize
									);

		}
		else
		{
			switch(afpCommand)
			{
				case afpAddIcon:
					afpError = FPAddIcon(afpSession, afpReqBuffer, afpReplyBuffer, afpDataSize);
					break;

				case afpZzzzz:
					afpError = FPZzzz(afpSession, afpReqBuffer, afpReplyBuffer, afpDataSize);
					break;

				default:
					DBGWRITE(dbg_level_trace, "Call not implemented (%d)\n", afpCommand);
					afpError = afpCallNotSupported;
					break;
			}
		}
	}
	else
	{
		switch(afpCommand)
		{
			case afpGetSInfo:
				afpError = FPGetSrvrInfo(afpSession, afpReqBuffer, afpReplyBuffer, afpDataSize);
				break;

			case afpLoginExt:
			case afpLogin:
				afpError = FPLogin(afpSession, afpReqBuffer, afpReplyBuffer, afpDataSize);
				break;

			case afpContLogin:
				afpError = FPContLogin(afpSession, afpReqBuffer, afpReplyBuffer, afpDataSize);
				break;

			case afpChangePwd:
			{
				AFP_USER_DATA	userInfo;

				afpSession->GetUserInfo(&userInfo);

				// If the users password has expired and they are temporarily authenticated
				// (e.g. logged in but in a "weird only chngpswd works state"), allow them
				// to change their password.
				if ((userInfo.flags & kMustChngPswd) && (userInfo.flags & kTempAuthenticated))
				{
					afpError = FPChangePswd(afpSession, afpReqBuffer, afpReplyBuffer, afpDataSize);
				}
				break;
			}

			default:
				DBGWRITE(dbg_level_warning, "Attempted call without authentication (%d)\n", afpCommand);
				afpError = afpCallNotSupported;
				break;
		}
	}

	return( afpError );
}


