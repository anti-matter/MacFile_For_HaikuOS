#include <fs_attr.h>
#include <DataIO.h>

#include "debug.h"

#include "afp.h"
#include "afpmsg.h"
#include "afpdesk.h"
#include "afpuser.h"
#include "afplogon.h"
#include "afpvolume.h"
#include "afpaccess.h"
#include "afpextattr.h"
#include "afphostname.h"
#include "afpaccess.h"
#include "afpreplay.h"
#include "commands.h"
#include "dsi_stats.h"
#include "fp_rangelock.h"
#include "afp_buffer.h"
#include "afp_session.h"
#include "fp_volume.h"
#include "fp_objects.h"
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
	{FPUnimplemented, 			""},					//afpDirClose
	{FPCloseFork, 				"FPCloseFork"},
	{FPCopyFile, 				"FPCopyFile"},
	{FPCreateDir,				"FPCreateDir"},
	{FPCreateFile,				"FPCreateFile"},
	{FPDelete,					"FPDelete"},
	{FPEnumerate,				"FPEnumerate"},
	{FPFlush,					"FPFlush"},
	{FPFlushFork,				"FPFlushFork"},
	{FPUnimplemented,			""},					//afpGetDirParms
	{FPUnimplemented,			""},					//afpGetFileParms
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
	{FPUnimplemented,			""},					//afpOpenDir
	{FPOpenFork,				"FPOpenFork"},
	{FPRead,					"FPRead"},
	{FPRename,					"FPRename"},
	{FPSetFileDirParms,			"FPSetFileDirParms"},
	{FPSetFileDirParms,			"FPSetFileDirParms"},
	{FPSetForkParms,			"FPSetForkParms"},
	{FPUnimplemented,			""},					//afpSetVolParms
	{FPWrite,					"FPWrite"},
	{FPGetFileDirParms,			"FPGetFileDirParms"},
	{FPSetFileDirParms,			"FPSetFileDirParms"},
	{FPChangePswd,				"FPChangePswd"},
	{FPGetUserInfo,				"FPGetUserInfo"},
	{FPGetServerMessage,		"FPGetServerMessage"},
	{FPCreateID,				"FPCreateID"},
	{FPUnimplemented,			""},					//afpDeleteID
	{FPResolveID,			    "FPResolveID"},			//afpResolveID
	{FPUnimplemented,			""},					//afpExchangeFiles
	{FPUnimplemented,			""},					//43
	{FPUnimplemented,			""},					//44
	{FPUnimplemented,			""},					//45
	{FPUnimplemented,			""},					//46
	{FPUnimplemented,			""},					//47
	{FPOpenDT,					"FPOpenDT"},
	{FPCloseDT,					"FPCloseDT"},
	{FPUnimplemented,			""},					//50
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
	{FPUnimplemented,			""},					//afpGetAuthMethods
	{FPLogin,					"FPLogin"},
	{FPGetSessionToken,			"FPGetSessionToken"},
	{FPDisconnectOldSession,	"FPDisconnectOldSession"},
	{FPEnumerate,				"FPEnumerateExt"},
	{FPUnimplemented,			""},					//afpCatSearchExt
	{FPEnumerate,				"FPEnumerateExt2"},
	{FPGetExtAttribute,			"FPGetExtAttribute"},
	{FPSetExtAttribute,			"FPSetExtAttribute"},
	{FPRemoveExtAttribute,		"FPRemoveExtAttribute"},
	{FPListExtAttribute,		"FPListExtAttribute"},
	{FPUnimplemented,			""},					//73
	{FPUnimplemented,			""},					//74
	{FPUnimplemented,			""},					//75
	{FPUnimplemented,			""},					//76
	{FPUnimplemented,			""},					//77
	{FPSyncDir,					""},
	{FPSyncFork,				""}
};


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

	//
	//Whether we're authenticated or not is how we determine
	//what afp commands we allow the client to execute.
	//
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

				//
				//If the users password has expired and they are temporarily authenticated
				//(e.g. logged in but in a "weird only chngpswd works state"), allow them
				//to change their password.
				//
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


/*
 * FPGetSrvrInfo()
 *
 * Description:
 *		The FPGetSrvrInfo() AFP API. This is the only AFP API (other than
 *		FPLogon of course) that can be called without opening a session
 *		and being authenticated.
 *
 * Returns: None
 */

AFPERROR FPGetSrvrInfo(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)
	#pragma unused(afpReqBuffer)

	char		hostname[MAX_HOSTNAME_LEN];
	int16		afpFlags				= 0;
	int8*		pBuffer					= NULL;
	int8*		pAddressCountOffset		= NULL;
	int8*		pServerSigOffset		= NULL;

	*afpDataSize = 0;

	//
	//Set the flags which tell what our server supports.
	//
	afpFlags = kSupportsTCPIP
				| kSupportsSrvrNotification
				| kSupportsCopyFile
				| kSupportsSrvrMsgs
				| kSupportsReconnect
				| kSupportsChngPswd
				| kSupportsExtSleep;

	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_FLAGS]) = htons(afpFlags);

	//
	//We don't have a custom volume icon.
	//
	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_VOLUMEICON]) = 0;

	//
	//Blast in the computer name.
	//
	afp_GetHostname(hostname, sizeof(hostname));

	pBuffer = &afpReplyBuffer[SRVRINFO_OFFSET_SRVRNAME];
	PUSH_CSTRING(hostname, pBuffer);

	//
	//Add an extra padding byte if the AFP buffer is not on
	//an even boundary.
	//
	if ((pBuffer - afpReplyBuffer) % 2)
		*pBuffer++ = 0x00;

	//
	//Save the server signature offset
	//
	pServerSigOffset = pBuffer;
	pBuffer += sizeof(int16);

	//
	//Network address count offset, save it for later.
	//
	pAddressCountOffset = pBuffer;
	pBuffer += sizeof(int16);

	//
	//Set the machine type and offset.
	//
	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_MACHTYPE]) =
											htons(pBuffer - afpReplyBuffer);

	PUSH_CSTRING(AFP_MACHINE_TYPE, pBuffer);

	//
	//Paste in the AFP version we support.
	//
	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_AFPVERSCOUNT]) =
											htons(pBuffer - afpReplyBuffer);

	*pBuffer++ = AFP_VERSION_COUNT;

	PUSH_CSTRING(AFP_22_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_30_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_31_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_32_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_33_VERSION_STR, pBuffer);

	//
	//Now the supported UAM's get included.
	//
	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_UAMCOUNT]) =
											htons(pBuffer - afpReplyBuffer);

	if (afpAccountEnabled(AFP_GUEST_NAME))
	{
		*pBuffer++ = UAM_COUNT;
		PUSH_CSTRING(UAM_NONE_STR, pBuffer);
	}
	else
	{
		*pBuffer++ = UAM_COUNT - 1;
	}

	//
	//This is always supported in our server, no matter what.
	//
	PUSH_CSTRING(UAM_CLEAR_TEXT, pBuffer);
	PUSH_CSTRING(UAM_DHCAST128, pBuffer);

	//
	//Set the server sig offset and value.
	//
	*((int16*)pServerSigOffset) = htons(pBuffer - afpReplyBuffer);
	*pBuffer++ = 0;

	//Network Address: Note, there is no reason for us to pass
	//back address since we only support connections over TCP/IP.
	//This feature is only for connection where the initial connection
	//is done over AppleTalk.

	*((int16*)pAddressCountOffset) = htons(pBuffer - afpReplyBuffer);
	*pBuffer++ = 0;

	//
	//This size does NOT include the size of the DSI Header.
	//
	*afpDataSize = (pBuffer - afpReplyBuffer);

	return( AFP_OK );
}


/*
 * FPGetSessionToken()
 *
 * Description:
 *		Returns a session token to the client for re-connection at
 *		a later time if the client crashes or connection is lost.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetSessionToken(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	afp_session*	afpKillSession	= NULL;
	int8			afpType			= 0;
	int32			afpIDSize		= 0;
	int8*			afpID			= NULL;
	int32			afpToken		= 0;
	uint32			afpTimeStamp	= 0;
	bool			fSavedID		= false;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	//
	//First byte is afp command and padding.
	//
	afpRequest.Advance(sizeof(int16));
	afpType	= afpRequest.GetInt16();

	if (afpType != kLoginWithoutID)
	{
		afpIDSize = afpRequest.GetInt32();

		DBGWRITE(dbg_level_trace, "Client ID size = %lu\n", afpIDSize);

		//
		//For types 3 & 4, a time stamp is included in the request
		//
		switch(afpType)
		{
			case kLoginWithTimeAndID:
			case kReconnWithTimeAndID:
				afpTimeStamp = afpRequest.GetInt32();
				break;

			default:
				break;
		}

		afpID = new int8[afpIDSize];

		if (afpID != NULL) {

			afpRequest.GetRawData(afpID, afpIDSize);
		}
	}

	switch(afpType)
	{
		case kLoginWithoutID:
			DBGWRITE(dbg_level_trace, "kLoginWithoutID\n");
			break;

		//
		//DEPRECATED: This constant is deprecated per AFP3.2 spec, but we keep
		//it around for older clients.
		//
		case kLoginWithID:
			DBGWRITE(dbg_level_trace, "kLoginWithID\n");

			afpKillSession = gAFPSessionMgr->FindSessionByID(afpIDSize, afpID);

			if (afpKillSession != NULL)
			{
				DBGWRITE(dbg_level_trace, "Killing old session!\n");
				afpSession->KillSession();
			}

			afpSession->SetClientID(afpIDSize, afpID);
			fSavedID = true;
			break;

		//
		//The client wants his old session to be discarded. The client is sending
		//an IDLength, an ID, and a Timestamp. We only discard the old session if
		//the timestamps DO NOT match.
		//
		case kLoginWithTimeAndID:
			DBGWRITE(dbg_level_trace, "kLoginWithTimeAndID\n");

			afpKillSession = gAFPSessionMgr->FindSessionByID(afpIDSize, afpID);

			if (afpKillSession != NULL)
			{
				DBGWRITE(dbg_level_trace, "Found old session!!\n");

				if (afpKillSession->GetAFPTimeStamp() != afpTimeStamp)
				{
					//
					//Time stamps don't match, discard the old session.
					//
					afpSession->KillSession();
					break;
				}
			}

			afpSession->SetAFPTimeStamp(afpTimeStamp);
			afpSession->SetClientID(afpIDSize, afpID);

			fSavedID = true;
			break;

		//
		//DEPRECATED: Deprecated as per AFP3.2 spec
		//
		case kReconnWithID:
			DBGWRITE(dbg_level_trace, "kReconnWithID\n");
			break;

		//
		//The client has just reconnected to a previously disconnected
		//session. Update the session with the new ID.
		//
		case kReconnWithTimeAndID:
			DBGWRITE(dbg_level_trace, "kReconnWithTimeAndID\n");

			afpSession->SetAFPTimeStamp(afpTimeStamp);
			afpSession->SetClientID(afpIDSize, afpID);

			fSavedID = true;
			break;

		//
		//The following IDs are unsupported by this afp server since we
		//don't support the reconnect UAM.
		//
		case kRecon1Login:
		case kRecon1ReconnectLogin:
		case kRecon1Refresh:
		case kGetKerberosSessionKey:

		default:
			DBGWRITE(dbg_level_trace, "Bad type! (%d)\n", afpType);
			return( afpParmErr );
	}

	afpToken = afpSession->GetToken();

	afpReply.AddInt32(AFP_SESSION_TOKEN_SIZE);
	afpReply.AddInt32(afpToken);

	DBGWRITE(dbg_level_trace, "Token = %lx\n", afpToken);

	if ((!fSavedID) && (afpID != NULL))
	{
		delete [] afpID;
	}

	*afpDataSize = afpReply.GetDataLength();

	return( AFP_OK );
}


/*
 * FPDisconnectOldSession()
 *
 * Description:
 *		AFP clients call this when they are inadvertantly cutoff from
 *		the file server. The client calls this to transfer all resources
 *		from the old session to this new one.
 *
 * Returns: AFPERROR
 */

AFPERROR FPDisconnectOldSession(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	afp_session*	afpLostSession	= NULL;
	int8			afpType			= 0;
	int32			afpTokenLength	= 0;
	int32			afpToken		= 0;

	DBGWRITE(dbg_level_trace, "Enter\n");

	afpRequest.Advance(sizeof(int16));

	afpType			= afpRequest.GetInt8();
	afpTokenLength	= afpRequest.GetInt32();

	if (afpTokenLength != AFP_SESSION_TOKEN_SIZE)
	{
		//
		//Our token length is fixed, it's not right, we're in
		//deep doo-doo.
		//
		return( afpParmErr );
	}

	afpToken 		= afpRequest.GetInt32();
	afpLostSession 	= gAFPSessionMgr->FindSessionByToken(afpToken);

	if (afpLostSession != NULL)
	{
		DBGWRITE(dbg_level_info, "Found diconnected session!\n");

		afpLostSession->KillSession();
	}

	return( AFP_OK );
}


/*
 * FPGetSrvrParms()
 *
 * Description:
 *		Return server parameters.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetSrvrParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)
	#pragma unused(afpReqBuffer)

	afp_buffer	afpReply(afpReplyBuffer);
	int8		numVolumes;
	int8		i;

	DBGWRITE(dbg_level_info, "Getting server parms...\n");

	//
	//First, stuff in the system time into the reply buffer.
	//
	afpReply.AddInt32(TO_AFP_TIME(real_time_clock()));

	//
	//Stuff in the number of volumes we have shared out.
	//
	numVolumes = volume_blist->CountItems();
	afpReply.AddInt8(numVolumes);

	//
	//Now stuff in all the volume names as pascal strings.
	//
	for (i = 0; i < numVolumes; i++)
	{
		fp_volume*		afpVolume = NULL;
		char			volumeName[MAX_AFP_NAME];
		int8			volumeFlags;

		if ((afpVolume = (fp_volume*)volume_blist->ItemAt(i)) != NULL)
		{
			volumeFlags = 0;
			strcpy(volumeName, afpVolume->GetVolumeName());

			DBGWRITE(dbg_level_trace, "Adding volume %s\n", volumeName);

			afpReply.AddInt8(volumeFlags);
			afpReply.AddCStringAsPascal(volumeName);
		}
	}

	*afpDataSize = afpReply.GetDataLength();

	return( AFP_OK );
}


/*
 * FPOpenVol()
 *
 * Description:
 *		Open a volume for access to this user session.
 *
 * Returns: AFPERROR
 */

AFPERROR FPOpenVol(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer);
	char		szVolumeName[MAX_VOLNAME_LEN+1];
	Password	szPassword;
	fp_volume*	afpVolume	= NULL;
	int16		volBitmap	= 0;
	AFPERROR	afpError	= AFP_OK;

	afpRequest.Advance(sizeof(int16));

	//
	//Get the volume bitmap from the request buffer.
	//
	volBitmap = afpRequest.GetInt16();

	//
	//Extract the volume name the client wants to open.
	//
	afpRequest.GetString(szVolumeName, sizeof(szVolumeName));

	//
	//A null byte may be added to make the next parm start
	//on an even boundary.
	//
	if ((afpRequest.GetCurrentPosPtr() - afpReqBuffer) % 2) {

		afpRequest.Advance(sizeof(int8));
	}

	//
	//Extract the volume password, its okay if there isn't one there.
	//
	afpRequest.GetRawData(szPassword, sizeof(szPassword));

	//
	//Now search for the volume object associated with the volume name.
	//
	afpVolume = FindVolume(szVolumeName);

	//
	//If we found the volume, then get the parameters.
	//
	if (afpVolume != NULL)
	{
		DBGWRITE(dbg_level_trace, "Opening volume %s\n", szVolumeName);

		afpError = afpVolume->fp_GetVolParms(volBitmap, afpReply);

		if (AFP_SUCCESS(afpError))
		{
			//
			//Now call the volume routine to actually flag the vol as open.
			//This call could fail if the user currently has too many volumes
			//already open.
			//
			afpError = afpVolume->fp_OpenVolume(afpSession);

			if (AFP_SUCCESS(afpError))
			{
				//
				//Get the size of the volume parameter data.
				//
				*afpDataSize = afpReply.GetDataLength();
			}
		}
	}
	else
	{
		//
		//If we get here, we couldn't find the requested volume.
		//
		afpError = afpParmErr;

		DBGWRITE(dbg_level_trace, "Failed to find named volume\n");
	}

	return( afpError );
}


/*
 * FPCloseVol()
 *
 * Description:
 *		Close a volume from access to this user session.
 *
 * Returns: AFPERROR
 */

AFPERROR FPCloseVol(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpDataSize)

	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer);
	fp_volume*	afpVolume	= NULL;
	uint16		afpVolumeID	= 0;
	AFPERROR	afpError 	= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word contains the afp command and padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	//
	//Get the volume ID requested to be closed.
	//
	afpVolumeID = afpRequest.GetInt16();

	//
	//Get the volume object in charge of the volume
	//
	afpVolume = FindVolume(afpVolumeID);

	//
	//Now, if we found the volume and the user has it open, perform
	//the close operation.
	//
	if ((afpVolume != NULL) && (afpSession->HasVolumeOpen(afpVolume)))
	{
		afpError = afpVolume->fp_CloseVolume(afpSession);
	}

	return( afpError );
}


/*
 * FPGetVolParms()
 *
 * Description:
 *		Returns the parameters of a specified volume.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetVolParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer);
	int16		afpVolumeID		= 0;
	int16		afpVolBitmap	= 0;
	fp_volume*	afpVolume		= NULL;
	AFPERROR	afpError 		= AFP_OK;

	//
	//The first word contains the afp command and padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	//
	//Get the volume ID the client wants to get parms for.
	//
	afpVolumeID = afpRequest.GetInt16();

	//
	//Now search for the volume object associated with the volume name.
	//
	afpVolume = FindVolume(afpVolumeID);

	//
	//If we found the volume using the volID, then afpSession will
	//not be null. The client must also have previously opened the
	//volume using FPOpenVol.
	//
	if ((afpVolume != NULL) && (afpSession->HasVolumeOpen(afpVolume)))
	{
		//
		//Get the bitmap the describes what parms we are going to supply.
		//
		afpVolBitmap = afpRequest.GetInt16();

		//
		//Now call on the volume object to get the information requested.
		//
		afpError = afpVolume->fp_GetVolParms(afpVolBitmap, afpReply);

		//
		//Lastly, if we succeed, get the afp data length.
		//
		if (AFP_SUCCESS(afpError))
		{
			*afpDataSize = afpReply.GetDataLength();
		}
	}
	else
	{
		//
		//Either the volume ID is bad or the user did not have the
		//volume opened for access.
		//
		afpError = afpParmErr;

		DBGWRITE(dbg_level_error, "Bad ID or vol not opened!\n");
	}

	return( afpError );
}


/*
 * FPResolveID()
 *
 * Description:
 *		Returns file parameters for a given file ID.
 *
 * Returns: AFPERROR
 */

AFPERROR FPResolveID(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer);

	DBGWRITE(dbg_level_trace, "FPResolveID\n");

	afpRequest.Advance(sizeof(int16));

	auto afpVolumeID = afpRequest.GetInt16();
	auto afpFileID = afpRequest.GetInt32();
	auto afpBitmap = afpRequest.GetInt16();

	//
	//Get a pointer to the volume object we'll be working with
	//
	auto afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	BEntry afpEntry;
	auto afpError = fp_objects::GetEntryFromFileId(afpVolume->GetDirectory(), afpFileID, afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Could not find file base on ID!\n");
		return( afpError );
	}

	if ((!afpEntry.IsFile()) && (!afpEntry.IsSymLink()))
	{
		DBGWRITE(dbg_level_warning, "File object is neither a file, or a symlink\n");
		return( afpParmErr );
	}

	BPath path;
	if (afpEntry.GetPath(&path) != B_OK)
	{
		DBGWRITE(dbg_level_warning, "Failed to get entry path!\n");
		return( afpParmErr );
	}

	DBGWRITE(dbg_level_trace, "Found file via ID (%lu): %s\n", afpFileID, path.Path());

	//
	//Don't support the following bitmaps.
	//
	if (afpSession->GetAFPVersion() < afpVersion30)
	{
		if (afpBitmap & kFPProDos)
		{
			afpBitmap &= ~kFPProDos;
		}
	}

	if (afpBitmap & kFPShortName)
	{
		afpBitmap &= ~kFPShortName;
	}

	//
	//Return the bitmap that tells what information the rest
	//of the buffer contains.
	//
	afpReply.push_num(afpBitmap);

	//
	//If the bitmap is empty, then we return nothing but the bitmap.
	//
	if (afpBitmap == kFPFileNone)
	{
		*afpDataSize = afpReply.GetDataLength();
		return( AFP_OK );
	}

	afpError = fp_objects::fp_GetFileParms(
								afpSession,
								afpVolume,
								&afpEntry,
								afpBitmap,
								&afpReply
								);

	if (AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_trace, "Data length: %lu\n", afpReply.GetDataLength());

		*afpDataSize = afpReply.GetDataLength();
	}

	DBGWRITE(dbg_level_trace, "Returninjg: %lu\n", afpError);

	return( afpError );
}


/*
 * FPGetFileDirParms()
 *
 * Description:
 *		Returns file or dir parameters for a given path.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetFileDirParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer);
	char		afpPathname[MAX_AFP_PATH];
	BEntry		afpEntry;
	fp_volume*	afpVolume		= NULL;
	uint16		afpVolumeID		= 0;
	int32		afpDirID		= 0;
	int16		afpFileBitmap	= 0;
	int16		afpDirBitmap	= 0;
	int8		afpPathType		= 0;
	int8		afpAttributes	= 0;
	AFPERROR	afpError 		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word contains the afp command and padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	//
	//Get the parms that define what we're to do and on what.
	//
	afpVolumeID 	= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpFileBitmap	= afpRequest.GetInt16();
	afpDirBitmap	= afpRequest.GetInt16();
	afpPathType		= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	DBGWRITE(dbg_level_trace, "DirID = %lu\n", afpDirID);
	DBGWRITE(dbg_level_trace, "Pathname = %s\n", afpPathname);

	//
	//If the object is a directory, then set the attribute
	//bit that informs the client.
	//
	if (afpEntry.IsDirectory()) {

		afpAttributes |= kFileDirIsDir;
	}

	//
	//We don't support the following bitmaps, so we clear them. Note that
	//we don't clear the KFPProDos bit in AFP3.x since it is now the unicode
	//name bit.
	//
	if (afpSession->GetAFPVersion() < afpVersion30)
	{
		if (afpDirBitmap & kFPProDos)	afpDirBitmap  &= ~kFPProDos;
		if (afpFileBitmap & kFPProDos)	afpFileBitmap &= ~kFPProDos;
	}

	if (afpDirBitmap & kFPDirShortName)		afpDirBitmap  &= ~kFPDirShortName;
	if (afpFileBitmap & kFPShortName)		afpFileBitmap &= ~kFPShortName;

	//
	//The following items are returned in all versions of this call
	//regardless of the bitmaps or object type.
	//
	afpReply.AddInt16(afpFileBitmap);
	afpReply.AddInt16(afpDirBitmap);
	afpReply.AddInt8(afpAttributes);

	//
	//If both bitmaps are null, then we are to just return
	//the bitmaps and the attributes.
	//
	if ((afpFileBitmap == kFPFileNone) && (afpDirBitmap == kFPDirNone))
	{
		*afpDataSize = afpReply.GetDataLength();
		return( AFP_OK );
	}

	//
	//Add a padding byte if we're giving more.
	//
	afpReply.AddInt8(0);

	if (afpEntry.IsDirectory())
	{
		afpError = fp_objects::fp_GetDirParms(
									afpSession,
									afpVolume,
									&afpEntry,
									afpDirBitmap,
									&afpReply
									);
	}
	else if ((afpEntry.IsFile()) || (afpEntry.IsSymLink()))
	{
		afpError = fp_objects::fp_GetFileParms(
									afpSession,
									afpVolume,
									&afpEntry,
									afpFileBitmap,
									&afpReply
									);
	}

	if (AFP_SUCCESS(afpError)) {

		*afpDataSize = afpReply.GetDataLength();
	}
	else {

		DBGWRITE(dbg_level_trace, "Returning: %lu\n", afpError);
	}

	return( afpError );
}


/*
 * FPEnumerate()
 *
 * Description:
 *		Enumerates the files and directorys within a directory.
 *
 * Returns: AFPERROR
 */

AFPERROR FPEnumerate(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	char		afpPathname[MAX_AFP_PATH];
	BEntry		afpEntry;
	fp_volume*	afpVolume		= NULL;
	int16*		afpActCountSpot	= NULL;
	int8		afpCommand		= 0;
	int16		afpActCount		= 0;
	int8		afpAttributes	= 0;
	int16		afpObjectsFound	= 0;
	int16		afpVolID		= 0;
	int32		afpDirID		= 0;
	int16		afpFileBitmap	= 0;
	int16		afpDirBitmap	= 0;
	int16		afpReqCount		= 0;
	int32		afpStartIndex	= 0;
	int32		afpMaxReplySize	= 0;
	int8		afpPathType		= 0;
	AFPERROR	afpError		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word contains the afp command and padding byte.
	//
	afpCommand = afpRequest.GetInt8();
	afpRequest.Advance(sizeof(int8));

	//
	//Extract the enumerate parameters for the API.
	//
	afpVolID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpFileBitmap	= afpRequest.GetInt16();
	afpDirBitmap	= afpRequest.GetInt16();
	afpReqCount		= afpRequest.GetInt16();

	//
	//In AFP3.1 we may get the new Ext2 call which allows for a
	//larger number of files to be on the volume.
	//
	switch(afpCommand)
	{
		case afpEnumerate:
		case afpEnumerateExt:
			afpStartIndex	= afpRequest.GetInt16();
			afpMaxReplySize	= afpRequest.GetInt16();
			break;

		case afpEnumerateExt2:
			afpStartIndex	= afpRequest.GetInt32();
			afpMaxReplySize	= afpRequest.GetInt32();
			break;

		default:
			return( afpParmErr );
	}

	afpPathType	= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//06.12.09: There should be no reason to enumerate the parent of the
	//root directory. We must inhibit it as hackers could then gain access
	//to the entire disk and its contents.
	//
	if (afpDirID == kParentOfRoot)
	{
		DBGWRITE(dbg_level_warning, "Parent of root directory is not allowed\n");
		return( afpParmErr );
	}

	//
	//We don't support the following bitmaps, so we clear them.
	//
	if (afpSession->GetAFPVersion() < afpVersion30)
	{
		if (afpDirBitmap & kFPDirProDOS)	afpDirBitmap  &= ~kFPDirProDOS;
		if (afpFileBitmap & kFPProDos)		afpFileBitmap &= ~kFPProDos;
	}

	if (afpDirBitmap & kFPDirShortName)		afpDirBitmap  &= ~kFPDirShortName;
	if (afpFileBitmap & kFPShortName)		afpFileBitmap &= ~kFPShortName;

	//
	//The first thing we do is add the bitmaps to the reply buffer.
	//
	afpReply.AddInt16(afpFileBitmap);
	afpReply.AddInt16(afpDirBitmap);

	//
	//This is where the actual number of bytes in the buffer will
	//be inserted into the buffer.
	//
	afpActCountSpot = (int16*)afpReply.GetCurrentPosPtr();
	afpReply.Advance(sizeof(int16));

	//
	//Set the entry object that will point to the directory we're enumerating.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	DBGWRITE(dbg_level_trace, "Enumerating directory: dirID: %lu, path: %s at index: %lu\n",
			afpDirID, afpPathname, afpStartIndex);

	//
	//OK, now the hard part. We need to iterate through all the directories
	//children and include them in the buffer until it is full.
	//
	BDirectory	directory(&afpEntry);
	BEntry		entry;

	//
	//Make sure the directory object got intialized properly.
	//
	if (directory.InitCheck() != B_OK)
	{
		DBGWRITE(dbg_level_error, "Directory object failed to initialize (dirID = %lu)\n", afpDirID);
		return( afpObjectNotFound );
	}

	while(directory.GetNextEntry(&entry) != B_ENTRY_NOT_FOUND)
	{
		bool 		afpIsDirectory;
		int8		tempBuffer[1024];
		afp_buffer	afpParmsBuffer(tempBuffer);

		afpIsDirectory = entry.IsDirectory();

		//
		//Check to make sure we don't include items not to be included
		//in the return enumeration.
		//
		if (	((afpDirBitmap == kFPDirNone) && (afpIsDirectory))		||
				((afpFileBitmap == kFPFileNone) && (!afpIsDirectory))	)
		{
			continue;
		}

		//
		//Call the appropriate GetXXXParms calls.
		//
		afpParmsBuffer.Rewind();
		afpAttributes = 0;

		if (afpIsDirectory)
		{
			//
			//Check to make sure the session has search access to the directory
			//before including directories in the enumeration.
			//
			afpError = afpCheckSearchAccess(afpSession, &afpEntry);
			if (AFP_FAILURE(afpError))
			{
				DBGWRITE(dbg_level_warning, "User doesn't have search access to the directory, hiding folders!\n");
				continue;
			}

			//
			//Set the afp flag that marks this as a directory.
			//
			afpAttributes |= kFileDirIsDir;

			afpError = fp_objects::fp_GetDirParms(
										afpSession,
										afpVolume,
										&entry,
										afpDirBitmap,
										&afpParmsBuffer
										);
		}
		else
		{
			//
			//Check to make sure the session has read access to the directory
			//before including any files.
			//
			afpError = afpCheckReadAccess(afpSession, &afpEntry);
			if (AFP_FAILURE(afpError))
			{
				DBGWRITE(dbg_level_warning, "User doesn't have read access to the directory, hiding files!\n");
				continue;
			}

			afpError = fp_objects::fp_GetFileParms(afpSession, afpVolume, &entry, afpFileBitmap, &afpParmsBuffer);
		}

		if (AFP_SUCCESS(afpError))
		{
			//
			//This is how many total objects we found during our search.
			//
			afpObjectsFound++;

			//
			//If we haven't hit the start index yet, continue on...
			//
			if (afpObjectsFound < afpStartIndex) {
				continue;
			}

			int8*	structLen 		= NULL;
			int16*	extStructLen	= NULL;
			int32	sizeRequired;
			int16	len;

			//
			//Check to make sure there is enough room in the buffer to add the
			//file/dir information.
			//
			sizeRequired = afpParmsBuffer.GetDataLength() + afpReply.GetDataLength();

			//
			//There are items after the file dirs parms that are added to the buffer,
			//so we add a little extra to be safe.
			//
			sizeRequired += (4);

			//
			//wMaxReplySize is the maximum size our return buffer can be.
			//
			if (sizeRequired >= afpMaxReplySize)
			{
				//
				//We need to stay inside the callers max reply size in the request.
				//
				break;
			}

			//
			//Make sure we didn't get bigger then our reply buffer can handle.
			//
			if (sizeRequired > afpReply.GetBufferSize())
			{
				//
				//This should never happen (yeah right). In case it does, just
				//bail from here.
				//
				break;
			}

			//
			//Increment the count of the # of objects we're returning.
			//
			afpActCount++;

			//
			//The first byte (or word for AFP3.x) contains the struct len.
			//
			if (afpCommand > afpEnumerate)
			{
				extStructLen = (int16*)afpReply.GetCurrentPosPtr();
				afpReply.Advance(sizeof(int16));
			}
			else
			{
				structLen = afpReply.GetCurrentPosPtr();
				afpReply.Advance(sizeof(int8));
			}

			//
			//Push in the attributes flag (dir, yes/no)
			//
			afpReply.AddInt8(afpAttributes);

			//
			//For AFP3.x, we add an extra null byte for padding here.
			//
			if (afpCommand > afpEnumerate) {
				afpReply.AddInt8(0);
			}

			//
			//Insert the FDP parameters into the reply buffer.
			//
			afpReply.AddRawData(afpParmsBuffer.GetBuffer(), afpParmsBuffer.GetDataLength());

			//
			//Set the structure length byte in the buffer. The extra bytes are
			//for the first parms in the structure (structLen, attributes and padding).
			//
			len = afpParmsBuffer.GetDataLength() + ((afpCommand > afpEnumerate) ? 4 : 2);

			//
			//This entry must end on an even boundary.
			//
			if (len % 2)
			{
				afpReply.AddInt8(0);
				len++;
			}

			//
			//AFP3.x uses a word to describe the struct length vs. a byte
			//
			if (afpCommand > afpEnumerate)
			{
				*extStructLen = htons(len);
			}
			else {
				*structLen = len;
			}
		}
		else
		{
			//
			//We had an error looking up the parms of an object.
			//
			DBGWRITE(dbg_level_error, "Error finding parms for object\n");
		}

		//
		//If we've gotten as much data as the client requested, then
		//exit this beast.
		//
		if ((afpActCount >= afpReqCount) || (afpReply.GetDataLength() >= afpMaxReplySize))
		{
			break;
		}
	}

	//
	//Set the actual number of objects found and in the buffer.
	//
	*afpActCountSpot = htons(afpActCount);

	DBGWRITE(dbg_level_trace, "Number of objects enumerated = %lu\n", afpActCount);

	//
	//This is the total size of the afp reply.
	//
	*afpDataSize = afpReply.GetDataLength();

	return((afpActCount == 0) ? afpObjectNotFound : AFP_OK);
}


/*
 * FPSetFileDirParms()
 *
 * Description:
 *		Set file or directory parameters.
 *
 * Returns: AFPERROR
 */

AFPERROR FPSetFileDirParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer	afpRequest(afpReqBuffer);
	char		afpPathname[MAX_AFP_PATH];
	BEntry		afpEntry;
	int8		afpCommand		= 0;
	fp_volume*	afpVolume		= NULL;
	int16		afpVolID		= 0;
	int16		afpBitmap		= 0;
	int32		afpDirID		= 0;
	int8		afpPathType		= 0;
	AFPERROR	afpError 		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word is afp command and padding byte.
	//
	afpCommand = afpRequest.GetInt8();
	afpRequest.Advance(sizeof(int8));

	//
	//Extract the volume and dir ID's as well as the bitmap that
	//tells what we're setting.
	//
	afpVolID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpBitmap	= afpRequest.GetInt16();
	afpPathType	= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//The remaining parameters for the call must start on an
	//even boundary.
	//
	if ((afpRequest.GetCurrentPosPtr() - afpRequest.GetBuffer()) % 2) {

		afpRequest.Advance(sizeof(int8));
	}

	//
	//Verify the bitmap contains valid settings, FPSetFileDirParms doesn't
	//support the following bitmaps. The user is supposed to use
	//FPSetFileParms or FPSetDirParms for these.
	//
	switch(afpCommand)
	{
		case afpSetFlDrParms:
			if ((afpBitmap & (	kFPParentID	  | kFPLongName | kFPShortName |
								kFPFileNum    | kFPDFLen    | kFPRFLen	   |
								kFPDirGroupID | kFPDirAccess | kFPDirUnicodeName |
								kFPExtDataForkLen | kFPExtRsrcForkLen)))
			{
				DBGWRITE(dbg_level_warning, "Bad bitmap!\n");
				return( afpBitmapErr );
			}
			break;

		case afpSetDirParms:
			if ((afpBitmap & (	kFPParentID	   | kFPLongName | kFPShortName |
								kFPDirOffCount | kFPDirID | kFPDirUnicodeName)))
			{
				DBGWRITE(dbg_level_warning, "Bad bitmap!\n");
				return( afpBitmapErr );
			}
			break;

		case afpSetFileParms:
			if ((afpBitmap & (	kFPParentID	| kFPLongName | kFPShortName |
								kFPFileNum  | kFPDFLen    | kFPRFLen     |
								kFPExtDataForkLen | kFPUnicodeName | kFPExtRsrcForkLen)))
			{
				DBGWRITE(dbg_level_warning, "Bad bitmap!\n");
				return( afpBitmapErr );
			}
			break;

		default:
			ASSERT(0);
			return( afpCallNotSupported );
	}

	//
	//Set the entry object that will point to the object we're changing.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	//
	//Check to make sure we are allowed to write on the volume.
	//
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have write access!\n");
		return( afpError );
	}

	//
	//Call the work routine to do all the work for us.
	//
	afpError = fp_objects::fp_SetFileDirParms(afpSession, &afpRequest, &afpEntry, afpBitmap);

	afpVolume->MakeDirty();

	return( afpError );
}


/*
 * FPCreateDir()
 *
 * Description:
 *		Creates a new directory.
 *
 * Returns: AFPERROR
 */

AFPERROR FPCreateDir(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer);
	char		afpPathname[MAX_AFP_PATH];
	BEntry		afpEntry;
	fp_volume*	afpVolume		= NULL;
	int16		afpVolumeID		= 0;
	int32		afpDirID		= 0;
	int8		afpPathType		= 0;
	AFPERROR	afpError		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word contains the afp command and padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	//
	//Extract the enumerate parameters for the API.
	//
	afpVolumeID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpPathType		= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Pathname cannot be null since it contains the new dir name.
	//
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_warning, "Null pathname!\n");
		return( afpParmErr );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								NULL,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	//
	//Check to make sure we are allowed to write on the volume.
	//
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have write access!\n");
		return( afpError );
	}

	//
	//Now we use the B API's to create the new directory in the
	//filesystem.
	//
	BDirectory	dir(&afpEntry);

	if (dir.InitCheck() == B_OK)
	{
		BDirectory	newdir;
		node_ref	nodeRef;
		status_t	status;

		status = dir.CreateDirectory(afpPathname, &newdir);

		if (status == B_OK)
		{
			newdir.GetNodeRef(&nodeRef);
			afpReply.AddInt32(nodeRef.node);
		}
		else
		{
			DBGWRITE(dbg_level_error, "Failed to create directory! (%s)\n", GET_BERR_STR(status));

			switch(status)
			{
				case B_FILE_EXISTS:			afpError = afpObjectExists; break;
				case B_PERMISSION_DENIED:	afpError = afpAccessDenied;	break;

				default:
					afpError = afpParmErr;
			}
		}
	}

	if (AFP_SUCCESS(afpError))
	{
		BEntry	newEntry(&dir, afpPathname);
		mode_t	bperms = 0;

		if (newEntry.InitCheck() == B_OK)
		{
			//
			//If the new name is longer than kLongNames can handle, create
			//the longname and store it.
			//
			if (strlen(afpPathname) > MAX_AFP_2_NAME)
			{
				fp_objects::CreateLongName(
								NULL,
								&newEntry,
								true
								);
			}

			//
			//In Haiku, every new directory is currently being set denying
			//write permissions to users and guests. We'll set everything to
			//match the parent directory.
			//

			dir.GetPermissions(&bperms);
			newEntry.SetPermissions(bperms);
		}

		//
		//We need to signal to all clients that their picture of this
		//volume has changed.
		//

		afpVolume->MakeDirty();
	}

	*afpDataSize = afpReply.GetDataLength();

	return( afpError );
}


/*
 * FPFlush()
 *
 * Description:
 *		Flushes the volume. We don't do anything for BeOS.
 *
 * Returns: AFPERROR
 */

AFPERROR FPFlush(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	fp_volume*		afpVolume	= NULL;
	int16			afpVolumeID	= 0;
	AFPERROR		afpError	= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word contains the afp command and padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	afpVolumeID = afpRequest.GetInt16();

	//
	//For the BeOS, we don't have anything we need to do here. So,
	//we don't do anything other than verify that the volume is
	//indeed open and parameters are correct.
	//

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	return( afpError );
}


/*
 * FPFlushFork()
 *
 * Description:
 *		Flushes an open file fork.
 *
 * Returns: AFPERROR
 */

AFPERROR FPFlushFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	int16			afpForkRef	= 0;
	OPEN_FORK_ITEM*	forkItem	= NULL;
	AFPERROR		afpError	= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word contains the afp command and padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	afpForkRef	= afpRequest.GetInt16();
	forkItem 	= afpSession->GetForkItem(afpForkRef);
	afpError 	= (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_SUCCESS(afpError))
	{
		forkItem->mutex->Lock();

		if (forkItem->forkopen == kDataFork)
		{
			//
			//Sync the file using the filesystem API after making sure
			//the file is valid and exists.
			//
			if ((forkItem->file != NULL) 				&&
				(forkItem->file->InitCheck() == B_OK)	&&
				(forkItem->file->IsWritable())			)
			{
				forkItem->file->Sync();
			}
		}
		else
		{
			//
			//We call the resource fork closing method, but we tell it not
			//to actually delete the memory block which causes just the
			//file on disk to be updated.
			//
			afpSession->CloseAndWriteOutResourceFork(forkItem, false);
		}

		forkItem->mutex->Unlock();
	}

	return( afpError );
}


/*
 * FPCloseFork()
 *
 * Description:
 *		Closes an open file fork.
 *
 * Returns: AFPERROR
 */

AFPERROR FPCloseFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer	afpRequest(afpReqBuffer);
	int16		afpRefNum	= 0;
	AFPERROR	afpError	= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first word contains the afp command and padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	//
	//Get the file ref num we're closing down.
	//
	afpRefNum 	= afpRequest.GetInt16();
	afpError 	= afpSession->CloseFile(afpRefNum);

	DBGWRITE(dbg_level_trace, "Closed fork %u, Returning %lu\n", afpRefNum, afpError);

	return( afpError );
}


/*
 * FPOpenFork()
 *
 * Description:
 *		Open a file fork.
 *
 * Returns: AFPERROR
 */

AFPERROR FPOpenFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer);
	char		afpPathname[MAX_AFP_PATH];
	BEntry		afpEntry;
	int8		afpFork			= 0;
	fp_volume*	afpVolume		= NULL;
	int16		afpVolumeID		= 0;
	int32		afpDirID		= 0;
	int8		afpPathType		= 0;
	int16		afpBitmap		= 0;
	int16		afpMode			= 0;
	uint16		afpNewRefNum	= 0;
	AFPERROR	afpError		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first byte contains the afp command.
	//
	afpRequest.Advance(sizeof(int8));

	//
	//Get the parameters for the file we're opening.
	//
	afpFork		= (afpRequest.GetInt8() & kResourceForkBit) ? kRsrcFork : kDataFork;
	afpVolumeID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpBitmap	= afpRequest.GetInt16();
	afpMode		= afpRequest.GetInt16();
	afpPathType	= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Pathname cannot be null since it contains the file to open.
	//
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_trace, "Null pathname!\n");
		return( afpParmErr );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found = %s\n", afpPathname);
		return( afpError );
	}

	//
	//Check to make sure we are allowed to write on the volume.
	//
	if (afpMode & kWriteMode)
	{
		afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

		if (!AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_warning, "User doesn't have write access to the file! (%s)\n", afpPathname);

			//
			//If the caller is requesting write access, we deny the opening since
			//the file cannot be written to.
			//
			return( afpError );
		}
	}

	//
	//Check for read access to the file.
	//
	if (afpMode & kReadMode)
	{
		afpError = afpCheckReadAccess(afpSession, &afpEntry);

		if (!AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_warning, "User doesn't have read access to the file!\n");

			//
			//If the caller is requesting read access, we deny the opening since
			//the file cannot be written to.
			//
			return( afpError );
		}
	}

	//
	//Use the session object to actually open the file. The session is
	//the object that keeps track of open files.
	//
	afpError = afpSession->OpenFile(afpVolume, &afpEntry, afpMode, afpFork, &afpNewRefNum);

	if (AFP_SUCCESS(afpError))
	{
		//
		//We don't support the following bitmaps, so we clear them.
		//
		if (afpSession->GetAFPVersion() < afpVersion30)
		{
			if (afpBitmap & kFPProDos)		afpBitmap  &= ~kFPProDos;
			if (afpBitmap & kFPShortName)	afpBitmap  &= ~kFPShortName;
		}

		//
		//Add in the bitmap and new refnum id.
		//
		afpReply.AddInt16(afpBitmap);
		afpReply.AddInt16(afpNewRefNum);

		//
		//Now that we've opened the file, get the requested parameters.
		//
		afpError = fp_objects::fp_GetFileParms(afpSession, afpVolume, &afpEntry, afpBitmap, &afpReply);

		if (AFP_SUCCESS(afpError))
		{
			*afpDataSize = afpReply.GetDataLength();

			DBGWRITE(dbg_level_trace, "Fork (%lu) is open for '%s', with refNum: %lu\n", afpFork, afpPathname, afpNewRefNum);
		}
		else
		{
			DBGWRITE(dbg_level_warning, "Error getting parms! %lu\n", afpError);
			afpSession->CloseFile(afpNewRefNum);
		}
	}

	DBGWRITE(dbg_level_trace, "Returning %lu\n", afpError);

	return( afpError );
}


/*
 * FPSetForkParms()
 *
 * Description:
 *		Set the data or rsrc fork lengths.
 *
 * Returns: AFPERROR
 */

AFPERROR FPSetForkParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	int16			afpForkRef		= 0;
	int16			afpBitmap		= 0;
	off_t			afpForkLen		= 0;
	AFPERROR		afpError		= AFP_OK;
	OPEN_FORK_ITEM*	forkItem		= NULL;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first 2 bytes contain the afp command and padding.
	//
	afpRequest.Advance(sizeof(int16));

	afpForkRef 	= afpRequest.GetInt16();
	afpBitmap	= afpRequest.GetInt16();
	afpForkLen	= afpRequest.GetInt32();

	forkItem 	= afpSession->GetForkItem(afpForkRef);
	afpError 	= (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_SUCCESS(afpError))
	{
		//
		//Check to make sure this session has write access to this file
		//or directory.
		//
		afpError = afpCheckWriteAccess(afpSession, NULL, forkItem->entry);

		if (!AFP_SUCCESS(afpError))
		{
			return( afpError );
		}

		if ((afpBitmap & kFPDFLen) || (afpBitmap & kFPExtDataForkLen))
		{
			DBGWRITE(dbg_level_trace, "Setting data fork length: %lu\n", afpForkLen);

			if (forkItem->forkopen != kDataFork)
			{
				DBGWRITE(dbg_level_warning, "Data fork is not open!\n");
				return( afpBitmapErr );
			}

			switch(forkItem->file->SetSize(afpForkLen))
			{
				case B_OK:			afpError = AFP_OK;			break;
				case B_NOT_ALLOWED:	afpError = afpVolLocked;	break;
				case B_DEVICE_FULL:	afpError = afpDiskFull;		break;

				default:			afpError = afpMiscErr;		break;
			}
		}

		if ((afpBitmap & kFPRFLen) || (afpBitmap & kFPExtRsrcForkLen))
		{
			BNode	node(forkItem->entry);

			DBGWRITE(dbg_level_trace, "Setting rsrc fork length: %lld\n", afpForkLen);

			if (forkItem->forkopen != kRsrcFork)
			{
				DBGWRITE(dbg_level_warning, "Resource fork is not open!\n");
				return( afpBitmapErr );
			}

			forkItem->rsrcIO->SetSize(afpForkLen);

			//
			//If we set the size of the fork, we need to write out the entire
			//fork to disk so that subsequent GetFileParms calls will return
			//the proper RF length.
			//
			node.RemoveAttr(AFP_RSRC_ATTRIBUTE);

			//
			//06.02.09: Fixed bug where older mac clients appear to attempt to write
			//zero bytes to the file if there is no resource fork. This, of course,
			//returns an error that the older clients can't deal with, so they fail.
			//
			if (afpForkLen > 0)
			{
				//
				//Now write the data back to the resource stream of the file.
				//
				afpError = (node.WriteAttr(
								AFP_RSRC_ATTRIBUTE,
								B_RAW_TYPE,
								0,
								forkItem->rsrcIO->Buffer(),
								forkItem->rsrcIO->BufferLength() ) < B_OK) ? afpParmErr : AFP_OK;
			}
		}
	}
	else
	{
		DBGWRITE(dbg_level_trace, "Fork item not found!\n");
	}

	DBGWRITE(dbg_level_trace, "Returning %lu\n", afpError);

	return( afpError );
}


/*
 * FPGetForkParms()
 *
 * Description:
 *		Retrieves the parameters (FileParms) for an open fork file.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetForkParms(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	int16			afpForkRef		= 0;
	int16			afpBitmap		= 0;
	AFPERROR		afpError		= AFP_OK;
	OPEN_FORK_ITEM*	forkItem		= NULL;

	//
	//The first byte contains the afp command.
	//
	afpRequest.Advance(sizeof(int16));

	//
	//Get the parameters for the file we're opening.
	//
	afpForkRef	= afpRequest.GetInt16();
	afpBitmap	= afpRequest.GetInt16();

	DBGWRITE(dbg_level_trace, "Enter, getting parms for: %lu\n", afpForkRef);

	forkItem = afpSession->GetForkItem(afpForkRef);
	afpError = (forkItem == NULL) ? afpParmErr : AFP_OK;

	if (AFP_SUCCESS(afpError))
	{
		//
		//We don't support the following bitmaps, so we clear them.
		//
		if (afpSession->GetAFPVersion() < afpVersion30)
		{
			if (afpBitmap & kFPProDos)		afpBitmap  &= ~kFPProDos;
			if (afpBitmap & kFPShortName)	afpBitmap  &= ~kFPShortName;
		}

		//
		//Make sure the caller is asking for the length of the fork
		//that is actually opened.
		//
		if (((afpBitmap & kFPDFLen) && (forkItem->forkopen == kRsrcFork))	||
			((afpBitmap & kFPRFLen) && (forkItem->forkopen == kDataFork))	)
		{
			DBGWRITE(dbg_level_warning, "Attempt to get length of wrong fork!\n");
			return( afpBitmapErr );
		}

		//
		//Add in the bitmap
		//
		afpReply.AddInt16(afpBitmap);

		//
		//Now that we've opened the file, get the requested parameters.
		//
		afpError = fp_objects::fp_GetFileParms(
									afpSession,
									forkItem->volume,
									forkItem->entry,
									afpBitmap,
									&afpReply
									);

		if (AFP_SUCCESS(afpError))
		{
			*afpDataSize = afpReply.GetDataLength();
		}
	}

	DBGWRITE(dbg_level_trace, "Returning %lu\n", afpError);

	return( afpError );
}


/*
 * FPCreateFile()
 *
 * Description:
 *		Creates a new file on the disk.
 *
 * Returns: AFPERROR
 */

AFPERROR FPCreateFile(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	char			afpPathname[MAX_AFP_PATH];
	BEntry			afpEntry;
	fp_volume*		afpVolume		= NULL;
	int8			createFlag		= 0;
	int16			afpVolumeID		= 0;
	int32			afpDirID		= 0;
	int8			afpPathType		= 0;
	AFPERROR		afpError		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//First byte is afp command.
	//
	afpRequest.Advance(sizeof(int8));

	createFlag	= afpRequest.GetInt8();
	afpVolumeID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathType	= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Failed to get pathname from afp blob!\n");
		return( afpError );
	}

	//
	//Pathname cannot be null since it contains the new file name.
	//
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_warning, "Null pathname!\n");
		return( afpParmErr );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								NULL,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Directory not found! (dirID = %lu)\n", afpDirID);
		return( afpError );
	}

	//
	//Check to make sure we are allowed to write on the volume.
	//
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have write access!\n");
		return( afpError );
	}

	//
	//Now we use the B API's to create the new directory in the
	//filesystem.
	//
	BDirectory	dir(&afpEntry);

	if (dir.InitCheck() == B_OK)
	{
		status_t	status;

		DBGWRITE(dbg_level_trace, "Creating file: %s\n", afpPathname);

		status = dir.CreateFile(afpPathname, NULL, (createFlag & kHardCreate) ? false : true);

		DBGWRITE(dbg_level_trace, "CreateFile returned %s\n", GET_BERR_STR(status));

		switch(status)
		{
			case B_FILE_EXISTS:			afpError = afpObjectExists;	break;
			case B_NAME_TOO_LONG:		afpError = afpParmErr;		break;

			case B_NOT_ALLOWED:
			case B_PERMISSION_DENIED:	afpError = afpAccessDenied;	break;

			case B_OK:					afpError = AFP_OK;			break;
			default:					afpError = afpParmErr;		break;
		}
	}
	else
	{
		//
		//We failed to get an object for the directory.
		//
		DBGWRITE(dbg_level_warning, "dir.InitCheck() failed! (%s)\n", GET_BERR_STR(dir.InitCheck()));
		afpError = afpParmErr;
	}

	if (AFP_SUCCESS(afpError))
	{
		BEntry	newEntry(&dir, afpPathname);

		if (newEntry.InitCheck() == B_OK)
		{
			//
			//If the new name is longer than kLongNames can handle, create
			//the longname and store it.
			//
			if (strlen(afpPathname) > MAX_AFP_2_NAME)
			{
				fp_objects::CreateLongName(
								NULL,
								&newEntry,
								true
								);
			}
		}
		else
		{
			//
			//Something strange happened and we didn't actually create the file.
			//
			DBGWRITE(dbg_level_error, "newEntry.InitCheck() failed! (%s)\n", GET_BERR_STR(newEntry.InitCheck()));
			afpError = afpParmErr;
		}

		//
		//We need to signal to all clients that their picture of this
		//volume has changed.
		//
		afpVolume->MakeDirty();
	}

	DBGWRITE(dbg_level_trace, "Returning %lu\n", afpError);

	return( afpError );
}


/*
 * FPDelete()
 *
 * Description:
 *		Deletes a file or directory from the disk.
 *
 * Returns: AFPERROR
 */

AFPERROR FPDelete(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	char			afpPathname[MAX_AFP_PATH];
	BEntry			afpEntry;
	fp_volume*		afpVolume		= NULL;
	int16			afpVolumeID		= 0;
	int32			afpDirID		= 0;
	int8			afpPathType		= 0;
	AFPERROR		afpError		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));

	afpVolumeID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathType	= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Pathname cannot be null since it contains the file/dir to delete.
	//
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_warning, "Null pathname!\n");
		return( afpParmErr );
	}

	DBGWRITE(dbg_level_trace, "Deleting '%s' in dir %lu\n", afpPathname, afpDirID);

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	int16 afpAttributes = 0;
	if (AFP_SUCCESS(fp_objects::GetAFPAttributes(&afpEntry, &afpAttributes)))
	{
		if (afpAttributes & kFileDeleteInhibit)
		{
			DBGWRITE(dbg_level_warning, "File/Dir is locked and cannot be deleted! (%s)\n", afpEntry.Name());
			return( afpObjectLocked );
		}
	}

	//
	//We need to check write access to the parent directory.
	//
	BDirectory 	parent;
	BEntry		pEntry;

	afpEntry.GetParent(&parent);
	parent.GetEntry(&pEntry);

	afpError = afpCheckWriteAccess(afpSession, afpVolume, &pEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have write access to parent directory!\n");
		return( afpError );
	}

	//
	//Now call the object method that does all the nasty work for us.
	//
	status_t status = afpEntry.Remove();

	if (status != B_OK)
	{
		DBGWRITE(dbg_level_error, "Remove failed! reason = %s (%lu)\n", (status == B_NO_INIT) ? "B_NO_INIT" : "UNK", status);
		afpError = afpParmErr;
	}

	//
	//We need to signal to all clients that their picture of this
	//volume has changed.
	//
	if (AFP_SUCCESS(afpError)) {

		afpVolume->MakeDirty();
	}

	return( afpError );
}


/*
 * FPRead()
 *
 * Description:
 *		Read data from an open file.
 *
 * Returns: AFPERROR
 */

AFPERROR FPRead(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	off_t			seekResult		= 0;
	int8			afpCommand		= 0;
	int16			afpForkRef		= 0;
	off_t			afpOffset		= 0;
	size_t			afpReqCount		= 0;
	int32			afpActCount		= 0;
	AFPERROR		afpError		= AFP_OK;
	OPEN_FORK_ITEM*	forkItem		= NULL;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first byte contains the afp command.
	//
	afpCommand = afpRequest.GetInt8();
	afpRequest.Advance(sizeof(int8));

	afpForkRef	= afpRequest.GetInt16();

	switch(afpCommand)
	{
		case afpRead:
			afpOffset	= afpRequest.GetInt32();
			afpReqCount	= afpRequest.GetInt32();
			break;

		case afpReadExt:
			afpOffset	= afpRequest.GetInt64();
			afpReqCount	= (size_t)afpRequest.GetInt64();
			break;

		default:
			return( afpParmErr );
	}

	forkItem = afpSession->GetForkItem(afpForkRef);
	afpError = (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_SUCCESS(afpError))
	{
		if (afpReqCount > (size_t)afpReply.GetBufferSize())
		{
			//
			//The request is too big, reduce the count to what
			//we can hold. We'll return to the client what we
			//actually read.
			//
			afpReqCount = afpReply.GetBufferSize();

			DBGWRITE(dbg_level_info, "Resized afpReqCount to buffer size!!\n");
		}

		//
		//Check to see if any area in the range we're reading from is
		//locked.
		//
		if (fp_rangelock::RangeLocked(
						seekResult,
						seekResult + afpReqCount,
						forkItem->entry
						))
		{
			DBGWRITE(dbg_level_info, "****Range is currently locked!****\n");
			return( afpLockErr );
		}

		if (forkItem->forkopen == kDataFork)
		{
			if (!forkItem->file->IsReadable())
			{
				DBGWRITE(dbg_level_trace, "Data fork is not readable!\n");
				return( afpAccessDenied );
			}

			DBGWRITE(dbg_level_trace, "Reading (DF) %lu bytes from %lld offset\n", afpReqCount, afpOffset);

			//
			//Seek to the correct position to read from in the file.
			//
			seekResult = forkItem->file->Seek(afpOffset, SEEK_SET);

			if (seekResult == B_ERROR)
			{
				//
				//We had an error seeking to the position. Probably a bad
				//position was requested.
				//
				DBGWRITE(dbg_level_trace, "Seek() failed!\n");
				return( afpParmErr );
			}

			//
			//Now, perform the actual read from the file.
			//
			afpActCount = forkItem->file->Read(afpReply.GetCurrentPosPtr(), afpReqCount);
		}
		else //Reading from resource fork
		{
			DBGWRITE(dbg_level_trace, "Reading (RF) %lu bytes from %lld offset\n", afpReqCount, afpOffset);

			//
			//The resource fork should already be read in and in memory.
			//
			if (forkItem->rsrcIO != NULL)
			{
				//
				//Seek to the correct position to read from in the file.
				//
				seekResult = forkItem->rsrcIO->Seek(afpOffset, SEEK_SET);

				if (seekResult == B_ERROR)
				{
					//
					//We had an error seeking to the position. Probably a bad
					//position was requested.
					//
					DBGWRITE(dbg_level_error, "Seek() failed (RF)!\n");
					return( afpParmErr );
				}

				afpActCount = forkItem->rsrcIO->Read(
												afpReply.GetCurrentPosPtr(),
												afpReqCount
												);
			}
			else
			{
				DBGWRITE(dbg_level_error, "rsrcIO object is NULL!\n");
				return( afpParmErr );
			}
		}

		if (afpActCount < B_OK)
		{
			DBGWRITE(dbg_level_error, "Failed to read data!\n");

			afpError = afpMiscErr;
		}
		else if (afpReqCount > 0 && afpActCount == 0)
		{
			afpError = afpEofError;
		}
		else if (afpActCount > 0)
		{
			afpReply.Advance(afpActCount);
		}
	}

	*afpDataSize = afpActCount > 0 ? afpActCount : 0;

	DBGWRITE(dbg_level_trace, "Returning error %ld, act count: %ld\n", afpError, afpActCount);

	return( afpError );
}


/*
 * FPWrite()
 *
 * Description:
 *		Write data from an open file.
 *
 * Returns: AFPERROR
 */

AFPERROR FPWrite(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	int8			afpCommand		= 0;
	off_t			seekResult		= 0;
	int16			afpForkRef		= 0;
	off_t			afpOffset		= 0;
	size_t			afpReqCount		= 0;
	size_t			afpActCount		= 0;
	int8			afpFlag			= 0;
	AFPERROR		afpError		= AFP_OK;
	OPEN_FORK_ITEM*	forkItem		= NULL;

	//
	//The first byte contains the afp command.
	//
	afpCommand = afpRequest.GetInt8();

	afpFlag		= afpRequest.GetInt8();
	afpForkRef	= afpRequest.GetInt16();

	DBGWRITE(dbg_level_trace, "Enter, writing to forkref: %lu\n", afpForkRef);

	switch(afpCommand)
	{
		case afpWrite:
			afpOffset	= afpRequest.GetInt32();
			afpReqCount	= afpRequest.GetInt32();
			break;

		case afpWriteExt:
			afpOffset	= afpRequest.GetInt64();
			afpReqCount	= (size_t)afpRequest.GetInt64();
			break;

		default:
			return( afpParmErr );
	}

	forkItem = afpSession->GetForkItem(afpForkRef);
	afpError = (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_FAILURE(afpError))
	{
		DBGWRITE(dbg_level_error, "Error getting the open fork structure for forkref %lu!\n", afpForkRef);
		return afpError;
	}

	int16 afpAttributes = 0;
	if (AFP_SUCCESS(fp_objects::GetAFPAttributes(forkItem->entry, &afpAttributes)))
	{
		if (afpAttributes & kFileWriteInhibit)
		{
			DBGWRITE(dbg_level_warning, "File is locked and cannot be deleted! (%s)\n", forkItem->entry->Name());
			return( afpObjectLocked );
		}
	}

	if (forkItem->forkopen == kDataFork)
	{
		if (!forkItem->file->IsWritable())
		{
			DBGWRITE(dbg_level_warning, "Data fork is not writable!\n");
			return( afpParmErr );
		}

		//
		//If the bit is set, then we are calculating the offset from
		//the end of the file.
		//
		seekResult = forkItem->file->Seek(
						afpOffset,
						(afpFlag & kWriteStartEndFlag) ? SEEK_END : SEEK_SET
						);

		if (seekResult == B_ERROR)
		{
			//
			//We had an error seeking to the position. Probably a bad
			//position was requested.
			//
			DBGWRITE(dbg_level_warning, "Seek() failed!\n");
			return( afpParmErr );
		}

		DBGWRITE(dbg_level_info, "Writing (DF) %u bytes from %s at offset %u\n",
				afpReqCount,
				(afpFlag & kWriteStartEndFlag) ? "END" : "START",
				afpOffset
				);

		//
		//Check to see if any area in the range we're writing is
		//locked. Note that we call Position() here because we don't
		//easily know the offset (range start).
		//
		if (fp_rangelock::RangeLocked(
						seekResult,
						seekResult + afpReqCount,
						forkItem->entry
						))
		{
			DBGWRITE(dbg_level_warning, "****Range is currently locked!****\n");
			return( afpLockErr );
		}

		//
		//Call on the Be file object to do the BeOS specific file
		//system work for us.
		//
		afpActCount = forkItem->file->Write(
										afpRequest.GetCurrentPosPtr(),
										afpReqCount
										);

		if (afpActCount < B_OK)
		{
			DBGWRITE(dbg_level_warning, "Failed to write data!\n");
			afpError = afpMiscErr;
		}
		else
		{
			DBGWRITE(dbg_level_trace, "Actually wrote %lu bytes\n", afpActCount);

			switch(afpCommand)
			{
				case afpWrite:
					afpReply.AddInt32(forkItem->file->Position());
					break;

				case afpWriteExt:
					afpReply.AddInt64(forkItem->file->Position());
					break;
			}

			*afpDataSize = afpReply.GetDataLength();
		}
	}
	else //Writing to resource fork
	{
		//
		//Since the Be file system doesn't support resource forks, we have to
		//use the file attributes stream of a file to hold the Mac resource
		//data. Unfortunately, the WriteAttr() and ReadAttr() functions were
		//never finished by Be and their offset parameters don't work. This
		//means we have to read the entire contents of the stream in one shot
		//no matter how big it is, manipulate the contents, then write the
		//entire stream back out in one shot again. This won't scale very well.
		//

		DBGWRITE(dbg_level_trace, "Writing (RF) %lu bytes from %lu offset (from %s)\n",
				afpReqCount,
				afpOffset,
				(afpFlag & kWriteStartEndFlag) ? "END" : "START"
				);

		//
		//Check to see if any area in the range we're writing is
		//locked. Note that we call Position() here because we don't
		//easily know the offset (range start).
		//
		if (fp_rangelock::RangeLocked(
						afpOffset,
						afpOffset + afpReqCount,
						forkItem->entry
						))
		{
			DBGWRITE(dbg_level_warning, "****Range is currently locked!****\n");
			return( afpLockErr );
		}

		//
		//Move the file pointer to the right place in the file.
		//
		seekResult = forkItem->rsrcIO->Seek(
								afpOffset,
								(afpFlag & kWriteStartEndFlag) ? SEEK_END : SEEK_SET
								);

		if (seekResult == B_ERROR)
		{
			//
			//We had an error seeking to the position. Probably a bad
			//position was requested.
			//
			DBGWRITE(dbg_level_warning, "Seek() failed for resource fork!\n");
			return( afpParmErr );
		}

		//
		//Now, do the actual "write" into the buffer that holds our current
		//resource data.
		//
		afpActCount = forkItem->rsrcIO->Write(afpRequest.GetCurrentPosPtr(), afpReqCount);
		afpError 	= (afpActCount < B_OK) ? afpParmErr : AFP_OK;

		if (AFP_SUCCESS(afpError))
		{
			forkItem->rsrcDirty = true;

			switch(afpCommand)
			{
				case afpWrite:
					afpReply.AddInt32(forkItem->rsrcIO->Position());
					break;

				case afpWriteExt:
					afpReply.AddInt64(forkItem->rsrcIO->Position());
					break;
			}

			*afpDataSize = afpReply.GetDataLength();
		}
	}

	return( afpError );
}


/*
 * FPMoveAndRename()
 *
 * Description:
 *		Move and optionally rename a file or dir.
 *
 * Returns: AFPERROR
 */

AFPERROR FPMoveAndRename(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	char			afpPathname[MAX_AFP_PATH];
	char			afpNewPathname[MAX_AFP_NAME];
	BEntry			afpSrcEntry;
	BEntry			afpDstEntry;
	BDirectory		afpMoveToDir;
	fp_volume*		afpVolume		= NULL;
	int16			afpVolumeID		= 0;
	int32			afpSrcDirID		= 0;
	int32			afpDstDirID		= 0;
	int8			afpPathType		= 0;
	int16			afpAttributes	= 0;
	AFPERROR		afpError		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//The first byte contains the afp command.
	//
	afpRequest.Advance(sizeof(int16));

	afpVolumeID		= afpRequest.GetInt16();
	afpSrcDirID		= afpRequest.GetInt32();
	afpDstDirID		= afpRequest.GetInt32();
	afpPathType		= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//*****************
	//Beging by getting the source file/dir entry.
	//*****************

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpSrcDirID,
								afpPathname,
								afpSrcEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n",
				afpPathname,
				afpSrcDirID
				);

		return( afpError );
	}

	//
	//Check to make sure we are allowed to write on the volume.
	//
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpSrcEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_trace, "User doesn't have write access!\n");
		return( afpError );
	}

	//
	//Make sure we're allowed to move/rename this object.
	//
	afpError = fp_objects::GetAFPAttributes(&afpSrcEntry, &afpAttributes);

	if (AFP_SUCCESS(afpError))
	{
		if (afpAttributes & (kFileDeleteInhibit | kFileRenameInhibit))
		{
			return( afpObjectLocked );
		}
	}

	//*****************
	//Now get the destination BDirectory we're moving to
	//*****************

	afpPathType = afpRequest.GetInt8();

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDstDirID,
								afpPathname,
								afpDstEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n",
				afpPathname,
				afpDstDirID
				);

		return( afpError );
	}

	//
	//Check to make sure we are allowed to write on the volume.
	//
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpDstEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_trace, "User doesn't have write access!\n");
		return( afpError );
	}

	//
	//Create the BDirectory object.
	//
	afpMoveToDir.SetTo(&afpDstEntry);

	if (afpMoveToDir.InitCheck() != B_OK)
	{
		afpError = afpDirNotFound;
	}
	else
	{
		status_t	status;

		//
		//Get the (optionally) supplied new name after the move.
		//
		afpPathType = afpRequest.GetInt8();

		afpError = afpRequest.GetString(afpNewPathname, sizeof(afpNewPathname), true, afpPathType);

		if (!AFP_SUCCESS(afpError))
		{
			return( afpError );
		}

		//
		//If the source and destination dir ID's are the same, then we're
		//just renaming the file.
		//
		if (afpSrcDirID != afpDstDirID)
		{
			status = afpSrcEntry.MoveTo(&afpMoveToDir);

			if (status != B_OK)
			{
				switch(status)
				{
					case B_FILE_EXISTS:
						afpError = afpObjectExists;
						break;

					default:
						afpError = afpParmErr;
						break;
				}
			}
			else
			{
				afpVolume->MakeDirty();
				afpError = AFP_OK;
			}
		}

		//
		//If the move succeeded see if we should rename the file.
		//
		if (AFP_SUCCESS(afpError))
		{
			if (strlen(afpNewPathname) > 0)
			{
				status = afpSrcEntry.Rename(afpNewPathname);

				//
				//The client tries to rename the moved dir to its own name, I
				//don't know why. We'll fail here if don't ignore the error.
				//
				if ((status == B_FILE_EXISTS) || (status == B_OK)) {

					afpError = AFP_OK;
				}
				else {

					afpError = afpObjectLocked;
				}
			}
		}
	}

	DBGWRITE(dbg_level_trace, "Returning %lu\n", afpError);

	return( afpError );
}


/*
 * FPRename()
 *
 * Description:
 *		Write data from an open file.
 *
 * Returns: AFPERROR
 */

AFPERROR FPRename(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	char			afpPathname[MAX_AFP_PATH];
	BEntry			afpEntry;
	fp_volume*		afpVolume		= NULL;
	int16			afpVolumeID		= 0;
	int32			afpDirID		= 0;
	int8			afpPathType		= 0;
	int16			afpAttributes	= 0;
	AFPERROR		afpError		= AFP_OK;

	DBGWRITE(dbg_level_warning, "Enter\n");

	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));


	afpVolumeID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathType	= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n",
				afpPathname,
				afpDirID
				);

		return( afpError );
	}

	//
	//Check to make sure we are allowed to write on the volume.
	//
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Doesn't have write access to parent directory!\n");
		return( afpError );
	}

	//
	//Make sure we're allowed to rename this object.
	//
	afpError = fp_objects::GetAFPAttributes(&afpEntry, &afpAttributes);

	if (AFP_SUCCESS(afpError))
	{
		if (afpAttributes & (kFileDeleteInhibit | kFileRenameInhibit))
		{
			DBGWRITE(dbg_level_warning, "Object is locked for renaming! (%s)\n", afpEntry.Name());
			return( afpObjectLocked );
		}
	}

	//
	//Get the pathtype for the new name.
	//
	afpPathType = afpRequest.GetInt8();

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	if (strlen(afpPathname) != 0)
	{
		afpError = (afpEntry.Rename(afpPathname) == B_OK) ? AFP_OK : afpObjectLocked;
	}
	else
	{
		afpError = afpParmErr;
	}

	//
	//We need to signal to all clients that their picture of this
	//volume has changed.
	//
	afpVolume->MakeDirty();

	return( afpError );
}


/*
 * FPMapID()
 *
 * Description:
 *		Map a user ID to a string name
 *
 * Returns: AFPERROR
 */

AFPERROR FPMapID(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)

	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			afpName[MAX_AFP_2_NAME + 1];
	AFP_USER_DATA	userData;
	int8			afpUserType	= 0;
	uint32			afpID		= 0;
	AFPERROR		afpError	= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	memset(afpName, 0, sizeof(afpName));

	//
	//The first byte is padding.
	//
	afpRequest.Advance(sizeof(int8));

	afpUserType	= afpRequest.GetInt8();
	afpID		= afpRequest.GetInt32();

	DBGWRITE(dbg_level_info, "Looking for ID: [%lu] and type: [%d]\n", afpID, afpUserType);

	//
	//First, figure out what name we're supposed to return. User, or group
	//
	switch(afpUserType)
	{
		case 1: //Roman user name
		case 3: //UTF-8 user name
			afpError = afpGetUserDataByID(&userData, afpID);

			if (AFP_SUCCESS(afpError))
			{
				strcpy(afpName, userData.username);
				DBGWRITE(dbg_level_info, "Returning username: %s\n", afpName);
			}
			else
				afpError = afpItemNotFound;
			break;

		case 2: //Roman group name
		case 4: //UTF-8 group name
			//
			//We only support a fixed set of user groups in Haiku currently.
			//
			switch(afpID)
			{
				case 0:
					strcpy(afpName, "Users");
					break;
				case AFP_HAIKU_GROUP_USERS_ID:
					strcpy(afpName, AFP_HAIKU_GROUP_USERS_NAME);
					break;

				case AFP_HAIKU_GROUP_ADMINS_ID:
					strcpy(afpName, AFP_HAIKU_GROUP_ADMINS_NAME);
					break;

				case AFP_HAIKU_GROUP_GUESTS_ID:
					strcpy(afpName, AFP_HAIKU_GROUP_GUESTS_NAME);
					break;

				default:
					afpError = afpItemNotFound;
					break;
			}

			if (AFP_SUCCESS(afpError)) {

				DBGWRITE(dbg_level_info, "Returning group name: %s\n", afpName);
			}
			break;

		default:
			afpError = afpParmErr;
			break;
	}

	//
	//Now, pack the response into the reply buffer based on the type
	//of string the client requested.
	//
	if (AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_trace, "%s\n", afpName);

		switch(afpUserType)
		{
			case 1: //Roman user name
			case 3: //UTF-8 user name
				afpReply.AddCStringAsPascal(afpName);
				break;

			case 2: //Roman group name
			case 4: //UTF-8 group name
				afpReply.AddUniString(afpName, false, true);
				break;

			default:
				afpError = afpParmErr;
				break;
		}
	}

	*afpDataSize = afpReply.GetDataLength();

	return( afpError );
}


/*
 * FPMapName()
 *
 * Description:
 *		Map a user name to the user ID.
 *
 * Returns: AFPERROR
 */

AFPERROR FPMapName(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)

	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			afpName[MAX_AFP_STRING_LEN+1];
	AFP_USER_DATA	userData;
	int8			afpType		= 0;
	AFPERROR		afpError 	= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	afpRequest.Advance(sizeof(int8));
	afpType = afpRequest.GetInt8();

	switch(afpType)
	{
		case 1:	//UTF-8 user name
		case 2:	//UTF-8 group name
			afpError = afpRequest.GetString(afpName, sizeof(afpName), false, kUnicodeNames);
			break;

		case 3:	//Roman user name
		case 4: //Roman group name
			afpError = afpRequest.GetString(afpName, sizeof(afpName), false, kLongNames);
			break;

		default:
			afpError = afpParmErr;
			break;
	}

	if (AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_info, "Mapping ID for name: %s\n", afpName);

		switch(afpType)
		{
			case 1:
			case 3:
				if (afpGetUserDataByName(afpName, &userData) == B_OK)
				{
					DBGWRITE(dbg_level_trace, "Returning user ID: %lu\n", userData.id);
					afpReply.AddInt32(userData.id);
				}
				else
					afpError = afpItemNotFound;
				break;

			case 2:
			case 4:
			{
				uint32	groupID = 0;

				if (strcmp(afpName, AFP_HAIKU_GROUP_USERS_NAME) == 0)
					groupID = AFP_HAIKU_GROUP_USERS_ID;
				else if (strcmp(afpName, AFP_HAIKU_GROUP_ADMINS_NAME) == 0)
					groupID = AFP_HAIKU_GROUP_ADMINS_ID;
				else if (strcmp(afpName, AFP_HAIKU_GROUP_GUESTS_NAME) == 0)
					groupID = AFP_HAIKU_GROUP_GUESTS_ID;
				else
					afpError = afpItemNotFound;

				if (AFP_SUCCESS(afpError))
				{
					DBGWRITE(dbg_level_trace, "Returning group ID: %lu\n", groupID);
					afpReply.AddInt32(groupID);
				}
				break;
			}
		}
	}

	*afpDataSize = afpReply.GetDataLength();

	return( afpError );
}


/*
 * FPGetUserInfo()
 *
 * Description:
 *		Map a user name to a user and group ID.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetUserInfo(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)

	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	AFP_USER_DATA	userData;
	int32			afpUserID	= 0;
	int16			afpBitmap	= 0;
	int8			afpThisUser	= 0;
	AFPERROR		afpError	= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	afpRequest.Advance(sizeof(int8));
	afpThisUser = afpRequest.GetInt8();

	if (afpThisUser == 0)
	{
		DBGWRITE(dbg_level_warning, "This user bit not set!!\n");
		return( afpParmErr );
	}

	afpUserID	= afpRequest.GetInt32();
	afpBitmap	= afpRequest.GetInt16();

	afpReply.AddInt16(afpBitmap);

	if (afpBitmap & kUserID)
	{
		afpError = afpSession->GetUserInfo(&userData);

		if (AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_trace, "Found user id: %lu (%s)\n", userData.id, userData.username);
			afpReply.AddInt32(userData.id);
		}
		else
			afpError = afpItemNotFound;
	}

	if (afpBitmap & kGroupID)
	{
		uint32 groupID = 0;

		if (afpSession->IsAdmin())
			groupID = AFP_HAIKU_GROUP_ADMINS_ID;
		else if (afpSession->IsUser())
			groupID = AFP_HAIKU_GROUP_USERS_ID;
		else if (afpSession->IsGuest())
			groupID = AFP_HAIKU_GROUP_GUESTS_ID;
		else
			afpError = afpItemNotFound;

		DBGWRITE(dbg_level_trace, "Found group id: %lu\n", groupID);
		afpReply.AddInt32(groupID);
	}

	*afpDataSize = afpReply.GetDataLength();

	return( afpError );
}


/*
 * FPByteRangeLock()
 *
 * Description:
 *		Lock a range of bytes within an open file.
 *
 * Returns: AFPERROR
 */

AFPERROR FPByteRangeLock(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	uint8			afpCommand		= 0;
	int8			afpBRLFlags		= 0;
	int16			afpForkRef		= 0;
	off_t			afpOffset		= 0;
	off_t			afpLength		= 0;
	off_t			afpFileSize		= 0;
	AFPERROR		afpError		= AFP_OK;
	OPEN_FORK_ITEM*	forkItem		= NULL;

	//
	//The first byte contains the afp command.
	//
	afpCommand 	= afpRequest.GetInt8();
	afpBRLFlags	= afpRequest.GetInt8();
	afpForkRef	= afpRequest.GetInt16();

	switch (afpCommand)
	{
		case afpByteRangeLock:
			afpOffset	= afpRequest.GetInt32();
			afpLength	= afpRequest.GetInt32();
			break;

		case afpByteRangeLockExt:
			afpOffset	= afpRequest.GetInt64();
			afpLength	= afpRequest.GetInt64();
			break;

		default:
			return( afpParmErr );
	}

	//
	//Get the fork structure for the opened fork.
	//
	forkItem = afpSession->GetForkItem(afpForkRef);
	afpError = (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_SUCCESS(afpError))
	{
		if (forkItem->forkopen == kDataFork) {

			forkItem->entry->GetSize(&afpFileSize);
		}
		else
		{
			attr_info	info = {0,0};
			BNode		node(forkItem->entry);

			if (node.GetAttrInfo(AFP_RSRC_ATTRIBUTE, &info) == B_OK) {
				afpFileSize = info.size;
			}

			DBGWRITE(dbg_level_info, "Locking resource fork...\n");
		}

		DBGWRITE(dbg_level_info, "Offset = %lld\n", afpOffset);
		DBGWRITE(dbg_level_info, "Length = %lld\n", afpLength);

		if (afpBRLFlags & kUnlockFlag)
		{
			fp_rangelock*	afpLock = NULL;

			afpLock  = fp_rangelock::SessionRangeLocked(afpOffset, afpLength, forkItem);
			afpError = (afpLock == NULL) ? afpRangeNotLocked : AFP_OK;

			if (AFP_SUCCESS(afpError))
			{
				DBGWRITE(dbg_level_trace, "Unlocking...\n");
				delete afpLock;
			}
			else
			{
				DBGWRITE(dbg_level_warning, "Failure unlocking range! (%lu)\n", afpError);
			}
		}
		else //Locking
		{
			//
			//If the user wants us to set the offset from the end of the
			//file, then we have a lot more work to do.
			//
			if (afpBRLFlags & kStartEndFlag)
			{
				DBGWRITE(dbg_level_trace, "Going from end of file!\n");
				afpOffset = (afpOffset < 0) ? (afpFileSize + afpOffset) : (afpFileSize - afpOffset);
			}
			else //From the beginning
			{
				if (afpOffset < 0)
				{
					//
					//The offset can only be negative if we are working from the
					//end of a file.
					//
					DBGWRITE(dbg_level_warning, "Negative offset from start!\n");
					return( afpParmErr );
				}

				//
				//If the length is 0xFFFFFFFF, then we have special handling to do.
				//
				if (((afpCommand == afpByteRangeLock) && (afpLength == 0xFFFFFFFF))	||
					((afpCommand == afpByteRangeLockExt) && ((uint64)afpLength == ULONGLONG_MAX)))
				{
					switch(afpOffset)
					{
						case 0:
							//
							//0 offset means we lock the entire range of the file.
							//
							afpLength = afpFileSize;
							break;
						default:
							//
							//The user wants to lock from offset to the end of the file.
							//
							afpLength = (afpFileSize - afpOffset);
							break;
					}
				}
			}//from beginning

			fp_rangelock*	afpLock = new fp_rangelock(forkItem);

			if (afpLock != NULL)
			{
				afpError = afpLock->Lock(afpOffset, afpLength);

				if (AFP_SUCCESS(afpError))
				{
					DBGWRITE(dbg_level_trace, "Locked...\n");
				}
				else
				{
					//
					//We failed to lock, free the object and return the error.
					//
					delete afpLock;

					DBGWRITE(dbg_level_warning, "Lock error when locking! (%lu)\n", afpError);
				}
			}
		}

		//
		//We return the first byte of the newly locked range
		//
		switch (afpCommand)
		{
			case afpByteRangeLock:
				afpReply.AddInt32(afpOffset);
				break;

			case afpByteRangeLockExt:
				afpReply.AddInt64(afpOffset);
				break;

			default:
				return( afpParmErr );
		}

		*afpDataSize = afpReply.GetDataLength();
	}

	return( afpError );
}


/*
 * FPCopyFile()
 *
 * Description:
 *		Lock a range of bytes within an open file.
 *
 * Returns: AFPERROR
 */

AFPERROR FPCopyFile(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)

	afp_buffer		afpRequest(afpReqBuffer);
	char			afpSrcPathname[MAX_AFP_PATH];
	char			afpDstPathname[MAX_AFP_PATH];
	char			afpNewName[MAX_AFP_NAME];
	BEntry			afpSrcEntry;
	BEntry			afpDstEntry;
	fp_volume*		afpSrcVolume	= NULL;
	fp_volume*		afpDstVolume	= NULL;
	int16			afpSrcVolID		= 0;
	int32			afpSrcDirID		= 0;
	int16			afpDstVolID		= 0;
	int32			afpDstDirID		= 0;
	int8			afpPathType		= 0;
	AFPERROR		afpError		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	//
	//Skip the first byte which is afp command.
	//
	afpRequest.Advance(sizeof(int16));

	afpSrcVolID		= afpRequest.GetInt16();
	afpSrcDirID		= afpRequest.GetInt32();
	afpDstVolID		= afpRequest.GetInt16();
	afpDstDirID		= afpRequest.GetInt32();

	//
	//Get the pathname of the object we're copying.
	//
	afpPathType	= afpRequest.GetInt8();
	afpError 	= afpRequest.GetString(
								afpSrcPathname,
								sizeof(afpSrcPathname),
								true,
								afpPathType
								);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Get the destination pathname we're copying to
	//
	afpPathType		= afpRequest.GetInt8();
	afpError 		= afpRequest.GetString(
								afpDstPathname,
								sizeof(afpDstPathname),
								true,
								afpPathType
								);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//And now the [optional] new name for the file
	//
	afpPathType		= afpRequest.GetInt8();
	afpError 		= afpRequest.GetString(
								afpNewName,
								sizeof(afpNewName),
								true,
								afpPathType
								);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Make sure the client has opened the volumes that the file copy
	//is happening on.
	//

	if ((afpSrcVolume = FindVolume(afpSrcVolID)) == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Src volume not found! (%d)\n", afpSrcVolID);
		return( afpParmErr );
	}

	if ((afpDstVolume = FindVolume(afpDstVolID)) == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Dst volume not found! (%d)\n", afpDstVolID);
		return( afpParmErr );
	}

	if ((!afpSession->HasVolumeOpen(afpSrcVolume)) || (!afpSession->HasVolumeOpen(afpDstVolume)))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume(s) open!\n");
		return( afpParmErr );
	}

	//
	//Set the entry object that will point to the object we're copying.
	//
	afpError = fp_objects::SetAFPEntry(
								afpSrcVolume,
								afpSrcDirID,
								afpSrcPathname,
								afpSrcEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Src Object not found! (name = %s, dirID = %lu)\n",
				afpSrcPathname,
				afpSrcDirID
				);

		return( afpError );
	}

	//
	//As per the spec, we only do copy file on files, NOT directories.
	//
	if (!afpSrcEntry.IsFile()) {

		return( afpObjectTypeErr );
	}

	//
	//Get the entry object for the destination directory.
	//
	afpError = fp_objects::SetAFPEntry(
								afpDstVolume,
								afpDstDirID,
								afpDstPathname,
								afpDstEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Dst Object not found! (name = %s, dirID = %lu)\n",
				afpDstPathname,
				afpDstDirID
				);

		return( afpError );
	}

	//
	//Now construct the new full pathname to what will be the newly created object.
	//
	BFile	destFile;
	BFile	srcFile;

	if (strlen(afpNewName) == 0)
	{
		if (afpSrcEntry.GetName(afpNewName) != B_OK)
		{
			DBGWRITE(dbg_level_warning, "Failed to get name of src file!\n");
			return( afpParmErr );
		}
	}

	//
	//Check to make sure this session has write access to the destination
	//directory location.
	//
	afpError = afpCheckWriteAccess(afpSession, afpDstVolume, &afpDstEntry);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Now that we have the new pathname all set, we need to actually create
	//a new file in the destination.
	//
	BDirectory	destDir(&afpDstEntry);

	if (destDir.CreateFile(afpNewName, &destFile, true) != B_OK)
	{
		DBGWRITE(dbg_level_error, "CreateFile() failed!\n");
		return( afpParmErr );
	}

	srcFile.SetTo(&afpSrcEntry, B_READ_WRITE);

	if (srcFile.InitCheck() != B_OK)
	{
		DBGWRITE(dbg_level_error, "InitCheck() failed on src BFile!\n");
		return( afpParmErr );
	}

	//
	//Now finally, perform the actual copy operation.
	//
	if (fp_objects::CopyFile(srcFile, destFile) != B_OK)
	{
		DBGWRITE(dbg_level_error, "CopyFile() failed!\n");
		return( afpParmErr );
	}

	//
	//We need to signal to all clients that their picture of this
	//volume has changed.
	//
	afpDstVolume->MakeDirty();

	return( AFP_OK );
}


/*
 * FPCreateID()
 *
 * Description:
 *		Creates a unique file ID. In our case, the BeOS always keeps unique
 *		file ID's, so we just pass the current node id as the file ID.
 *
 * Returns: AFPERROR
 */

AFPERROR FPCreateID(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpRequest(afpReqBuffer);
	afp_buffer	afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	char		afpPathname[MAX_AFP_PATH];
	BEntry		afpEntry;
	node_ref	nref;
	fp_volume*	afpVolume		= NULL;
	int16		afpVolumeID		= 0;
	int32		afpDirID		= 0;
	int8		afpPathType		= 0;
	AFPERROR	afpError		= AFP_OK;

	DBGWRITE(dbg_level_warning, "Enter...\n");

	//
	//First word is command the padding byte.
	//
	afpRequest.Advance(sizeof(int16));

	afpVolumeID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathType	= afpRequest.GetInt8();

	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	//
	//The client must have the volume open for access using
	//FPOpenVol before making this call.
	//
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		//
		//Nope, client made a boo boo, return parm error.
		//
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	//
	//Pathname cannot be null since it contains the file to open.
	//
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_warning, "Null pathname!\n");
		return( afpParmErr );
	}

	//
	//Set the entry object that will point to this afp object.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	//
	//OK, the file exists, now get the node_ref for the file and
	//return the node number as the file ID.
	//
	if (afpEntry.GetNodeRef(&nref) == B_OK)
	{
		afpReply.AddInt32(nref.node);

		*afpDataSize 	= afpReply.GetDataLength();
		afpError		= AFP_OK;
	}

	return( afpError );
}


/*
 * FPZzzz()
 *
 * Description:
 *		The client is going to sleep.
 *
 * Returns: AFPERROR
 */

AFPERROR FPZzzz(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	AFPERROR		afpError 	= AFP_OK;
	int32			afpFlag 	= 0;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	//
	//Advance beyond the afp cmd byte and the padding
	//
	afpRequest.Advance(sizeof(int16));

	//
	//The flag parameter is 1 if client is going to sleep, 2 if
	//the client is waking from sleep.
	//
	afpFlag = afpRequest.GetInt32();

	switch(afpFlag)
	{
		case 1:
			DBGWRITE(dbg_level_trace, "Setting client to sleep\n");

			afpSession->SetClientIsSleeping(true);
			break;

		case 2:
			DBGWRITE(dbg_level_trace, "Setting client to awake\n");

			afpSession->SetClientIsSleeping(false);
			break;

		default:
			afpError = afpParmErr;
			break;
	}

	DBGWRITE(dbg_level_trace, "Returning: %lu\n", afpError);

	return( afpError );
}























