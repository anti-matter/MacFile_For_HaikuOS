#include <Locker.h>
#include <fs_attr.h>
#include <DataIO.h>
#include <memory>

#include "afpdesk.h"
#include "afpvolume.h"
#include "afp_buffer.h"
#include "dsi_connection.h"
#include "fp_volume.h"
#include "fp_objects.h"

std::recursive_mutex desk_mutex;

/*
 * FPOpenDT()
 *
 * Description:
 *		Opens the desktop database for access.
 *
 * Returns: AFPERROR
 */

AFPERROR FPOpenDT(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	char			afpDeskPath[MAX_AFP_PATH];
	BFile*			afpDTFile	= NULL;
	BEntry*			afpDTEntry	= NULL;
	BDirectory*		root 		= NULL;
	fp_volume*		afpVolume	= NULL;
	int16			afpVolumeID	= 0;
	AFPERROR		afpError	= AFP_OK;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));
	
	//
	//The only parameter for this API is the volume ID for
	//which we want to access the DT database.
	//
	afpVolumeID = afpRequest.GetInt16();
	
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
	//Build the full path to the desktop database.
	//
	sprintf(afpDeskPath, "%s/%s", afpVolume->GetPath()->Path(), DESKTOP_FILE_NAME);

	root = afpVolume->GetDirectory();
	
	//
	//Check to see if the desktop db already exists at the root.
	//
	if (!root->Contains(DESKTOP_FILE_NAME))
	{
		FINDER_INFO	finfo;
		
		//
		//Nope, file doesn't exist, create it.
		//
		if (root->CreateFile(DESKTOP_FILE_NAME, NULL) != B_OK)
		{
			//
			//Massive error, couldn't create the desktop directory
			//
			DBGWRITE(dbg_level_warning, "Failed to create desktop file\n");
			return( afpParmErr );
		}
		
		//
		//Get a BEntry the points to the directory we just created.
		//
		afpDTEntry = new BEntry(afpDeskPath);
		
		if (afpDTEntry != NULL)
		{
			if (afpDTEntry->InitCheck() != B_OK)
			{
				DBGWRITE(dbg_level_warning, "Failed to create DT entry!\n");
				afpError = afpParmErr;
			}
			else
			{
				//
				//The desktop file should be invisible to Mac clients. Set
				//the Finder Info so this happens.
				//
				finfo.fdLocation.x	= 20;
				finfo.fdLocation.y	= 20;
				finfo.fdFldr		= 0;
				finfo.fdFlags		= kFDInvisible;
				
				afpError = fp_objects::SetAFPFinderInfo(afpDTEntry, &finfo);
				
				if (!AFP_SUCCESS(afpError))
				{
					//
					//Failure setting the AFP attributes for the new directory!
					//
					DBGWRITE(dbg_level_warning, "Failure setting FInfo on new desktop file\n");
				}
			}
		}
	}
	else
	{
		afpDTEntry 	= new BEntry(afpDeskPath);
		afpError 	= (afpDTEntry != NULL) ? AFP_OK : afpParmErr;
	}
	
	//
	//Now that we've either created the desktop file or it already exists,
	//open the file and get a BFile object for it.
	//
	if (AFP_SUCCESS(afpError))
	{
		afpError	= afpParmErr;
		afpDTFile 	= new BFile(afpDeskPath, B_READ_WRITE);
				
		if (afpDTFile != NULL)
		{
			if (afpDTFile->InitCheck() != B_OK)
			{
				//
				//Something really strange happend, the file exists but we can't
				//get an object to point to it.
				//
				DBGWRITE(dbg_level_error, "Failure in InitCheck() for desktop file\n");
			}
			else
			{
				uint16	afpRefNum = 0;
				
				//
				//Call on the session object to remember this opened desktop
				//database. The session is responsible for maintaining the
				//opened list as well as closing all open desks.
				//
				afpError = afpSession->OpenDesktop(
										afpDTEntry,
										afpDTFile,
										afpVolumeID,
										&afpRefNum
										);
										
				if (AFP_SUCCESS(afpError))
				{					
					afpReply.AddInt16(afpRefNum);
					*afpDataSize = afpReply.GetDataLength();
				}
			}
		}
	}
	
	//
	//If there were any errors, we need to make sure we clean up
	//after ourselves.
	//
	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_error, "FPOpenDT failed! Releasing memory!\n");
		
		if (afpDTEntry != NULL) {
		
			delete afpDTEntry;
		}
		
		if (afpDTFile != NULL) {
		
			delete afpDTFile;
		}
	}

	return( afpError );
}


/*
 * FPCloseDT()
 *
 * Description:
 *		Closes the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPCloseDT(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)
	
	afp_buffer		afpRequest(afpReqBuffer);
	AFPERROR		afpError 	= AFP_OK;
	int16			afpRefID	= 0;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));
	
	afpRefID = afpRequest.GetInt16();
	afpError = afpSession->CloseDesktop(afpRefID);

	return( afpError );
}


/*
 * FPAddIcon()
 *
 * Description:
 *		Add an icon to the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPAddIcon(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)
	
	afp_buffer		afpRequest(afpReqBuffer);
	AFPERROR		afpError 	= AFP_OK;
	int16			afpRefID	= 0;
	DESKTOP_ENTRY	dtEntry;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	memset(&dtEntry, 0, sizeof(dtEntry));
	
	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));
	
	afpRefID = afpRequest.GetInt16();
	
	dtEntry.fileCreator = afpRequest.pull_num<uint32>();
	dtEntry.fileType = afpRequest.pull_num<uint32>();	
	dtEntry.iconType = afpRequest.GetInt8();
	
	afpRequest.Advance(sizeof(int8));
	
	dtEntry.tag = afpRequest.pull_num<uint32>();
	dtEntry.dataSize = afpRequest.pull_num<uint16>();
	dtEntry.entryType = ENTRY_TYPE_ICON;
	
	if (dtEntry.dataSize > MAX_DESKTOP_DATA_SIZE)
	{
		DBGWRITE(dbg_level_warning, "Icon too big! (%lu)\n", (int)dtEntry.dataSize);
		return afpParmErr;
	}
	
	afpRequest.GetRawData(&dtEntry.data, dtEntry.dataSize);
	
	afpError = afp_AddEntry(afpSession, afpRefID, &dtEntry);
	
	DBGWRITE(dbg_level_info, "Added icon size = %lu, with error: %d\n", (int)dtEntry.dataSize, afpError);
						
	return( afpError );
}


/*
 * FPGetIcon()
 *
 * Description:
 *		Get an icon from the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetIcon(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	AFPERROR		afpError	= AFP_OK;
	int16			afpRefID	= 0;
	DESKTOP_ENTRY	dtEntry;
	DESKTOP_ENTRY	iconEntry;
	
	DBGWRITE(dbg_level_trace, "Enter\n");

	memset(&dtEntry, 0, sizeof(dtEntry));
	memset(&iconEntry, 0, sizeof(iconEntry));
	
	//First word is command and padding.
	afpRequest.Advance(sizeof(int16));
	
	afpRefID = afpRequest.GetInt16();

	dtEntry.entryType = ENTRY_TYPE_ICON;
	dtEntry.fileCreator = afpRequest.pull_num<uint32>();
	dtEntry.fileType = afpRequest.pull_num<uint32>();	
	dtEntry.iconType = afpRequest.pull_num<int8>();

	afpRequest.Advance(sizeof(int8));
	
	dtEntry.dataSize = afpRequest.pull_num<uint16>();
	
	afpError = afp_FindDTEntry(
					afpSession,
					afpRefID,
					&dtEntry,
					0,
					&iconEntry
					);
					
	if (AFP_SUCCESS(afpError) && dtEntry.dataSize > 0)
	{
		//
		//We found the bitmap and the caller actually has memory
		//reserved for the bimap.
		//
		//As per the spec, even if the client didn't reserve enough
		//space, we still copy what we can.
		//
		
		int16 copySize = min_c(iconEntry.dataSize, dtEntry.dataSize);
		
		afpReply.AddRawData(iconEntry.data, copySize);
		*afpDataSize = afpReply.GetDataLength();
		
		DBGWRITE(dbg_level_info, "Returning icon:\n");
		DUMP_DT_ENTRY(&iconEntry, true);
		DBG_DUMP_BUFFER((char*)iconEntry.data, copySize, dbg_level_info);
	}
	
	return( afpError );
}


/*
 * FPGetIconInfo()
 *
 * Description:
 *		Get information regarding an icon from the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetIconInfo(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	AFPERROR		afpError		= AFP_OK;
	DESKTOP_ENTRY	searchEntry;
	DESKTOP_ENTRY	iconEntry;
	
	DBGWRITE(dbg_level_trace, "Enter\n");

	memset(&searchEntry, 0, sizeof(searchEntry));
	memset(&iconEntry, 0, sizeof(iconEntry));

	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));
	
	//
	//Build the search criteria for the icon we're looking for
	//from the supplied parameters.
	//
	searchEntry.entryType = ENTRY_TYPE_ICON;
	const auto afpRefID = afpRequest.pull_num<int16>();
	searchEntry.fileCreator = afpRequest.pull_num<uint32>();
	const auto afpIconIndex = afpRequest.pull_num<uint16>();
	
	//
	//Use the handy-dandy worker function to do the search.
	//
	afpError = afp_FindDTEntry(
					afpSession,
					afpRefID,
					&searchEntry,
					afpIconIndex,
					&iconEntry
					);
					
	if (AFP_SUCCESS(afpError))
	{
		afpReply.push_num<uint32>(iconEntry.tag);
		afpReply.push_num<uint32>(iconEntry.fileType);
		afpReply.push_num(iconEntry.iconType);
		afpReply.push_num<int8>(0);
		afpReply.push_num<uint16>(iconEntry.dataSize);
		
		*afpDataSize = afpReply.GetDataLength();
	}
	
	return( afpError );
}


/*
 * FPAddComment()
 *
 * Description:
 *		Add a comment to the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPAddComment(
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
	char			afpComment[MAX_COMMENT_SIZE+1];
	BEntry			afpEntry;
	fp_volume*		afpVolume		= NULL;
	AFPERROR		afpError		= AFP_OK;
	int16			afpRefID		= 0;
	int8			afpPathtype		= 0;
	uint32			afpDirID		= 0;
	OPEN_DESK_ITEM*	afpDeskItem		= NULL;

	DBGWRITE(dbg_level_trace, "Enter\n");
	
	//First word is command and padding.
	afpRequest.Advance(sizeof(int16));
	
	//
	//Extract the new entry information from the supplied parms.
	//
	afpRefID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathtype	= afpRequest.GetInt8();
	
	//
	//Get the name of the object we're saving the comment for.
	//
	afpError = afpRequest.GetString(
					afpPathname,
					sizeof(afpPathname), 
					true, 
					afpPathtype
					);
	
	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "String size error copying path!\n");
		return( afpParmErr );
	}
	
	//
	//The next name starts on an even boundary
	//
	if ((afpRequest.GetCurrentPosPtr() - afpReqBuffer) % 2)
		afpRequest.Advance(sizeof(int8));
	
	//
	//Now, get the comment as passed from the client.
	//
	afpError = afpRequest.GetString(
					afpComment,
					sizeof(afpComment), 
					true, 
					kLongNames
					);
	
	if (!AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_warning, "String size error copying comment!\n");
		return( afpParmErr );
	}
	
	//
	//Get a pointer to the open desk items data structure so we can
	//get info about the actual dt file.
	//
	afpDeskItem = afpSession->GetDeskItem(afpRefID);
	
	if (afpDeskItem == NULL)
	{
		DBGWRITE(dbg_level_warning, "Failed to get desk item by refID!\n");
		return( afpParmErr );
	}
	
	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpDeskItem->volID);
	
	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpDeskItem->volID);
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
								
	if (AFP_SUCCESS(afpError))
	{
		BNode		node(&afpEntry);
		off_t		fsize	= 0;
		
		node.Lock();
		
		fsize = node.WriteAttr(
						AFP_CMNT_ATTRIBUTE,
						B_RAW_TYPE,
						0,
						afpComment,
						strlen(afpComment)
						);
		
		afpError = (fsize < B_OK) ? afpParmErr : AFP_OK;
		
		node.Unlock();
	}
	else
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
		return( afpError );
	}
		
	return( afpError );
}


/*
 * FPGetComment()
 *
 * Description:
 *		Get a comment for a file/dir from the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetComment(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	char			afpPathname[MAX_AFP_PATH];
	char			afpComment[MAX_COMMENT_SIZE+1];
	BEntry			afpEntry;
	fp_volume*		afpVolume		= NULL;
	AFPERROR		afpError		= AFP_OK;
	int16			afpRefID		= 0;
	int8			afpPathtype		= 0;
	uint32			afpDirID		= 0;
	OPEN_DESK_ITEM*	afpDeskItem		= NULL;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));

	//
	//Extract the new entry information from the supplied parms.
	//
	afpRefID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathtype	= afpRequest.GetInt8();
	
	//
	//Get the name of the object we're getting the comment from.
	//
	afpError = afpRequest.GetString(
					afpPathname,
					sizeof(afpPathname), 
					true, 
					afpPathtype
					);
	
	if (!AFP_SUCCESS(afpError))
	{
		return( afpParmErr );
	}
	
	//
	//Get a pointer to the open desk items data structure so we can
	//get info about the actual dt file.
	//
	afpDeskItem = afpSession->GetDeskItem(afpRefID);
	
	if (afpDeskItem == NULL)
	{
		DBGWRITE(dbg_level_warning, "Failed to get desk item by refID!\n");
		return( afpParmErr );
	}
	
	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpDeskItem->volID);
	
	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpDeskItem->volID);
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
								
	if (AFP_SUCCESS(afpError))
	{
		BNode		node(&afpEntry);
		off_t		fsize	= 0;
		attr_info	info	= {0,0};
		
		//
		//Initialize the afperror to say we didn't find a comment
		//for the file.
		//
		afpError = afpItemNotFound;
		
		node.Lock();
		
		if (node.GetAttrInfo(AFP_CMNT_ATTRIBUTE, &info) == B_OK)
		{
			if (info.size > 0)
			{
				memset(afpComment, 0, sizeof(afpComment));
				
				fsize = node.ReadAttr(
								AFP_CMNT_ATTRIBUTE,
								B_RAW_TYPE,
								0,
								afpComment,
								min_c(info.size, sizeof(afpComment)-1)
								);
								
				if (fsize < B_OK)
				{
					DBGWRITE(dbg_level_warning, "Failed to read comment!\n");
					afpError = afpParmErr;
				}
				else
				{
					afpError = AFP_OK;
				}
			}
		}
				
		node.Unlock();
	}
	else
	{
		DBGWRITE(dbg_level_warning, "Object not found! (name = %s, dirID = %lu)\n", afpPathname, afpDirID);
	}
					
	if (AFP_SUCCESS(afpError))
	{
		//
		//We succeeded in finding the comment. Stuff the comment
		//into the reply blob.
		//
		afpReply.AddCStringAsPascal(afpComment);
				
		*afpDataSize = afpReply.GetDataLength();
	}
	
	return( afpError );
}


/*
 * FPRemoveComment()
 *
 * Description:
 *		Removes a comment from the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPRemoveComment(
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
	AFPERROR		afpError		= AFP_OK;
	int16			afpRefID		= 0;
	int8			afpPathtype		= 0;
	uint32			afpDirID		= 0;
	OPEN_DESK_ITEM*	afpDeskItem		= NULL;

	DBGWRITE(dbg_level_trace, "Enter\n");
	
	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));
	
	//
	//Extract the new entry information from the supplied parms.
	//
	afpRefID	= afpRequest.GetInt16();
	afpDirID	= afpRequest.GetInt32();
	afpPathtype	= afpRequest.GetInt8();
	
	//
	//Get the name of the object we're saving the comment for.
	//
	afpError = afpRequest.GetString(
					afpPathname,
					sizeof(afpPathname), 
					true, 
					afpPathtype
					);
	
	if (!AFP_SUCCESS(afpError))
	{
		return( afpParmErr );
	}
	
	//
	//Get a pointer to the open desk items data structure so we can
	//get info about the actual dt file.
	//
	afpDeskItem = afpSession->GetDeskItem(afpRefID);
	
	if (afpDeskItem == NULL)
	{
		DBGWRITE(dbg_level_warning, "Failed to get desk item by refID!\n");
		return( afpParmErr );
	}
	
	//
	//Get a pointer to the volume object we'll be working with
	//
	afpVolume = FindVolume(afpDeskItem->volID);
	
	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail...
		//
		DBGWRITE(dbg_level_warning, "Volume not found! (%d)\n", afpDeskItem->volID);
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
								
	if (AFP_SUCCESS(afpError))
	{
		BNode	node(&afpEntry);
		
		node.Lock();
		
		afpError = (node.RemoveAttr(AFP_CMNT_ATTRIBUTE) == B_OK) ? afpParmErr : AFP_OK;
		
		node.Unlock();
	}
	
	return( afpError );
}


/*
 * FPAddAPPL()
 *
 * Description:
 *		Adds an APPL entry to the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPAddAPPL(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)
	
	afp_buffer		afpRequest(afpReqBuffer);
	AFPERROR		afpError		= AFP_OK;
	int16			afpRefID		= 0;
	int8			afpPathtype		= 0;
	DESKTOP_ENTRY	newEntry{};

	DBGWRITE(dbg_level_trace, "Enter\n");

	memset(&newEntry, 0, sizeof(newEntry));

	//First word is command and padding.
	afpRequest.Advance(sizeof(int16));
	
	afpRefID 				= afpRequest.pull_num<int16>();
	newEntry.dirID 			= afpRequest.pull_num<int32>();
	newEntry.fileCreator	= afpRequest.pull_num<uint32>();
	newEntry.tag 			= afpRequest.pull_num<uint32>();
	newEntry.entryType 		= ENTRY_TYPE_APPL;
	
	afpPathtype = afpRequest.pull_num<int8>();
	afpError = afpRequest.GetString(
					newEntry.path,
					sizeof(newEntry.path), 
					true, 
					afpPathtype
					);
	
	if (!AFP_SUCCESS(afpError))
	{
		return( afpParmErr );
	}
	
	afpError = afp_AddEntry(afpSession, afpRefID, &newEntry);
	
	if (AFP_SUCCESS(afpError))
	{
		DBGWRITE(dbg_level_info, "Added APPL:\n");
		DUMP_DT_ENTRY(&newEntry, false);
	}
	else
	{
		DBGWRITE(dbg_level_info, "Error adding APPL: %d\n", afpError);
	}
	
	return( afpError );
}


/*
 * FPGetAPPL()
 *
 * Description:
 *		Get an APPL entry for a file from the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetAPPL(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer, SRVR_REQUEST_QUANTUM_SIZE);
	AFPERROR		afpError		= AFP_OK;
	int16			afpRefID		= 0;
	int16			afpBitmap		= 0;
	uint16			afpIndex		= 0;
	DESKTOP_ENTRY	searchEntry;
	DESKTOP_ENTRY	foundEntry;
	
	DBGWRITE(dbg_level_trace, "Enter\n");

	memset(&searchEntry, 0, sizeof(searchEntry));
	memset(&foundEntry, 0, sizeof(foundEntry));

	//First word is command and padding.
	afpRequest.Advance(sizeof(int16));

	afpRefID 				= afpRequest.pull_num<int16>();
	searchEntry.fileCreator	= afpRequest.pull_num<uint32>();
	searchEntry.entryType 	= ENTRY_TYPE_APPL;
	afpIndex				= afpRequest.pull_num<uint16>();
	afpBitmap				= afpRequest.pull_num<int16>();
	
	afpError = afp_FindDTEntry(
					afpSession,
					afpRefID,
					&searchEntry,
					afpIndex,
					&foundEntry
					);
					
	if (AFP_SUCCESS(afpError))
	{
		OPEN_DESK_ITEM*		deskitem 	= NULL;
		fp_volume*			afpVolume	= NULL;
		BEntry				afpEntry;
		
		deskitem = afpSession->GetDeskItem(afpRefID);
		
		if (deskitem == NULL) {
			return( afpParmErr );
		}
		
		afpVolume = FindVolume(deskitem->volID);
	
		if (afpVolume == NULL) {
			return( afpParmErr );
		}
		
		//
		//Set the entry object that will point to this afp object.
		//
		afpError = fp_objects::SetAFPEntry(
									afpVolume,
									foundEntry.dirID,
									foundEntry.path,
									afpEntry
									);
									
		if (!AFP_SUCCESS(afpError)) {
			return( afpError );
		}

		afpReply.push_num(afpBitmap);
		afpReply.push_num(foundEntry.tag);
		
		afpError = fp_objects::fp_GetFileParms(
								afpSession,
								afpVolume,
								&afpEntry,
								afpBitmap,
								&afpReply
								);
								
		if (AFP_SUCCESS(afpError))
		{
			*afpDataSize = afpReply.GetDataLength();
		}
	}
		
	return( afpError );
}


/*
 * FPRemoveAPPL()
 *
 * Description:
 *		Removes an APPL entry from the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR FPRemoveAPPL(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)
	
	afp_buffer		afpRequest(afpReqBuffer);
	AFPERROR		afpError		= AFP_OK;
	int16			afpRefID		= 0;
	int8			afpPathtype		= 0;
	DESKTOP_ENTRY	searchEntry;

	memset(&searchEntry, 0, sizeof(searchEntry));

	//
	//First word is command and padding.
	//
	afpRequest.Advance(sizeof(int16));
	
	afpRefID 				= afpRequest.pull_num<int16>();
	searchEntry.dirID 		= afpRequest.GetInt32();
	searchEntry.fileCreator	= afpRequest.GetInt32();
	searchEntry.entryType 	= ENTRY_TYPE_APPL;
	afpPathtype 			= afpRequest.GetInt8();
	
	afpError = afpRequest.GetString(
					searchEntry.path,
					sizeof(searchEntry.path), 
					true, 
					afpPathtype
					);
	
	//
	//Although the spec says to, we don't check to make sure that
	//the actual application really exists on the volume. Why? Because
	//when you move or delete a file, the client always calls this
	//AFTER it deletes or moves it.
	//

	afp_RemoveEntry(afpSession, afpRefID, &searchEntry);
	
	return( afpError );
}



//===============================Worker Routines===============================

/*
 * afp_GetDesktopEntries()
 *
 * Description:
 *		Read database into memory.
 *
 * Returns: int32
 */

std::unique_ptr<DESKTOP_ENTRY[]> afp_GetDesktopEntries(
	BFile* desktop_file,
	int32* entry_count
	)
{
	std::lock_guard lock(desk_mutex);

	off_t desk_file_size;
	auto status = desktop_file->GetSize(&desk_file_size);
	if (status != B_OK)
	{
		DBGWRITE(dbg_level_error, "Error getting desktop file size!\n");
		return nullptr;
	}
	
	if (desk_file_size < sizeof(DESKTOP_ENTRY))
	{
		// No entries in the db yet.
		return nullptr;
	}
	
	int32 num_entries = desk_file_size / sizeof(DESKTOP_ENTRY);
	
	DBGWRITE(dbg_level_info, "Desktop file size: %lu\n", desk_file_size);
	DBGWRITE(dbg_level_info, "Desktop entries: %lu\n", num_entries);
	
	// Read in the entire db for performance reasons.
	
	desktop_file->Seek(0, SEEK_SET);
	auto entries = std::make_unique<DESKTOP_ENTRY[]>(num_entries);
	auto bytesRead = desktop_file->Read(entries.get(), desk_file_size);

	*entry_count = num_entries;
	
	return std::move(entries);
}


/*
 * afp_CountDesktopItems()
 *
 * Description:
 *		Counts the entries for a particular type in a desktop database.
 *
 * Returns: int32
 */

std::unique_ptr<DESKTOP_ENTRY[]> afp_CountDesktopItems(
	OPEN_DESK_ITEM* deskitem,
	int8 entryType,
	int32* entry_count,
	int32* item_count
	)
{	
	auto entries = afp_GetDesktopEntries(deskitem->file, entry_count);
		
	*item_count = 0;
	int32 count = 0;
	
	for (int i = 0; i < *entry_count; i++)
	{
		if (entries[i].entryType != entryType)
		{
			continue;
		}
		
		count++;
	}
	
	*item_count = count;
	
	DBGWRITE(dbg_level_info, "Item count (%d) count in db: %lu\n", entryType, count);
	
	return std::move(entries);
}


/*
 * afp_FindDTEntry()
 *
 * Description:
 *		Find an entry in the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR afp_FindDTEntry(
	afp_session*	afpSession,
	int16			refnum,
	DESKTOP_ENTRY*	searchCriteria,
	uint16			searchIndex,
	DESKTOP_ENTRY*	foundEntry,
	int32*			foundPosition
	)
{
	DESKTOP_ENTRY*		deskEntry	= NULL;
	OPEN_DESK_ITEM*		deskitem	= NULL;
	AFPERROR			afpError	= AFP_OK;
	ssize_t				position	= 0;
	
	deskitem = afpSession->GetDeskItem(refnum);
	
	if (deskitem == NULL)
	{
		//
		//Big problems if we can't get the desktop file blob.
		//
		return( afpParmErr );
	}
	
	int32 entryCount = 0;
	std::unique_ptr<DESKTOP_ENTRY[]> entries;
	
	int32 iconCount = 0;
	if (searchCriteria->entryType == ENTRY_TYPE_ICON)
	{
		entries = afp_CountDesktopItems(deskitem, ENTRY_TYPE_ICON, &entryCount, &iconCount);
		if (entries == nullptr || searchIndex > iconCount)
		{
			DBGWRITE(dbg_level_warning, "Icon search index out of range (%lu vs %lu)\n", searchIndex, iconCount);
			return afpItemNotFound;
		}
	}
	
	int32 applCount = 0;
	if (searchCriteria->entryType == ENTRY_TYPE_APPL)
	{
		entries = afp_CountDesktopItems(deskitem, ENTRY_TYPE_APPL, &entryCount, &applCount);
		if (entries == nullptr || searchIndex > applCount)
		{
			DBGWRITE(dbg_level_warning, "APPL search index out of range (%lu vs %lu)\n", searchIndex, applCount);
			return afpItemNotFound;
		}
	}
	
	if (entries == nullptr)
	{
		entries = afp_GetDesktopEntries(deskitem->file, &entryCount);
	}
	
	if (entries == nullptr)
	{
		DBGWRITE(dbg_level_warning, "No entries in database!!\n");
		return afpItemNotFound;
	}
	
	// This is not used anymore?
	if (searchCriteria->entryType == ENTRY_TYPE_EMPTY)
	{
		return afpItemNotFound;
	}
	
	DBGWRITE(dbg_level_info, "Looking for desktop entry type %d\n", searchCriteria->entryType);
	
	uint16 index = 0;
	int32 foundIndex = UINT32_MAX;

	for (int32 j = 0; j < entryCount; j++)
	{
		if (searchCriteria->entryType == ENTRY_TYPE_ICON)
		{
			//Data size is 0 if looking for FPGetIconInfo, otherwise it's a FPGetIcon call.
			if (searchCriteria->dataSize != 0)
			{				
				if (searchCriteria->equal_icon(&entries[j]))
				{
					DBGWRITE(dbg_level_info, "Must be FPGetIcon call, search index: %lu\n", searchIndex);
					
					foundIndex = j;
					break;
				}
			}
			
			index++;
			
			if (searchCriteria->fileCreator == entries[j].fileCreator && searchIndex == index)
			{
				DBGWRITE(dbg_level_info, "Found ICON entry!\n");

				foundIndex = j;
				break;
			}
		}
		else if (searchCriteria->entryType == ENTRY_TYPE_APPL)
		{
			index++;
			
			if (searchCriteria->equal_aapl(&entries[j]))
			{
				DBGWRITE(dbg_level_info, "Found APPL entry for path %s\n", entries[j].path);
				
				foundIndex = j;
				break;
			}
		}
		else if (searchCriteria->entryType == ENTRY_TYPE_CMNT)
		{
			//We should never get here. Comments are handled/stored in the file attributes.
		}
	}
	
	if (foundIndex != UINT32_MAX)
	{
		if (foundPosition != nullptr)
		{
			*foundPosition = foundIndex;
		}
		
		memcpy(foundEntry, &entries[foundIndex], sizeof(DESKTOP_ENTRY));
		
		DBGWRITE(dbg_level_info, "Found desktop entry at index %lu:\n", foundIndex);
		DUMP_DT_ENTRY(foundEntry, true);
		
		return AFP_OK;
	}
	
	return( afpItemNotFound );
}


/*
 * afp_AddEntry()
 *
 * Description:
 *		Adds an entry to the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR afp_AddEntry(
	afp_session*	afpSession,
	int16			refnum,
	DESKTOP_ENTRY*	dtEntry
	)
{
	DESKTOP_ENTRY		foundEntry;
	OPEN_DESK_ITEM*		deskitem	= NULL;
	AFPERROR			afpError	= AFP_OK;
	
	std::lock_guard lock(desk_mutex);
	
	deskitem = afpSession->GetDeskItem(refnum);
	
	if (deskitem == NULL)
	{
		//
		//Big problems if we can't get the desktop file blob.
		//
		DBGWRITE(dbg_level_warning, "Failed to get desk item\n");
		return( afpParmErr );
	}
	
	int32 foundPosition = 0;
	afpError = afp_FindDTEntry(
						afpSession,
						refnum,
						dtEntry,
						0,
						&foundEntry,
						&foundPosition
						);
	
	if (AFP_SUCCESS(afpError))
	{
		//
		//We found an entry matching this file and type already
		//in the database. We're supposed to check the bitmap
		//size and only replace it if it's the same.
		//
		if (dtEntry->entryType == ENTRY_TYPE_ICON)
		{
			if (dtEntry->dataSize != foundEntry.dataSize)
			{
				afpError = afpIconTypeError;
			}
		}
		
		if (AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_info, "Replacing entry of type %d in database!\n", dtEntry->entryType);

			std::lock_guard lock(desk_mutex);
			
			int32 entry_count = 0;
			auto entries = afp_GetDesktopEntries(deskitem->file, &entry_count);
			if (entries == nullptr)
			{
				return afpParmErr;
			}
			
			memcpy(&entries[foundPosition], dtEntry, sizeof(DESKTOP_ENTRY));

			deskitem->file->Seek(0, SEEK_SET);			
			auto bytesWritten = deskitem->file->Write(dtEntry, sizeof(DESKTOP_ENTRY));
		}
	}
	else if (afpError == afpItemNotFound)
	{
		std::lock_guard lock(desk_mutex);

		deskitem->file->Seek(0, SEEK_END);
		auto bytesWritten = deskitem->file->Write(dtEntry, sizeof(DESKTOP_ENTRY));
		
		if (bytesWritten < B_OK)
		{
			DBGWRITE(dbg_level_error, "Writing new item of type %d FAILED!\n", dtEntry->entryType);
			afpError = afpParmErr;
		}
		else
		{
			DBGWRITE(dbg_level_info, "Writing new item of type %d succeeded!\n", dtEntry->entryType);
		}
		
		afpError = AFP_OK;
	}
			
	return( afpError );
}


/*
 * afp_RemoveEntry()
 *
 * Description:
 *		Removes an APPL entry from the desktop database.
 *
 * Returns: AFPERROR
 */

AFPERROR afp_RemoveEntry(
	afp_session*	afpSession,
	int16			refnum,
	DESKTOP_ENTRY*	dtEntry
	)
{
	DESKTOP_ENTRY		foundEntry;
	OPEN_DESK_ITEM*		deskitem	= NULL;
	AFPERROR			afpError	= AFP_OK;
	
	DBGWRITE(dbg_level_info, "Removing entry of type %d...\n", dtEntry->entryType);
	
	std::lock_guard lock(desk_mutex);
	
	deskitem = afpSession->GetDeskItem(refnum);
	
	if (deskitem == NULL)
	{
		//
		//Big problems if we can't get the desktop file blob.
		//
		return( afpParmErr );
	}
	
	int32 foundPosition = 0;
	afpError = afp_FindDTEntry(
						afpSession,
						refnum,
						dtEntry,
						0,
						&foundEntry,
						&foundPosition
						);
	
	if (AFP_SUCCESS(afpError))
	{
		std::lock_guard lock(desk_mutex);

		int32 entry_count = 0;
		auto entries = afp_GetDesktopEntries(deskitem->file, &entry_count);
		if (entries == nullptr)
		{
			return afpParmErr;
		}
		
		int32 nextPosition = foundPosition + 1;
		if (nextPosition != entry_count)
		{
			memcpy(&entries[foundPosition], &entries[nextPosition], sizeof(DESKTOP_ENTRY) * (entry_count - nextPosition));
		}
		
		deskitem->file->Seek(0, SEEK_SET);
		auto bytesWritten = deskitem->file->Write(entries.get(), sizeof(DESKTOP_ENTRY) * (entry_count - 1));
		deskitem->file->SetSize((entry_count - 1) * sizeof(DESKTOP_ENTRY));
		
		if (bytesWritten < B_OK)
		{
			DBGWRITE(dbg_level_warning, "Removing entry of type %d FAILED!\n", dtEntry->entryType);
			afpError = afpParmErr;
		}
		else
		{
			DBGWRITE(dbg_level_info, "Removing entry of type %d succeeded!\n", dtEntry->entryType);
		}		
	}
	
	return( afpError );
}


































