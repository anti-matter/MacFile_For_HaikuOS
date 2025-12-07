#include <fs_attr.h>
#include <DataIO.h>
#include <Node.h>
#include <StorageKit.h>

#include "afp.h"
#include "afpextattr.h"


/*
 * FPGetExtAttribute()
 *
 * Description:
 *		Retrieves an extended attribute from the file.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			afpPathname[MAX_AFP_PATH];
	char			afpAttrName[B_ATTR_NAME_LENGTH];
	BEntry			afpEntry;
	BNode			afpNode;
	fp_volume*		afpVolume		= NULL;
	int16			afpVolID		= 0;
	int32			afpDirID		= 0;
	uint16			afpBitmap		= 0;
	int64			afpOffset		= 0;
	int64			afpReqCount		= 0;
	int32			afpMaxReplySize	= 0;
	int8			afpPathtype		= 0;
	uint16			afpNameLen		= 0;
	int32			afpDataLen		= 0;
	AFPERROR		afpError		= AFP_OK;
	status_t		status			= B_OK;
	attr_info		attrInfo		= {0,0};

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//Advance beyond command and pad byte
	//
	afpRequest.Advance(sizeof(int16));

	afpVolID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpBitmap		= afpRequest.GetInt16();
	afpOffset		= afpRequest.GetInt64();
	afpReqCount		= afpRequest.GetInt64();
	afpMaxReplySize	= afpRequest.GetInt32();
	afpPathtype		= afpRequest.GetInt8();

	//
	//See if the user has the volume open and get a pointer to
	//the volume object.
	//
	if (!afpSession->HasVolumeOpen(afpVolID, &afpVolume))
	{
		DBGWRITE(dbg_level_error, "User does not have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathtype);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Could not retrieve pathname! (%lu)\n", afpError);
		return( afpError );
	}

	DBGWRITE(dbg_level_trace, "Pathname: %s\n", afpPathname);

	//
	//Advance if we need to in order to land on an even boundary
	//
	afpRequest.AdvanceIfOdd();

	//
	//Get the size of the attribute name string.
	//
	afpNameLen = afpRequest.GetInt16();

	DBGWRITE(dbg_level_trace, "Max reply size = %ul\n", afpMaxReplySize);

	//
	//Now get the string. Note that on return afpNameLen now contains the actual
	//length of the converted string.
	//
	afpError = afpRequest.GetUniString(afpAttrName, sizeof(afpAttrName), &afpNameLen);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Could not retrieve attribute name! (%lu)\n", afpError);
		return( afpError );
	}

	DBGWRITE(dbg_level_trace, "Getting attr, file=%s, attr=%s\n", afpPathname, afpAttrName);

	//
	//Set the entry object that will point to the object we're working with.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry,
								(afpBitmap & kXAttrNoFollow) ? false : true
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	//
	//Get a BNode to the file so we can get attribute control.
	//
	afpNode.SetTo(&afpEntry);
	status = afpNode.InitCheck();

	if (status != B_OK)
	{
		//
		//We failed to get a properly initialized BNode.
		//
		DBGWRITE(dbg_level_error, "BNode not initialized! (%s)\n", GET_BERR_STR(status));
		return( afpParmErr );
	}

	status = afpNode.GetAttrInfo(afpAttrName, &attrInfo);
	if (status == B_OK && attrInfo.size > 0)
	{
		afpDataLen = attrInfo.size;

		//Plum the first 2 items in the reply block. These will always be
		//returned in every case beyond here.
		afpReply.AddInt16(afpBitmap);
		afpReply.AddInt32(afpDataLen);

		if (afpMaxReplySize >= afpDataLen && afpReply.EnoughSpace(afpDataLen))
		{
				ssize_t read = afpNode.ReadAttr(
								afpAttrName,
								B_RAW_TYPE,
								0,
								afpReply.GetCurrentPosPtr(),
								min_c(afpDataLen, afpMaxReplySize)
								);

				afpReply.Advance(read);
		}
        else
        {
            DBGWRITE(dbg_level_error, "Not enough space in reply buffer!\n");
            afpError = afpParmErr;
        }

		*afpDataSize = afpReply.GetDataLength();
	}
	else
	{
		//
		//The attribute doesn't exist, return error.
		//
		DBGWRITE(dbg_level_error, "Attribute does not exist! (%s)\n", afpAttrName);

		//
		//01.20.10: According to spec, we return afpMiscErr for attributes not found
		//in the file.
		//
		return( afpMiscErr );
	}

	return( afpError );
}


/*
 * FPSetExtAttribute()
 *
 * Description:
 *		Sets an extended file attribute, optionally replacing existing
 *		content.
 *
 * Returns: AFPERROR
 */

AFPERROR FPSetExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			afpPathname[MAX_AFP_PATH];
	char			afpAttrName[B_ATTR_NAME_LENGTH];
	BEntry			afpEntry;
	BNode			afpNode;
	fp_volume*		afpVolume		= NULL;
	int16			afpVolID		= 0;
	int32			afpDirID		= 0;
	int64			afpOffset		= 0;
	int8			afpPathtype		= 0;
	uint16			afpBitmap		= 0;
	uint16			afpNameLen		= 0;
	uint32			afpAttrDataLen	= 0;
	AFPERROR		afpError		= AFP_OK;
	status_t		status			= B_OK;
	uint32			bytesWritten	= 0;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	//
	//Advance beyond command and pad byte
	//
	afpRequest.Advance(sizeof(int16));

	afpVolID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpBitmap		= afpRequest.GetInt16();
	afpOffset		= afpRequest.GetInt64();
	afpPathtype		= afpRequest.GetInt8();

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathtype);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Could not retrieve pathname! (%lu)\n", afpError);
		return( afpError );
	}

	//
	//A pad byte is inserted to make the buffer end on an
	//even boundary.
	//
	afpRequest.AdvanceIfOdd();

	//
	//Now get the string. Note that on return afpNameLen now contains the actual
	//length of the converted string.
	//
	afpNameLen 	= afpRequest.GetInt16();
	afpError 	= afpRequest.GetUniString(afpAttrName, sizeof(afpAttrName), &afpNameLen);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Could not retrieve attribute name! (%lu)\n", afpError);
		return( afpError );
	}

	DBGWRITE(dbg_level_info, "Setting attr, file=%s, attr=%s\n", afpPathname, afpAttrName);

	//
	//The attribute data length we're going to write.
	//
	afpAttrDataLen = afpRequest.GetInt32();

	//
	//See if the user has the volume open and get a pointer to
	//the volume object.
	//
	if (!afpSession->HasVolumeOpen(afpVolID, &afpVolume))
	{
		DBGWRITE(dbg_level_error, "User does not have volume open!\n");
		return( afpParmErr );
	}

	//
	//Set the entry object that will point to the object we're working with.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry,
								(afpBitmap & kXAttrNoFollow) ? false : true
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	//
	//Get a BNode to the file so we can get attribute control.
	//
	afpNode.SetTo(&afpEntry);
	status = afpNode.InitCheck();

	if (status != B_OK)
	{
		//
		//We failed to get a properly initialized BNode.
		//
		DBGWRITE(dbg_level_error, "BNode not initialized! (%s)\n", GET_BERR_STR(status));
		return( afpParmErr );
	}

	//
	//Get the size of the attribute data.
	//
	attr_info attrInfo;
	status = afpNode.GetAttrInfo(afpAttrName, &attrInfo);

	//
	//If the following bit is set, then we fail if the attribute already
	//exists. We know from the call above. Your guess is as good as mine
	//as to which error to return, the spec doesn't state.
	//
	if ((status == B_OK) && (afpBitmap & kXAttrCreate))
	{
		return( afpObjectExists );
	}
	else if (afpBitmap & kXAttrReplace)
	{
		return( afpObjectNotFound );
	}

	//
	//Now, write out the attribute data from the supplied buffer.
	//
	bytesWritten = afpNode.WriteAttr(
					afpAttrName,
					B_RAW_TYPE,
					0,
					afpRequest.GetCurrentPosPtr(),
					afpAttrDataLen
					);

	if (bytesWritten < afpAttrDataLen)
	{
		DBGWRITE(dbg_level_error, "WriteAttr failed to write the correct bytes!\n");
		return( afpMiscErr );
	}

	DBGWRITE(dbg_level_trace, "Wrote %lu bytes\n", bytesWritten);
	DBGWRITE(dbg_level_trace, "Returning: %lu\n", afpError);

	return( afpError );
}


/*
 * FPListExtAttribute()
 *
 * Description:
 *		List all extended attributes.
 *
 * Returns: AFPERROR
 */

AFPERROR FPListExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			afpPathname[MAX_AFP_PATH];
	char			name[B_ATTR_NAME_LENGTH];
	BEntry			afpEntry;
	BNode			afpNode;
	fp_volume*		afpVolume		= NULL;
	int16			afpVolID		= 0;
	int32			afpDirID		= 0;
	uint16			afpBitmap		= 0;
	int16			afpReqCount		= 0;
	int32			afpStartIndex	= 0;
	int32			afpMaxReplySize	= 0;
	int8			afpPathtype		= 0;
	int32			afpDataLen		= 0;
	uint32*			saveDL			= NULL;
	AFPERROR		afpError		= AFP_OK;
	status_t		status			= B_OK;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	//
	//Advance beyond command and pad byte
	//
	afpRequest.Advance(sizeof(int16));

	afpVolID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpBitmap		= afpRequest.GetInt16();
	afpReqCount		= afpRequest.GetInt16();
	afpStartIndex	= afpRequest.GetInt32();
	afpMaxReplySize	= afpRequest.GetInt32();
	afpPathtype		= afpRequest.GetInt8();

	//
	//See if the user has the volume open and get a pointer to
	//the volume object.
	//
	if (!afpSession->HasVolumeOpen(afpVolID, &afpVolume))
	{
		DBGWRITE(dbg_level_error, "User does not have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathtype);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Could not retrieve pathname! (%lu)\n", afpError);
		return( afpError );
	}

	//
	//Set the entry object that will point to the object we're working with.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry,
								(afpBitmap & kXAttrNoFollow) ? false : true
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	//
	//Get a BNode to the file so we can get attribute control.
	//
	status = afpNode.SetTo(&afpEntry);

	if (status == B_OK) {
		status = afpNode.InitCheck();
	}

	if (status != B_OK)
	{
		//
		//We failed to get a properly initialized BNode.
		//
		DBGWRITE(dbg_level_error, "BNode not initialized! (%s)\n", GET_BERR_STR(status));
		return( afpParmErr );
	}

	//
	//The bitmap is always returned to the caller.
	//
	afpReply.AddInt16(afpBitmap);

	//
	//Save the position of the datalen field...
	//
	saveDL = (uint32*)afpReply.GetCurrentPosPtr();
	afpReply.Advance(sizeof(int32));

	//
	//Now get the size of all the name strings and optionally plumb
	//them into the reply buffer.
	//
	while(afpNode.GetNextAttrName(name) == B_OK)
	{
        if (strcmp(name, AFP_RSRC_ATTRIBUTE) == 0)
        {
            // We don't offer up the resource fork as that is handled
            // as a file fork instead.
            continue;
        }

		afpDataLen += strlen(name) + sizeof(char);

		if (afpMaxReplySize > 0)
		{
			if (afpDataLen <= afpMaxReplySize)
			{
				DBGWRITE(dbg_level_info, "Adding %s to list of attr names\n", name);
				afpReply.AddRawData(name, strlen(name) + sizeof(char));
			}
			else
			{
				//
				//Our buffer will grow larger than the client can handle. Stop
				//including new names, adjust the reported data size and return.
				//
				DBGWRITE(dbg_level_error, "Data len exceeds client buffer size! (%lu)\n", afpMaxReplySize);

				afpDataLen -= strlen(name) + sizeof(char);
				break;
			}
		}
	}

	//
	//If we're only returning the size, we need to include the size of
	//the bitmap and datalen parameters.
	//
	if (afpMaxReplySize == 0) {

		afpDataLen += (sizeof(int16) + sizeof(int32));
	}

	DBGWRITE(dbg_level_trace, "Name list datalen = %lu\n", afpDataLen);

	//
	//Set the data length field now that we've calculated it completely and
	//of course the return reply size.
	//
	*saveDL 		= htonl(afpDataLen);
	*afpDataSize 	= afpReply.GetDataLength();

	DBGWRITE(dbg_level_info, "Returning %lu\n", afpError);

	return( afpError );
}


/*
 * FPRemoveExtAttribute()
 *
 * Description:
 *		Remove an extended file attribute.
 *
 * Returns: AFPERROR
 */

AFPERROR FPRemoveExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			afpPathname[MAX_AFP_PATH];
	char			afpAttrName[B_ATTR_NAME_LENGTH];
	BEntry			afpEntry;
	BNode			afpNode;
	fp_volume*		afpVolume		= NULL;
	int16			afpVolID		= 0;
	int32			afpDirID		= 0;
	uint16			afpBitmap		= 0;
	int8			afpPathtype		= 0;
	uint16			afpNameLen		= 0;
	AFPERROR		afpError		= AFP_OK;
	status_t		status			= B_OK;

	DBGWRITE(dbg_level_trace, "Enter...\n");

	//
	//Advance beyond command and pad byte
	//
	afpRequest.Advance(sizeof(int16));

	afpVolID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	afpBitmap		= afpRequest.GetInt16();
	afpPathtype		= afpRequest.GetInt8();

	//
	//See if the user has the volume open and get a pointer to
	//the volume object.
	//
	if (!afpSession->HasVolumeOpen(afpVolID, &afpVolume))
	{
		DBGWRITE(dbg_level_error, "User does not have volume open!\n");
		return( afpParmErr );
	}

	//
	//Get the pathname of the object we're working on.
	//
	afpError = afpRequest.GetString(afpPathname, sizeof(afpPathname), true, afpPathtype);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Could not retrieve pathname! (%lu)\n", afpError);
		return( afpError );
	}

	//
	//Set the entry object that will point to the object we're working with.
	//
	afpError = fp_objects::SetAFPEntry(
								afpVolume,
								afpDirID,
								afpPathname,
								afpEntry,
								(afpBitmap & kXAttrNoFollow) ? false : true
								);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}

	//
	//A pad byte is inserted to make the buffer end on an
	//even boundary.
	//
	afpRequest.AdvanceIfOdd();

	//
	//Now get the string. Note that on return afpNameLen now contains the actual
	//length of the converted string.
	//
	afpNameLen 	= afpRequest.GetInt16();
	afpError 	= afpRequest.GetUniString(afpAttrName, sizeof(afpAttrName), &afpNameLen);

	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "Could not retrieve attribute name! (%lu)\n", afpError);
		return( afpError );
	}

	DBGWRITE(dbg_level_info, "Setting attr, file=%s, attr=%s\n", afpPathname, afpAttrName);

	//
	//Get a BNode to the file so we can get attribute control.
	//
	status = afpNode.SetTo(&afpEntry);

	if (status == B_OK) {
		status = afpNode.InitCheck();
	}

	if (status == B_OK)
	{
		//
		//Use the BNode to delete the attribute by name. Try to
		//capture some common errors.
		//
		status = afpNode.RemoveAttr(afpAttrName);

		if (status != B_OK)
		{
			switch(status)
			{
				case B_ENTRY_NOT_FOUND:
					afpError = afpObjectNotFound;
					break;

				case B_NOT_ALLOWED:
					afpError = afpAccessDenied;
					break;

				default:
					afpError = afpParmErr;
					break;
			}
		}
		else {
			afpError = AFP_OK;
		}
	}

	DBGWRITE(dbg_level_info, "Returning %lu\n", afpError);

	return( afpError );
}




