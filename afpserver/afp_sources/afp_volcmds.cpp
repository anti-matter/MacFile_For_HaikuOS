#include <memory>
#include <path.h>

#include "debug.h"

#include "afp.h"
#include "afp_session.h"
#include "afp_buffer.h"
#include "afpuser.h"
#include "afphostname.h"
#include "fp_objects.h"
#include "fp_volume.h"
#include "dsi_scavenger.h"

extern std::unique_ptr<BList> volume_blist;
extern dsi_scavenger*	gAFPSessionMgr;

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

	// Set the flags which tell what our server supports.
	afpFlags = kSupportsTCPIP
				| kSupportsSrvrNotification
				| kSupportsCopyFile
				| kSupportsSrvrMsgs
				| kSupportsReconnect
				| kSupportsChngPswd
				| kSupportsExtSleep;

	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_FLAGS]) = htons(afpFlags);

	// We don't have a custom volume icon.
	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_VOLUMEICON]) = 0;

	// Blast in the computer name.
	afp_GetHostname(hostname, sizeof(hostname));

	pBuffer = &afpReplyBuffer[SRVRINFO_OFFSET_SRVRNAME];
	PUSH_CSTRING(hostname, pBuffer);

	// Add an extra padding byte if the AFP buffer is not on
	// an even boundary.
	if ((pBuffer - afpReplyBuffer) % 2)
		*pBuffer++ = 0x00;

	// Save the server signature offset
	pServerSigOffset = pBuffer;
	pBuffer += sizeof(int16);

	// Network address count offset, save it for later.
	pAddressCountOffset = pBuffer;
	pBuffer += sizeof(int16);

	// Set the machine type and offset.
	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_MACHTYPE]) =
											htons(pBuffer - afpReplyBuffer);

	PUSH_CSTRING(AFP_MACHINE_TYPE, pBuffer);

	// Paste in the AFP version we support.
	*((int16*)&afpReplyBuffer[SRVRINFO_OFFSET_AFPVERSCOUNT]) =
											htons(pBuffer - afpReplyBuffer);

	*pBuffer++ = AFP_VERSION_COUNT;

	PUSH_CSTRING(AFP_22_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_30_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_31_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_32_VERSION_STR, pBuffer);
	PUSH_CSTRING(AFP_33_VERSION_STR, pBuffer);

	// Now the supported UAM's get included.
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

	// This is always supported in our server, no matter what.
	PUSH_CSTRING(UAM_CLEAR_TEXT, pBuffer);
	PUSH_CSTRING(UAM_DHCAST128, pBuffer);

	// Set the server sig offset and value.
	*((int16*)pServerSigOffset) = htons(pBuffer - afpReplyBuffer);
	*pBuffer++ = 0;

	// Network Address: Note, there is no reason for us to pass
	// back address since we only support connections over TCP/IP.
	// This feature is only for connection where the initial connection
	// is done over AppleTalk.

	*((int16*)pAddressCountOffset) = htons(pBuffer - afpReplyBuffer);
	*pBuffer++ = 0;

	// This size does NOT include the size of the DSI Header.
	*afpDataSize = (pBuffer - afpReplyBuffer);

	DBGWRITE(dbg_level_trace, "FPGetSrvrInfo reply datasize (%d bytes): ", *afpDataSize);

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

	// First byte is afp command and padding.
	afpRequest.Advance(sizeof(int16));
	afpType	= afpRequest.GetInt16();

	if (afpType != kLoginWithoutID)
	{
		afpIDSize = afpRequest.GetInt32();

		DBGWRITE(dbg_level_trace, "Client ID size = %lu\n", afpIDSize);

		// For types 3 & 4, a time stamp is included in the request
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

		if (afpID == NULL)
		{
			DBGWRITE(dbg_level_error, "Failed to allocate client ID buffer!\n");
			return( afpMiscErr );
		}

		afpRequest.GetRawData(afpID, afpIDSize);
	}

	switch(afpType)
	{
		case kLoginWithoutID:
			DBGWRITE(dbg_level_trace, "kLoginWithoutID\n");
			break;

		// DEPRECATED: This constant is deprecated per AFP3.2 spec, but we keep
		// it around for older clients.
		case kLoginWithID:
			DBGWRITE(dbg_level_trace, "kLoginWithID\n");

			afpKillSession = gAFPSessionMgr->FindSessionByID(afpIDSize, afpID);

			if (afpKillSession != NULL)
			{
				DBGWRITE(dbg_level_trace, "Killing old session!\n");
				afpKillSession->KillSession();
			}

			afpSession->SetClientID(afpIDSize, afpID);
			fSavedID = true;
			break;

		// The client wants his old session to be discarded. The client is sending
		// an IDLength, an ID, and a Timestamp. We only discard the old session if
		// the timestamps DO NOT match.
		case kLoginWithTimeAndID:
			DBGWRITE(dbg_level_trace, "kLoginWithTimeAndID\n");

			afpKillSession = gAFPSessionMgr->FindSessionByID(afpIDSize, afpID);

			if (afpKillSession != NULL)
			{
				DBGWRITE(dbg_level_trace, "Found old session!!\n");

				if (afpKillSession->GetAFPTimeStamp() != afpTimeStamp)
				{
					// Time stamps don't match, discard the old session.
					afpSession->KillSession();
					break;
				}
			}

			afpSession->SetAFPTimeStamp(afpTimeStamp);
			afpSession->SetClientID(afpIDSize, afpID);

			fSavedID = true;
			break;

		// DEPRECATED: Deprecated as per AFP3.2 spec
		case kReconnWithID:
			DBGWRITE(dbg_level_trace, "kReconnWithID\n");
			break;

		// The client has just reconnected to a previously disconnected
		// session. Update the session with the new ID.
		case kReconnWithTimeAndID:
			DBGWRITE(dbg_level_trace, "kReconnWithTimeAndID\n");

			afpSession->SetAFPTimeStamp(afpTimeStamp);
			afpSession->SetClientID(afpIDSize, afpID);

			fSavedID = true;
			break;

		// The following IDs are unsupported by this afp server since we
		// don't support the reconnect UAM.
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
		// Our token length is fixed, it's not right, we're in
		// deep doo-doo.
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

	// First, stuff in the system time into the reply buffer.
	afpReply.AddInt32(TO_AFP_TIME(real_time_clock()));

	// Stuff in the number of volumes we have shared out.
	numVolumes = volume_blist->CountItems();
	afpReply.AddInt8(numVolumes);

	// Now stuff in all the volume names as pascal strings.
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

	DBGWRITE(dbg_level_info, "FPGetSrvrParms reply hex (%d bytes): ", *afpDataSize);
	for (int32 _k = 0; _k < *afpDataSize; _k++)
		DBGWRITE(dbg_level_info, "%02X ", afpReply.GetBuffer()[_k]);
	DBGWRITE(dbg_level_info, "\n");

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

	// Get the volume bitmap from the request buffer.
	volBitmap = afpRequest.GetInt16();

	// Extract the volume name the client wants to open.
	afpRequest.GetString(szVolumeName, sizeof(szVolumeName));

	// A null byte may be added to make the next parm start
	// on an even boundary.
	afpRequest.AdvanceIfOdd();

	// Extract the volume password, its okay if there isn't one there.
	afpRequest.GetRawData(szPassword, sizeof(szPassword));

	DBGWRITE(dbg_level_info, "FPOpenVol volBitmap=0x%04X name=%s\n", volBitmap, szVolumeName);

	// Now search for the volume object associated with the volume name.
	afpVolume = FindVolume(szVolumeName);

	// If we found the volume, then get the parameters.
	if (afpVolume != NULL)
	{
		DBGWRITE(dbg_level_trace, "Opening volume %s\n", szVolumeName);

		afpError = afpVolume->fp_GetVolParms(volBitmap, afpSession->GetAFPVersion(), afpReply);

		if (AFP_SUCCESS(afpError))
		{
			// Now call the volume routine to actually flag the vol as open.
			// This call could fail if the user currently has too many volumes
			// already open.
			afpError = afpVolume->fp_OpenVolume(afpSession);

			if (AFP_SUCCESS(afpError))
			{
				// Get the size of the volume parameter data.
				*afpDataSize = afpReply.GetDataLength();
				DBGWRITE(dbg_level_trace, "FPOpenVol reply hex (%d bytes): \n", *afpDataSize);
				DBG_DUMP_BUFFER((const char*)afpReply.GetBuffer(), *afpDataSize, dbg_level_trace);
			}
		}
	}
	else
	{
		// If we get here, we couldn't find the requested volume.
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

	// The first word contains the afp command and padding byte.
	afpRequest.Advance(sizeof(int16));

	// Get the volume ID requested to be closed.
	afpVolumeID = afpRequest.GetInt16();

	// Get the volume object in charge of the volume
	afpVolume = FindVolume(afpVolumeID);

	// Now, if we found the volume and the user has it open, perform
	// the close operation.
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

	// The first word contains the afp command and padding byte.
	afpRequest.Advance(sizeof(int16));

	// Get the volume ID the client wants to get parms for.
	afpVolumeID = afpRequest.GetInt16();

	// Now search for the volume object associated with the volume name.
	afpVolume = FindVolume(afpVolumeID);

	// If we found the volume using the volID, then afpSession will
	// not be null. The client must also have previously opened the
	// volume using FPOpenVol.
	if ((afpVolume != NULL) && (afpSession->HasVolumeOpen(afpVolume)))
	{
		// Get the bitmap the describes what parms we are going to supply.
		afpVolBitmap = afpRequest.GetInt16();

		// Now call on the volume object to get the information requested.
		afpError = afpVolume->fp_GetVolParms(afpVolBitmap, afpSession->GetAFPVersion(), afpReply);

		// Lastly, if we succeed, get the afp data length.
		if (AFP_SUCCESS(afpError))
		{
			*afpDataSize = afpReply.GetDataLength();
		}
	}
	else
	{
		// Either the volume ID is bad or the user did not have the
		// volume opened for access.
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

	// Get a pointer to the volume object we'll be working with
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

	// Don't support the following bitmaps.
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

	// Return the bitmap that tells what information the rest
	// of the buffer contains.
	afpReply.push_num(afpBitmap);

	// If the bitmap is empty, then we return nothing but the bitmap.
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

	// The first byte is padding.
	afpRequest.Advance(sizeof(int8));

	afpUserType	= afpRequest.GetInt8();
	afpID		= afpRequest.GetInt32();

	DBGWRITE(dbg_level_info, "Looking for ID: [%lu] and type: [%d]\n", afpID, afpUserType);

	// First, figure out what name we're supposed to return. User, or group
	switch(afpUserType)
	{
		case 1: // Roman user name
		case 3: // UTF-8 user name
			afpError = afpGetUserDataByID(&userData, afpID);

			if (AFP_SUCCESS(afpError))
			{
				strcpy(afpName, userData.username);
				DBGWRITE(dbg_level_info, "Returning username: %s\n", afpName);
			}
			else
				afpError = afpItemNotFound;
			break;

		case 2: // Roman group name
		case 4: // UTF-8 group name
			// 
			// We only support a fixed set of user groups in Haiku currently.
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

	// Now, pack the response into the reply buffer based on the type
	// of string the client requested.
	if (AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_trace, "%s\n", afpName);

		switch(afpUserType)
		{
			case 1: // Roman user name
			case 3: // UTF-8 user name
				afpReply.AddCStringAsPascal(afpName);
				break;

			case 2: // Roman group name
			case 4: // UTF-8 group name
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
		case 1:	// UTF-8 user name
		case 2:	// UTF-8 group name
			afpError = afpRequest.GetString(afpName, sizeof(afpName), false, kUnicodeNames);
			break;

		case 3:	// Roman user name
		case 4: // Roman group name
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

	DBGWRITE(dbg_level_trace, "Enter\n");

	// First word is command the padding byte.
	afpRequest.Advance(sizeof(int16));

	afpVolumeID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathType	= afpRequest.GetInt8();

	// Get a pointer to the volume object we'll be working with
	afpVolume = FindVolume(afpVolumeID);

	if (afpVolume == NULL)
	{
		// The volume ID is not valid, bail...
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolumeID);
		return( afpParmErr );
	}

	// The client must have the volume open for access using
	// FPOpenVol before making this call.
	if (!afpSession->HasVolumeOpen(afpVolume))
	{
		// Nope, client made a boo boo, return parm error.
		DBGWRITE(dbg_level_warning, "User doesn't have volume open!\n");
		return( afpParmErr );
	}

	// Get the pathname of the object we're working on.
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	// Pathname cannot be null since it contains the file to open.
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_warning, "Null pathname!\n");
		return( afpParmErr );
	}

	// Set the entry object that will point to this afp object.
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

	// OK, the file exists, now get the node_ref for the file and
	// return the node number as the file ID.
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

	// Advance beyond the afp cmd byte and the padding
	afpRequest.Advance(sizeof(int16));

	// The flag parameter is 1 if client is going to sleep, 2 if
	// the client is waking from sleep.
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























