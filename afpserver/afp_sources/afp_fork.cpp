#include <fs_attr.h>

#include "debug.h"

#include "afp.h"
#include "afp_session.h"
#include "afp_buffer.h"
#include "afpaccess.h"
#include "fp_rangelock.h"
#include "fp_objects.h"
#include "fp_volume.h"
#include "afpvolume.h"
#include "dsi_connection.h"

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

	// The first word contains the afp command and padding byte.
	afpRequest.Advance(sizeof(int16));

	afpVolumeID = afpRequest.GetInt16();

	// For the BeOS, we don't have anything we need to do here. So,
	// we don't do anything other than verify that the volume is
	// indeed open and parameters are correct.

	// 
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

	// The first word contains the afp command and padding byte.
	afpRequest.Advance(sizeof(int16));

	afpForkRef	= afpRequest.GetInt16();
	forkItem 	= afpSession->GetForkItem(afpForkRef);
	afpError 	= (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_SUCCESS(afpError))
	{
		// Check if any other session holds a lock on this fork. A flush
		// affects the entire fork, so we reject it if another session
		// has locked any portion of it.
		if (forkItem->forkopen == kDataFork || forkItem->isResFile)
		{
			off_t	afpFileSize = 0;

			forkItem->entry->GetSize(&afpFileSize);

			if (fp_rangelock::RangeLocked(
								0,
								afpFileSize,
								afpSession,
								forkItem->entry
								))
			{
				DBGWRITE(dbg_level_warning, "****Cannot flush: range is locked by another session!****\n");
				afpError = afpLockErr;
			}
		}

		if (AFP_SUCCESS(afpError))
		{
			forkItem->mutex->Lock();

			if (forkItem->forkopen == kDataFork || forkItem->isResFile)
			{
				// Sync the file using the filesystem API after making sure
				// the file is valid and exists.
				if ((forkItem->file != NULL) 				&&
					(forkItem->file->InitCheck() == B_OK)	&&
					(forkItem->file->IsWritable())			)
				{
					forkItem->file->Sync();
				}
				else
				{
					DBGWRITE(dbg_level_warning, "File is not valid or writable, sync skipped\n");
					afpError = afpMiscErr;
				}
			}
			else
			{
				// We call the resource fork closing method, but we tell it not
				// to actually delete the memory block which causes just the
				// file on disk to be updated.
				afpSession->CloseAndWriteOutResourceFork(forkItem, false);
			}

			forkItem->mutex->Unlock();
		}
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

	// The first word contains the afp command and padding byte.
	afpRequest.Advance(sizeof(int16));

	// Get the file ref num we're closing down.
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

	// The first byte contains the afp command.
	afpRequest.Advance(sizeof(int8));

	// Get the parameters for the file we're opening.
	afpFork		= (afpRequest.GetInt8() & kResourceForkBit) ? kRsrcFork : kDataFork;
	afpVolumeID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpBitmap	= afpRequest.GetInt16();
	afpMode		= afpRequest.GetInt16();
	afpPathType	= afpRequest.GetInt8();

	// The open mode must request read and/or write access. A mode with
	// neither bit set is invalid; without this check the file would be
	// opened read-only (O_RDONLY) below with no access check performed.
	if ((afpMode & (kReadMode | kWriteMode)) == 0)
	{
		DBGWRITE(dbg_level_warning, "Invalid open mode (no read/write bits)! (0x%04x)\n", (unsigned)afpMode);
		return( afpParmErr );
	}

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
		DBGWRITE(dbg_level_trace, "Null pathname!\n");
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
		DBGWRITE(dbg_level_warning, "Object not found = %s\n", afpPathname);
		return( afpError );
	}

	// Check to make sure we are allowed to write on the volume.
	if (afpMode & kWriteMode)
	{
		afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

		if (!AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_warning, "User doesn't have write access to the file! (%s)\n", afpPathname);

			// If the caller is requesting write access, we deny the opening since
			// the file cannot be written to.
			return( afpError );
		}
	}

	// Check for read access to the file.
	if (afpMode & kReadMode)
	{
		afpError = afpCheckReadAccess(afpSession, &afpEntry);

		if (!AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_warning, "User doesn't have read access to the file!\n");

			// If the caller is requesting read access, we deny the opening since
			// the file cannot be written to.
			return( afpError );
		}
	}

	// Use the session object to actually open the file. The session is
	// the object that keeps track of open files.
	afpError = afpSession->OpenFile(afpVolume, &afpEntry, afpMode, afpFork, &afpNewRefNum);

	if (AFP_SUCCESS(afpError))
	{
		// We don't support the following bitmaps, so we clear them.
		if (afpSession->GetAFPVersion() < afpVersion30)
		{
			if (afpBitmap & kFPProDos)		afpBitmap  &= ~kFPProDos;
			if (afpBitmap & kFPShortName)	afpBitmap  &= ~kFPShortName;
		}

		// Add in the bitmap and new refnum id.
		afpReply.AddInt16(afpBitmap);
		afpReply.AddInt16(afpNewRefNum);

		// Now that we've opened the file, get the requested parameters.
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

	// The first 2 bytes contain the afp command and padding.
	afpRequest.Advance(sizeof(int16));

	afpForkRef 	= afpRequest.GetInt16();
	afpBitmap	= afpRequest.GetInt16();
	afpForkLen	= afpRequest.GetInt32();

	forkItem 	= afpSession->GetForkItem(afpForkRef);
	afpError 	= (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_SUCCESS(afpError))
	{
		// Check to make sure this session has write access to this file
		// or directory.
		afpError = afpCheckWriteAccess(afpSession, NULL, forkItem->entry);

		if (!AFP_SUCCESS(afpError))
		{
			return( afpError );
		}

		// For .res files, the resource fork data lives in the data fork.
		// Swap the bitmap flags so the code below operates on the data fork
		// when the client asks to resize the "resource fork".
		int16 adjustedBitmap = afpBitmap;
		if (forkItem->isResFile)
		{
			if (adjustedBitmap & kFPRFLen)
			{
				adjustedBitmap |= kFPDFLen;
				adjustedBitmap &= ~kFPRFLen;
			}
			else if (adjustedBitmap & kFPDFLen)
			{
				adjustedBitmap |= kFPRFLen;
				adjustedBitmap &= ~kFPDFLen;
			}

			if (adjustedBitmap & kFPExtRsrcForkLen)
			{
				adjustedBitmap |= kFPExtDataForkLen;
				adjustedBitmap &= ~kFPExtRsrcForkLen;
			}
			else if (adjustedBitmap & kFPExtDataForkLen)
			{
				adjustedBitmap |= kFPExtRsrcForkLen;
				adjustedBitmap &= ~kFPExtDataForkLen;
			}
		}

		if ((adjustedBitmap & kFPDFLen) || (adjustedBitmap & kFPExtDataForkLen))
		{
			DBGWRITE(dbg_level_trace, "Setting data fork length: %lu\n", afpForkLen);

			if (forkItem->forkopen != kDataFork && !forkItem->isResFile)
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

		if ((adjustedBitmap & kFPRFLen) || (adjustedBitmap & kFPExtRsrcForkLen))
		{
			// For .res files, the data fork SetSize above already handled it.
			if (!forkItem->isResFile)
			{
				BNode	node(forkItem->entry);

				DBGWRITE(dbg_level_trace, "Setting rsrc fork length: %lld\n", afpForkLen);

				if (forkItem->forkopen != kRsrcFork)
				{
					DBGWRITE(dbg_level_warning, "Resource fork is not open!\n");
					return( afpBitmapErr );
				}

				forkItem->rsrcIO->SetSize(afpForkLen);

				// If we set the size of the fork, we need to write out the entire
				// fork to disk so that subsequent GetFileParms calls will return
				// the proper RF length.
				node.RemoveAttr(AFP_RSRC_ATTRIBUTE);

				// 06.02.09: Fixed bug where older mac clients appear to attempt to write
				// zero bytes to the file if there is no resource fork. This, of course,
				// returns an error that the older clients can't deal with, so they fail.
				if (afpForkLen > 0)
				{
					// Now write the data back to the resource stream of the file.
					afpError = (node.WriteAttr(
									AFP_RSRC_ATTRIBUTE,
									B_RAW_TYPE,
									0,
									forkItem->rsrcIO->Buffer(),
									forkItem->rsrcIO->BufferLength() ) < B_OK) ? afpParmErr : AFP_OK;
				}
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

	// The first byte contains the afp command.
	afpRequest.Advance(sizeof(int16));

	// Get the parameters for the file we're opening.
	afpForkRef	= afpRequest.GetInt16();
	afpBitmap	= afpRequest.GetInt16();

	DBGWRITE(dbg_level_trace, "Enter, getting parms for: %lu\n", afpForkRef);

	forkItem = afpSession->GetForkItem(afpForkRef);
	afpError = (forkItem == NULL) ? afpParmErr : AFP_OK;

	if (AFP_SUCCESS(afpError))
	{
		// We don't support the following bitmaps, so we clear them.
		if (afpSession->GetAFPVersion() < afpVersion30)
		{
			if (afpBitmap & kFPProDos)		afpBitmap  &= ~kFPProDos;
			if (afpBitmap & kFPShortName)	afpBitmap  &= ~kFPShortName;
		}

		// Make sure the caller is asking for the length of the fork
		// that is actually opened. (Skip for .res files — bitmap is passed through.)
		if (!forkItem->isResFile &&
			(((afpBitmap & kFPDFLen) && (forkItem->forkopen == kRsrcFork))	||
			((afpBitmap & kFPRFLen) && (forkItem->forkopen == kDataFork))	))
		{
			DBGWRITE(dbg_level_warning, "Attempt to get length of wrong fork!\n");
			return( afpBitmapErr );
		}

		// Add in the bitmap
		afpReply.AddInt16(afpBitmap);

		// Now that we've opened the file, get the requested parameters.
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

	// First byte is afp command.
	afpRequest.Advance(sizeof(int8));

	createFlag	= afpRequest.GetInt8();
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
		DBGWRITE(dbg_level_warning, "Failed to get pathname from afp blob!\n");
		return( afpError );
	}

	// Pathname cannot be null since it contains the new file name.
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_warning, "Null pathname!\n");
		return( afpParmErr );
	}

	// Set the entry object that will point to this afp object.
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

	// Check to make sure we are allowed to write on the volume.
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have write access!\n");
		return( afpError );
	}

	// Now we use the B API's to create the new directory in the
	// filesystem.
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
		// We failed to get an object for the directory.
		DBGWRITE(dbg_level_warning, "dir.InitCheck() failed! (%s)\n", GET_BERR_STR(dir.InitCheck()));
		afpError = afpParmErr;
	}

	if (AFP_SUCCESS(afpError))
	{
		BEntry	newEntry(&dir, afpPathname);

		if (newEntry.InitCheck() == B_OK)
		{
			// If the new name is longer than kLongNames can handle, create
			// the longname and store it.
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
			// Something strange happened and we didn't actually create the file.
			DBGWRITE(dbg_level_error, "newEntry.InitCheck() failed! (%s)\n", GET_BERR_STR(newEntry.InitCheck()));
			afpError = afpParmErr;
		}

		// We need to signal to all clients that their picture of this
		// volume has changed.
		afpVolume->MakeDirty();
	}

	DBGWRITE(dbg_level_trace, "Returning %lu\n", afpError);

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

	// The first byte contains the afp command.
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
			// The request is too big, reduce the count to what
			// we can hold. We'll return to the client what we
			// actually read.
			afpReqCount = afpReply.GetBufferSize();

			DBGWRITE(dbg_level_info, "Resized afpReqCount to buffer size!!\n");
		}

		// Check to see if any area in the range we're reading from is
		// locked.
		if (fp_rangelock::RangeLocked(
						afpOffset,
						afpOffset + afpReqCount,
						afpSession,
						forkItem->entry
						))
		{
			DBGWRITE(dbg_level_warning, "****Range is currently locked!****\n");
			return( afpLockErr );
		}

		// Special case for files with extension ".res", we read from the data fork, but
		// tell the client it's the resource fork.
		if (forkItem->forkopen == kDataFork || forkItem->isResFile)
		{
			if (!forkItem->file->IsReadable())
			{
				DBGWRITE(dbg_level_trace, "Data fork is not readable!\n");
				return( afpAccessDenied );
			}

			DBGWRITE(dbg_level_trace, "Reading (DF) %lu bytes from %lld offset\n", afpReqCount, afpOffset);

			// Seek to the correct position to read from in the file.
			seekResult = forkItem->file->Seek(afpOffset, SEEK_SET);

			if (seekResult == B_ERROR)
			{
				// We had an error seeking to the position. Probably a bad
				// position was requested.
				DBGWRITE(dbg_level_trace, "Seek() failed!\n");
				return( afpParmErr );
			}

			// Now, perform the actual read from the file.
			afpActCount = forkItem->file->Read(afpReply.GetCurrentPosPtr(), afpReqCount);
		}
		else // Reading from resource fork
		{
			DBGWRITE(dbg_level_trace, "Reading (RF) %lu bytes from %lld offset\n", afpReqCount, afpOffset);

			// The resource fork should already be read in and in memory.
			if (forkItem->rsrcIO != NULL)
			{
				// Seek to the correct position to read from in the file.
				seekResult = forkItem->rsrcIO->Seek(afpOffset, SEEK_SET);

				if (seekResult == B_ERROR)
				{
					// We had an error seeking to the position. Probably a bad
					// position was requested.
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

	// The first byte contains the afp command.
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
			DBGWRITE(dbg_level_warning, "File is locked and cannot be written to! (%s)\n", forkItem->entry->Name());
			return( afpObjectLocked );
		}
	}

	// Special case for files with extension ".res". We write to data fork, but client
	// thinks it's the resource fork.
	if (forkItem->forkopen == kDataFork || forkItem->isResFile)
	{
		if (!forkItem->file->IsWritable())
		{
			DBGWRITE(dbg_level_warning, "Data fork is not writable!\n");
			return( afpParmErr );
		}

		// If the bit is set, then we are calculating the offset from
		// the end of the file.
		seekResult = forkItem->file->Seek(
						afpOffset,
						(afpFlag & kWriteStartEndFlag) ? SEEK_END : SEEK_SET
						);

		if (seekResult == B_ERROR)
		{
			// We had an error seeking to the position. Probably a bad
			// position was requested.
			DBGWRITE(dbg_level_warning, "Seek() failed!\n");
			return( afpParmErr );
		}

		DBGWRITE(dbg_level_info, "Writing (DF) %u bytes from %s at offset %u\n",
				afpReqCount,
				(afpFlag & kWriteStartEndFlag) ? "END" : "START",
				afpOffset
				);

		// Check to see if any area in the range we're writing is
		// locked. Note that we call Position() here because we don't
		// easily know the offset (range start).
		if (fp_rangelock::RangeLocked(
						seekResult,
						seekResult + afpReqCount,
						afpSession,
						forkItem->entry
						))
		{
			DBGWRITE(dbg_level_warning, "****Range is currently locked!****\n");
			return( afpLockErr );
		}

		// Call on the Be file object to do the BeOS specific file
		// system work for us.
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
	else // Writing to resource fork
	{
		// Since the Be file system doesn't support resource forks, we have to
		// use the file attributes stream of a file to hold the Mac resource
		// data. Unfortunately, the WriteAttr() and ReadAttr() functions were
		// never finished by Be and their offset parameters don't work. This
		// means we have to read the entire contents of the stream in one shot
		// no matter how big it is, manipulate the contents, then write the
		// entire stream back out in one shot again. This won't scale very well.

		DBGWRITE(dbg_level_trace, "Writing (RF) %lu bytes from %lu offset (from %s)\n",
				afpReqCount,
				afpOffset,
				(afpFlag & kWriteStartEndFlag) ? "END" : "START"
				);

		// Check to see if any area in the range we're writing is
		// locked. Note that we call Position() here because we don't
		// easily know the offset (range start).
		if (fp_rangelock::RangeLocked(
						afpOffset,
						afpOffset + afpReqCount,
						afpSession,
						forkItem->entry
						))
		{
			DBGWRITE(dbg_level_warning, "****Range is currently locked!****\n");
			return( afpLockErr );
		}

		// Move the file pointer to the right place in the file.
		seekResult = forkItem->rsrcIO->Seek(
								afpOffset,
								(afpFlag & kWriteStartEndFlag) ? SEEK_END : SEEK_SET
								);

		if (seekResult == B_ERROR)
		{
			// We had an error seeking to the position. Probably a bad
			// position was requested.
			DBGWRITE(dbg_level_warning, "Seek() failed for resource fork!\n");
			return( afpParmErr );
		}

		// Now, do the actual "write" into the buffer that holds our current
		// resource data.
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

	// The first byte contains the afp command.
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

	// Get the fork structure for the opened fork.
	forkItem = afpSession->GetForkItem(afpForkRef);
	afpError = (forkItem != NULL) ? AFP_OK : afpParmErr;

	if (AFP_SUCCESS(afpError))
	{
		if (forkItem->forkopen == kDataFork || forkItem->isResFile) {

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

			afpLock  = fp_rangelock::SessionRangeLocked(afpOffset, afpLength, afpSession, forkItem);
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
		else // Locking
		{
			// If the user wants us to set the offset from the end of the
			// file, then we have a lot more work to do.
			if (afpBRLFlags & kStartEndFlag)
			{
				DBGWRITE(dbg_level_trace, "Going from end of file!\n");
				afpOffset = (afpOffset < 0) ? (afpFileSize + afpOffset) : (afpFileSize - afpOffset);
			}
			else // From the beginning
			{
				if (afpOffset < 0)
				{
					// The offset can only be negative if we are working from the
					// end of a file.
					DBGWRITE(dbg_level_warning, "Negative offset from start!\n");
					return( afpParmErr );
				}

				// If the length is 0xFFFFFFFF, then we have special handling to do.
				if (((afpCommand == afpByteRangeLock) && (afpLength == 0xFFFFFFFF))	||
					((afpCommand == afpByteRangeLockExt) && ((uint64)afpLength == ULONGLONG_MAX)))
				{
					switch(afpOffset)
					{
						case 0:
							// 0 offset means we lock the entire range of the file.
							afpLength = afpFileSize;
							break;
						default:
							// The user wants to lock from offset to the end of the file.
							afpLength = (afpFileSize - afpOffset);
							break;
					}
				}
			}// from beginning

			fp_rangelock*	afpLock = new fp_rangelock(afpSession, forkItem);

			if (afpLock != NULL)
			{
				afpError = afpLock->Lock(afpOffset, afpLength);

				if (AFP_SUCCESS(afpError))
				{
					DBGWRITE(dbg_level_trace, "Locked...\n");
				}
				else
				{
					// We failed to lock, free the object and return the error.
					delete afpLock;

					DBGWRITE(dbg_level_warning, "Lock error when locking! (%lu)\n", afpError);
				}
			}
		}

		// We return the first byte of the newly locked range
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


