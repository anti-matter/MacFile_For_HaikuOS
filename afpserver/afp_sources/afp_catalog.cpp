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
#include "afpreplay.h"
#include "commands.h"
#include "dsi_stats.h"
#include "fp_rangelock.h"
#include "afp_buffer.h"
#include "afp_session.h"
#include "fp_volume.h"
#include "fp_objects.h"
#include "dsi_scavenger.h"

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

	// The first word contains the afp command and padding byte.
	afpRequest.Advance(sizeof(int16));

	// Get the parms that define what we're to do and on what.
	afpVolumeID 	= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpFileBitmap	= afpRequest.GetInt16();
	afpDirBitmap	= afpRequest.GetInt16();
	afpPathType		= afpRequest.GetInt8();

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

	DBGWRITE(dbg_level_info, "FPGetFileDirParms volID=%d dirID=%lu fileBitmap=0x%04X dirBitmap=0x%04X pathType=%d\n",
				afpVolumeID, afpDirID, afpFileBitmap, afpDirBitmap, afpPathType);
	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
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

	DBGWRITE(dbg_level_trace, "DirID = %lu\n", afpDirID);
	DBGWRITE(dbg_level_trace, "Pathname = %s\n", afpPathname);

	// If the object is a directory, then set the attribute
	// bit that informs the client.
	if (afpEntry.IsDirectory()) {

		afpAttributes |= kFileDirIsDir;
	}

	// We don't support the following bitmaps, so we clear them. Note that
	// we don't clear the KFPProDos bit in AFP3.x since it is now the unicode
	// name bit.
	if (afpSession->GetAFPVersion() < afpVersion30)
	{
		if (afpDirBitmap & kFPProDos)	afpDirBitmap  &= ~kFPProDos;
		if (afpFileBitmap & kFPProDos)	afpFileBitmap &= ~kFPProDos;

		// AFP 3.x-only file bits
		afpFileBitmap &= ~(kFPExtDataForkLen | kFPLaunchLimit | kFPExtRsrcForkLen | kFPUnixPrivs);
	}

	if (afpDirBitmap & kFPDirShortName)		afpDirBitmap  &= ~kFPDirShortName;
	if (afpFileBitmap & kFPShortName)		afpFileBitmap &= ~kFPShortName;

	// The following items are returned in all versions of this call
	// regardless of the bitmaps or object type.
	afpReply.AddInt16(afpFileBitmap);
	afpReply.AddInt16(afpDirBitmap);
	afpReply.AddInt8(afpAttributes);

	DBGWRITE(dbg_level_info, "FPGetFileDirParms reply: fileBitmap_out=0x%04X dirBitmap_out=0x%04X attrs=0x%02X\n",
				afpFileBitmap, afpDirBitmap, afpAttributes);
	// If both bitmaps are null, then we are to just return
	// the bitmaps and the attributes (no padding per AFP spec).
	if ((afpFileBitmap == kFPFileNone) && (afpDirBitmap == kFPDirNone))
	{
		*afpDataSize = afpReply.GetDataLength();
		return( AFP_OK );
	}

	// Add a padding byte before the FDP data. Per AFP spec, this padding
	// is only present when file or directory parameter data follows.
	// AppleShare Client 3.7.4 crashes if it finds unexpected padding
	// bytes when both bitmaps are zero.
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

		DBGWRITE(dbg_level_trace, "FPGetFileDirParms reply hex (%d bytes): \n", *afpDataSize);
		DBG_DUMP_BUFFER((const char*)afpReply.GetBuffer(), *afpDataSize, dbg_level_trace);
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

	// The first word contains the afp command and padding byte.
	afpCommand = afpRequest.GetInt8();
	afpRequest.Advance(sizeof(int8));

	// Extract the enumerate parameters for the API.
	afpVolID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpFileBitmap	= afpRequest.GetInt16();
	afpDirBitmap	= afpRequest.GetInt16();
	afpReqCount		= afpRequest.GetInt16();

	// In AFP3.1 we may get the new Ext2 call which allows for a
	// larger number of files to be on the volume.
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

	// Get a pointer to the volume object we'll be working with
	afpVolume = FindVolume(afpVolID);

	if (afpVolume == NULL)
	{
		// The volume ID is not valid, bail...
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolID);
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

	// 06.12.09: There should be no reason to enumerate the parent of the
	// root directory. We must inhibit it as hackers could then gain access
	// to the entire disk and its contents.
	if (afpDirID == kParentOfRoot)
	{
		DBGWRITE(dbg_level_warning, "Parent of root directory is not allowed\n");
		return( afpParmErr );
	}

	// We don't support the following bitmaps, so we clear them.
	if (afpSession->GetAFPVersion() < afpVersion30)
	{
		if (afpDirBitmap & kFPDirProDOS)	afpDirBitmap  &= ~kFPDirProDOS;
		if (afpFileBitmap & kFPProDos)		afpFileBitmap &= ~kFPProDos;
	}

	if (afpDirBitmap & kFPDirShortName)		afpDirBitmap  &= ~kFPDirShortName;
	if (afpFileBitmap & kFPShortName)		afpFileBitmap &= ~kFPShortName;

	// The first thing we do is add the bitmaps to the reply buffer.
	afpReply.AddInt16(afpFileBitmap);
	afpReply.AddInt16(afpDirBitmap);

	// This is where the actual number of bytes in the buffer will
	// be inserted into the buffer.
	afpActCountSpot = (int16*)afpReply.GetCurrentPosPtr();
	afpReply.Advance(sizeof(int16));

	// Set the entry object that will point to the directory we're enumerating.
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

	DBGWRITE(dbg_level_trace, "Enumerating directory: dirID: %d, path: %s at index: %d, max_reply_size: %d\n",
			afpDirID, afpPathname, afpStartIndex, afpMaxReplySize);

	// OK, now the hard part. We need to iterate through all the directories
	// children and include them in the buffer until it is full.
	BDirectory	directory(&afpEntry);
	BEntry		entry;

	// Make sure the directory object got intialized properly.
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

		// Check to make sure we don't include items not to be included
		// in the return enumeration.
		if (	((afpDirBitmap == kFPDirNone) && (afpIsDirectory))		||
				((afpFileBitmap == kFPFileNone) && (!afpIsDirectory))	)
		{
			continue;
		}

		// Call the appropriate GetXXXParms calls.
		afpParmsBuffer.Rewind();
		afpAttributes = 0;

		if (afpIsDirectory)
		{
			// Check to make sure the session has search access to the directory
			// before including directories in the enumeration.
			afpError = afpCheckSearchAccess(afpSession, &afpEntry);
			if (AFP_FAILURE(afpError))
			{
				DBGWRITE(dbg_level_warning, "User doesn't have search access to the directory, hiding folders!\n");
				continue;
			}

			// Set the afp flag that marks this as a directory.
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
			// Check to make sure the session has read access to the directory
			// before including any files.
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
			// This is how many total objects we found during our search.
			afpObjectsFound++;

			DBGWRITE(dbg_level_trace, "  Item %d: parms_buf_len=%d, reply_buf_len=%d, start_index=%d, max_reply=%d\n",
				afpObjectsFound, afpParmsBuffer.GetDataLength(), afpReply.GetDataLength(), afpStartIndex, afpMaxReplySize);

			// If we haven't hit the start index yet, continue on...
			if (afpObjectsFound < afpStartIndex) {
				DBGWRITE(dbg_level_trace, "  Skipping item %d (before start index %d)\n",
					afpObjectsFound, afpStartIndex);
				continue;
			}

			int8*	structLen 		= NULL;
			int16*	extStructLen	= NULL;
			int32	sizeRequired;
			int16	len;

			// Check to make sure there is enough room in the buffer to add the
			// file/dir information.
			sizeRequired = afpParmsBuffer.GetDataLength() + afpReply.GetDataLength();

			// There are items after the file dirs parms that are added to the buffer,
			// so we add a little extra to be safe.
			sizeRequired += (4);

			// wMaxReplySize is the maximum size our return buffer can be.
			if (sizeRequired >= afpMaxReplySize)
			{
				DBGWRITE(dbg_level_trace, "  Breaking: sizeRequired (%d) >= afpMaxReplySize (%d)\n",
					sizeRequired, afpMaxReplySize);
				// We need to stay inside the callers max reply size in the request.
				break;
			}

			// Make sure we didn't get bigger then our reply buffer can handle.
			if (sizeRequired > afpReply.GetBufferSize())
			{
				DBGWRITE(dbg_level_trace, "  Breaking: sizeRequired (%d) > reply buffer size (%d)\n",
					sizeRequired, afpReply.GetBufferSize());
				// This should never happen (yeah right). In case it does, just
				// bail from here.
				break;
			}

			// Increment the count of the # of objects we're returning.
			afpActCount++;

			// The first byte (or word for AFP3.x) contains the struct len.
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

			// Push in the attributes flag (dir, yes/no)
			afpReply.AddInt8(afpAttributes);

			// For AFP3.x, we add an extra null byte for padding here.
			if (afpCommand > afpEnumerate) {
				afpReply.AddInt8(0);
			}

			// Insert the FDP parameters into the reply buffer.
			afpReply.AddRawData(afpParmsBuffer.GetBuffer(), afpParmsBuffer.GetDataLength());

			// Set the structure length byte in the buffer. The extra bytes are
			// for the first parms in the structure (structLen, attributes and padding).
			len = afpParmsBuffer.GetDataLength() + ((afpCommand > afpEnumerate) ? 4 : 2);

			// This entry must end on an even boundary.
			if (len % 2)
			{
				afpReply.AddInt8(0);
				len++;
			}

			// AFP3.x uses a word to describe the struct length vs. a byte
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
			// We had an error looking up the parms of an object.
			DBGWRITE(dbg_level_error, "Error finding parms for object\n");
		}

		// If we've gotten as much data as the client requested, then
		// exit this beast.
		if ((afpActCount >= afpReqCount) || (afpReply.GetDataLength() >= afpMaxReplySize))
		{
			break;
		}
	}

	// Set the actual number of objects found and in the buffer.
	*afpActCountSpot = htons(afpActCount);

	DBGWRITE(dbg_level_trace, "Number of objects enumerated = %lu\n", afpActCount);

	// This is the total size of the afp reply.
	*afpDataSize = afpReply.GetDataLength();

	// When no items were added to the reply, tell the client there's nothing
	// left to enumerate. Mac clients rely on this error to know when to stop.
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

	// The first word is afp command and padding byte.
	afpCommand = afpRequest.GetInt8();
	afpRequest.Advance(sizeof(int8));

	// Extract the volume and dir ID's as well as the bitmap that
	// tells what we're setting.
	afpVolID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpBitmap	= afpRequest.GetInt16();
	afpPathType	= afpRequest.GetInt8();

	// Get a pointer to the volume object we'll be working with
	afpVolume = FindVolume(afpVolID);

	if (afpVolume == NULL)
	{
		// The volume ID is not valid, bail...
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpVolID);
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

	// The remaining parameters for the call must start on an
	// even boundary.
	if ((afpRequest.GetCurrentPosPtr() - afpRequest.GetBuffer()) % 2) {

		afpRequest.Advance(sizeof(int8));
	}

	// Verify the bitmap contains valid settings, FPSetFileDirParms doesn't
	// support the following bitmaps. The user is supposed to use
	// FPSetFileParms or FPSetDirParms for these.
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

	// Set the entry object that will point to the object we're changing.
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

	// Check to make sure we are allowed to write on the volume.
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have write access!\n");
		return( afpError );
	}

	// Call the work routine to do all the work for us.
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

	// The first word contains the afp command and padding byte.
	afpRequest.Advance(sizeof(int16));

	// Extract the enumerate parameters for the API.
	afpVolumeID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpPathType		= afpRequest.GetInt8();

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

	// Pathname cannot be null since it contains the new dir name.
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
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
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

			// In Haiku, every new directory is currently being set denying
			// write permissions to users and guests. We'll set everything to
			// match the parent directory.

			dir.GetPermissions(&bperms);
			newEntry.SetPermissions(bperms);
		}

		// We need to signal to all clients that their picture of this
		// volume has changed.

		afpVolume->MakeDirty();
	}

	*afpDataSize = afpReply.GetDataLength();

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

	// First word is command and padding.
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

	// Pathname cannot be null since it contains the file/dir to delete.
	if (strlen(afpPathname) == 0)
	{
		DBGWRITE(dbg_level_warning, "Null pathname!\n");
		return( afpParmErr );
	}

	DBGWRITE(dbg_level_trace, "Deleting '%s' in dir %lu\n", afpPathname, afpDirID);

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

	int16 afpAttributes = 0;
	if (AFP_SUCCESS(fp_objects::GetAFPAttributes(&afpEntry, &afpAttributes)))
	{
		if (afpAttributes & kFileDeleteInhibit)
		{
			DBGWRITE(dbg_level_warning, "File/Dir is locked and cannot be deleted! (%s)\n", afpEntry.Name());
			return( afpObjectLocked );
		}
	}

	// We need to check write access to the parent directory.
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

	// Now call the object method that does all the nasty work for us.
	status_t status = afpEntry.Remove();

	if (status != B_OK)
	{
		DBGWRITE(dbg_level_error, "Remove failed! reason = %s (%lu)\n", (status == B_NO_INIT) ? "B_NO_INIT" : "UNK", status);
		afpError = afpParmErr;
	}

	// We need to signal to all clients that their picture of this
	// volume has changed.
	if (AFP_SUCCESS(afpError)) {

		afpVolume->MakeDirty();
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

	// The first byte contains the afp command.
	afpRequest.Advance(sizeof(int16));

	afpVolumeID		= afpRequest.GetInt16();
	afpSrcDirID		= afpRequest.GetInt32();
	afpDstDirID		= afpRequest.GetInt32();
	afpPathType		= afpRequest.GetInt8();

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

	// *****************
	// Beging by getting the source file/dir entry.
	// *****************

	// 
	// Get the pathname of the object we're working on.
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	// Set the entry object that will point to this afp object.
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

	// Moving an object removes it from its current directory, so we need
	// write access to the source's PARENT directory. Checking the source
	// object itself is the wrong bit for a directory (its own bits would be
	// used instead of the parent's), which could allow a client to move a
	// directory it has no right to remove.
	{
		BDirectory	srcParent;
		BEntry		srcParentEntry;

		if (afpSrcEntry.GetParent(&srcParent) != B_OK)
		{
			DBGWRITE(dbg_level_warning, "Couldn't get parent of source object!\n");
			return( afpObjectNotFound );
		}

		if (srcParent.GetEntry(&srcParentEntry) != B_OK)
		{
			DBGWRITE(dbg_level_warning, "Couldn't get entry of source parent!\n");
			return( afpObjectNotFound );
		}

		afpError = afpCheckWriteAccess(afpSession, afpVolume, &srcParentEntry);
	}

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "User doesn't have write access to the source directory!\n");
		return( afpError );
	}

	// Make sure we're allowed to move/rename this object.
	afpError = fp_objects::GetAFPAttributes(&afpSrcEntry, &afpAttributes);

	if (AFP_SUCCESS(afpError))
	{
		if (afpAttributes & (kFileDeleteInhibit | kFileRenameInhibit))
		{
			return( afpObjectLocked );
		}
	}

	// *****************
	// Now get the destination BDirectory we're moving to
	// *****************

	afpPathType = afpRequest.GetInt8();

	// Get the pathname of the object we're working on.
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathType);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	// Set the entry object that will point to this afp object.
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

	// Check to make sure we are allowed to write on the volume.
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpDstEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_trace, "User doesn't have write access!\n");
		return( afpError );
	}

	// Create the BDirectory object.
	afpMoveToDir.SetTo(&afpDstEntry);

	if (afpMoveToDir.InitCheck() != B_OK)
	{
		afpError = afpDirNotFound;
	}
	else
	{
		status_t	status;

		// Get the (optionally) supplied new name after the move.
		afpPathType = afpRequest.GetInt8();

		afpError = afpRequest.GetString(afpNewPathname, sizeof(afpNewPathname), true, afpPathType);

		if (!AFP_SUCCESS(afpError))
		{
			return( afpError );
		}

		// If the source and destination dir ID's are the same, then we're
		// just renaming the file.
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

		// If the move succeeded see if we should rename the file.
		if (AFP_SUCCESS(afpError))
		{
			if (strlen(afpNewPathname) > 0)
			{
				status = afpSrcEntry.Rename(afpNewPathname);

				// The client tries to rename the moved dir to its own name, I
				// don't know why. We'll fail here if don't ignore the error.
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
 *		Rename a file or directory.
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

	DBGWRITE(dbg_level_trace, "Enter\n");
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

	// Set the entry object that will point to this afp object.
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

	// Check to make sure we are allowed to write on the volume.
	afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "Doesn't have write access to parent directory!\n");
		return( afpError );
	}

	// Make sure we're allowed to rename this object.
	afpError = fp_objects::GetAFPAttributes(&afpEntry, &afpAttributes);

	if (AFP_SUCCESS(afpError))
	{
		if (afpAttributes & (kFileDeleteInhibit | kFileRenameInhibit))
		{
			DBGWRITE(dbg_level_warning, "Object is locked for renaming! (%s)\n", afpEntry.Name());
			return( afpObjectLocked );
		}
	}

	// Get the pathtype for the new name.
	afpPathType = afpRequest.GetInt8();

	// Get the pathname of the object we're working on.
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

	// We need to signal to all clients that their picture of this
	// volume has changed.
	afpVolume->MakeDirty();

	return( afpError );
}


/*
 * FPCopyFile()
 *
 * Description:
 *		Copy a file from one location to another.
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

	// Skip the first byte which is afp command.
	afpRequest.Advance(sizeof(int16));

	afpSrcVolID		= afpRequest.GetInt16();
	afpSrcDirID		= afpRequest.GetInt32();
	afpDstVolID		= afpRequest.GetInt16();
	afpDstDirID		= afpRequest.GetInt32();

	// Get the pathname of the object we're copying.
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

	// Get the destination pathname we're copying to
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

	// And now the [optional] new name for the file
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

	// Make sure the client has opened the volumes that the file copy
	// is happening on.

	if ((afpSrcVolume = FindVolume(afpSrcVolID)) == NULL)
	{
		// The volume ID is not valid, bail...
		DBGWRITE(dbg_level_warning, "Src volume not found! (%d)\n", afpSrcVolID);
		return( afpParmErr );
	}

	if ((afpDstVolume = FindVolume(afpDstVolID)) == NULL)
	{
		// The volume ID is not valid, bail...
		DBGWRITE(dbg_level_warning, "Dst volume not found! (%d)\n", afpDstVolID);
		return( afpParmErr );
	}

	if ((!afpSession->HasVolumeOpen(afpSrcVolume)) || (!afpSession->HasVolumeOpen(afpDstVolume)))
	{
		// Nope, client made a boo boo, return parm error.
		DBGWRITE(dbg_level_warning, "User doesn't have volume(s) open!\n");
		return( afpParmErr );
	}

	// Set the entry object that will point to the object we're copying.
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

	// As per the spec, we only do copy file on files, NOT directories.
	if (!afpSrcEntry.IsFile()) {

		return( afpObjectTypeErr );
	}

	// Get the entry object for the destination directory.
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

	// Now construct the new full pathname to what will be the newly created object.
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

	// Check to make sure this session has write access to the destination
	// directory location.
	afpError = afpCheckWriteAccess(afpSession, afpDstVolume, &afpDstEntry);

	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}

	// Now that we have the new pathname all set, we need to actually create
	// a new file in the destination.
	BDirectory	destDir(&afpDstEntry);

	if (destDir.CreateFile(afpNewName, &destFile, true) != B_OK)
	{
		DBGWRITE(dbg_level_error, "CreateFile() failed!\n");
		return( afpParmErr );
	}

	srcFile.SetTo(&afpSrcEntry, B_READ_ONLY);

	if (srcFile.InitCheck() != B_OK)
	{
		DBGWRITE(dbg_level_error, "InitCheck() failed on src BFile!\n");
		return( afpParmErr );
	}

	// Now finally, perform the actual copy operation.
	if (fp_objects::CopyFile(srcFile, destFile) != B_OK)
	{
		DBGWRITE(dbg_level_error, "CopyFile() failed!\n");
		return( afpParmErr );
	}

	// We need to signal to all clients that their picture of this
	// volume has changed.
	afpDstVolume->MakeDirty();

	return( AFP_OK );
}


