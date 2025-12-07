#include <Node.h>
#include <Directory.h>
#include <fs_attr.h>
#include <SupportDefs.h>

#include <stdlib.h>
#include <strings.h>

#include "debug.h"
#include "commands.h"
#include "fp_objects.h"
#include "fp_volume.h"
#include "finder_info.h"

#if DEBUG
char errString[24];
#endif


/*
 * SetAFPEntry()
 *
 * Description:
 *		Get the BeEntry for the object pointed to by the AFP parms.
 *
 * Returns: None
 */

AFPERROR fp_objects::SetAFPEntry(
	fp_volume*		afpVolume,
	int32 			afpDirID,
	const char*		afpPathname,
	BEntry& 		afpEntry,
	bool			traverse
	)
{
	BDirectory	directory;
	BEntry 		rootDirEntry;
	BEntry		parentOfRoot;
	status_t	status		= 0;
	BDirectory*	volumeDir	= NULL;
	AFPERROR	afpError 	= AFP_OK;
		
	volumeDir = afpVolume->GetDirectory();
	
	//
	//First, if we're not operating in the root directory, then
	//we need to get a BDirectory for the directory "afpDirID".
	//
	switch(afpDirID)
	{ 
		//
		//In the case of the root and parent dirs, we write some
		//extra code to avoid overhead of initializing more objects
		//than we need to for performance.
		//
		case kRootDirID:
			if ((afpPathname == NULL) || (strlen(afpPathname) == 0))
			{
				status = volumeDir->GetEntry(&afpEntry);

				if (status != B_OK)
				{
					DBGWRITE(dbg_level_warning, "volumeDir->GetEntry(kRootDirID) failed (%s)\n",
							GET_BERR_STR(status)
							);
							
					return( afpObjectNotFound );
				}
				
				return( AFP_OK );
			}
			else
			{
				volumeDir->GetEntry(&rootDirEntry);
				directory.SetTo(&rootDirEntry);
			}
			break;

		case kParentOfRoot:
			if ((afpPathname == NULL) || (strlen(afpPathname) == 0))
			{
				status = volumeDir->GetEntry(&rootDirEntry);

				if (status != B_OK)
				{
					DBGWRITE(dbg_level_warning, "volumeDir->GetEntry(kParentOfRoot) failed (%s)\n",
							GET_BERR_STR(status)
							);
							
					return( afpObjectNotFound );
				}
				
				status = rootDirEntry.GetParent(&afpEntry);

				if (status != B_OK)
				{
					DBGWRITE(dbg_level_warning, "rootDirEntry.GetParent() failed (%s)\n",
							GET_BERR_STR(status)
							);
							
					return( afpObjectNotFound );
				}
				
				return( AFP_OK );
			}
			else
			{
				volumeDir->GetEntry(&rootDirEntry);
				rootDirEntry.GetParent(&parentOfRoot);
				directory.SetTo(&parentOfRoot);
			}
			break;
		
		default:
		{
			node_ref	nodeRef;
			node_ref	searchRef;
					
			volumeDir->GetNodeRef(&nodeRef);
			
			searchRef.device	= nodeRef.device;
			searchRef.node		= afpDirID;
			
			directory.SetTo(&searchRef);
			status = directory.InitCheck();
			
			if (status != B_OK)
			{
				if ((status == B_NOT_A_DIRECTORY) && (afpPathname == NULL) || (strlen(afpPathname) == 0))
				{
					//
					//In this case afpDirID must contain a fileId. Perform a search
					//looking for a file with the supplied id.
					//
					
					AFPERROR error = GetEntryFromFileId(volumeDir, afpDirID, afpEntry);
					
					DBGWRITE(dbg_level_trace, "GetEntryFromFileId() returned (%d)\n", error);
					return( error );
				}
				
				DBGWRITE(dbg_level_error, "BDirectory.InitCheck() failed (%s)\n", GET_BERR_STR(status));
				afpError = afpObjectNotFound;
			}
		}
		break;
	}
	
	if (AFP_SUCCESS(afpError))
	{
		//
		//If we're only looking for the BDirectory for a directory
		//and not another dir/file within that dir, then just return
		//the entry.
		//
		if ((afpPathname == NULL) || (strlen(afpPathname) == 0))
		{						
			status = directory.GetEntry(&afpEntry);
			
			if (status != B_OK)
			{
				DBGWRITE(dbg_level_error, "Getting entry failed (%s)\n", GET_BERR_STR(status));
				return( afpObjectNotFound );
			}
		}
		else
		{
			status = directory.FindEntry(afpPathname, &afpEntry, traverse);
			
			if (status != B_OK)
			{
				//
				//We failed to find the object by name, perhaps the client
				//is looking for a longname?
				//
				if (!AFP_SUCCESS(GetEntryByLongName(directory, afpPathname, afpEntry)))
				{
					DBGWRITE(dbg_level_error, "FindEntry() and GetEntryByLongName() failed for %s (%s)\n",
						afpPathname,
						GET_BERR_STR(status));
						
					return( afpObjectNotFound );
				}
			}
		}
		
		//
		//We have to perform this check because sometimes FindEntry()
		//returns B_OK even though it didn't find the file!
		//
		status = afpEntry.InitCheck();
		
		if ((status != B_OK) || (!afpEntry.Exists()))
		{
			DBGWRITE(dbg_level_error, "InitCheck() or Exists() failed! (%s)\n", GET_BERR_STR(status));

			afpError = afpObjectNotFound;
		}
	}
		
	return( afpError );
}


/*
 * GetEntryFromFileId()
 *
 * Description:
 *		Find an entry by looking for a matching POSIX file id. Unfortunately, there is
 *		no existing mechanism to do this via the Haiku filesystem API.
 *
 * Returns: error code.
 */

AFPERROR fp_objects::GetEntryFromFileId(
	BDirectory* 	volumeDirectory,
	uint32		 	fileId,
	BEntry& 		afpEntry
	)
{
	entry_ref	ref;

	volumeDirectory->Rewind();
	
	while(volumeDirectory->GetNextRef(&ref) == B_OK)
	{
		BEntry tempEntry(&ref);
		
		if (tempEntry.IsDirectory())
		{
			BDirectory tempDirectory(&tempEntry);
			status_t status = tempDirectory.InitCheck();
			
			if (status != B_OK)
			{
				DBGWRITE(dbg_level_error, "BDirectory.InitCheck() failed (%s)\n", GET_BERR_STR(status));
				continue;
			}
			
			AFPERROR result = GetEntryFromFileId(&tempDirectory, fileId, afpEntry);
			if (result == AFP_OK)
			{
				return AFP_OK;
			}
			
			continue;
		}
		
		// Look for a file with the right fileId.
		
		node_ref nodeRef;
		tempEntry.GetNodeRef(&nodeRef);
				
		if (nodeRef.node == fileId)
		{
			afpEntry.SetTo(&ref);
			return AFP_OK;
		}
	}
	
	return( afpObjectNotFound );
}


/*
 * GetEntryByLongName()
 *
 * Description:
 *		Find an entry by looking for a matching longname in the file
 *		attributes.
 *
 * Returns: None
 */

AFPERROR fp_objects::GetEntryByLongName(
	BDirectory& 	dir,
	const char* 	afpPathname,
	BEntry& 		afpEntry
	)
{
	char 	longName[B_FILE_NAME_LENGTH];
	BEntry	temp;

	dir.Rewind();
	
	while(dir.GetNextEntry(&temp) == B_OK)
	{
		BNode	node(&temp);
		ssize_t	sizeRead = 0;
					
		sizeRead = node.ReadAttr(
						AFP_ATTR_LONGNAME,
						B_STRING_TYPE,
						0,
						longName, 
						sizeof(longName));
		
		if (sizeRead != B_ENTRY_NOT_FOUND)
		{
			//
			//We have to add the trailing null byte.
			//
			longName[sizeRead] = 0;
			
			if (!strcmp(afpPathname, longName))
			{
				entry_ref	ref;
				
				//
				//We have a match, return this item's entry.
				//
				temp.GetRef(&ref);
				afpEntry.SetTo(&ref);
									
				return( AFP_OK );
			}
		}
	}
	
	return( afpObjectNotFound );
}


/*
 * CreateLongName()
 *
 * Description:
 *		The name for an object can only be up to 32 characters long for AFP2.2,
 *		therefore, we need to shorten longer names (allowed by AFP3.x and Be)
 *		when it's an AFP2.2 connection (kFPLongNames).
 *
 * Returns: None
 */

void fp_objects::CreateLongName(
	char*		afpPathname,
	BEntry* 	afpEntry,
	bool		afpHardCreate
	)
{
	BDirectory	dir;
	BEntry		temp;
	BNode		node(afpEntry);
	char		afpNewName[B_FILE_NAME_LENGTH];
	char		afpName[MAX_AFP_2_NAME+1];	
	char		numstr[16];
	char		fileExtension[8];
	ssize_t		sizeRead	= 0;
	int			i 			= 0;
	int			nameLen		= 0;
	
	sizeRead = node.ReadAttr(
						AFP_ATTR_LONGNAME,
						B_STRING_TYPE,
						0,
						afpNewName, 
						sizeof(afpNewName)
						);
	
	if ((sizeRead == B_ENTRY_NOT_FOUND) || (afpHardCreate))
	{
		memset(fileExtension, 0, sizeof(fileExtension));
		memset(numstr, 0, sizeof(numstr));
		
		afpEntry->GetName(afpNewName);
		afpEntry->GetParent(&dir);
		
		//
		//If the filename contains a file designator (dot 3 or 4 extension)
		//then we try to preserve this.
		//
		nameLen = strlen(afpNewName);
		
		if (afpNewName[nameLen-4] == '.') {
			strncpy(fileExtension, &afpNewName[nameLen-4], 4);
		}
		else if (afpNewName[nameLen-5] == '.') {
			strncpy(fileExtension, &afpNewName[nameLen-5], 5);
		}
		
		while(true)
		{
			memset(afpName, 0, sizeof(afpName));
			sprintf(numstr, "%d", i);
			
			strncpy(
				afpName,
				afpNewName,
				MAX_AFP_2_NAME - strlen(numstr) - strlen(fileExtension) - 1
				);
						
			strcat(afpName, "~");
			strcat(afpName, numstr);
			strcat(afpName, fileExtension);
						
			if (GetEntryByLongName(dir, afpName, temp) == AFP_OK)
			{
				i++;
			}
			else
			{
				//
				//We're done as an entry with this name doesn't exist.
				//
				break;
			}
		}
				
		node.WriteAttr(
			AFP_ATTR_LONGNAME,
			B_STRING_TYPE,
			0,
			afpName, 
			strlen(afpName)
			);

		//
		//Only copy the new name into the buffer if provided.
		//
		if (afpPathname != NULL) {
			strcpy(afpPathname, afpName);
		}
	}
	else
	{
		//
		//We have to add the trailing (null) byte if the read
		//was successfull.
		//
		afpNewName[sizeRead] = 0;

		//
		//Only copy the new name into the buffer if provided.
		//
		if (afpPathname != NULL) {
			strcpy(afpPathname, afpNewName);
		}
	}
	
	DBGWRITE(dbg_level_trace, "New longname = %s\n", afpName);
}


/*
 * fp_GetDirParms()
 *
 * Description:
 *		Get the directory parameters for the FPGetDirParms call.
 *
 * Returns: None
 */

AFPERROR fp_objects::fp_GetDirParms(
	afp_session*	afpSession,
	fp_volume*		afpVolume,
	BEntry* 		afpEntry,
	int16			afpDirBitmap,
	afp_buffer* 	afpReply
	)
{
	BDirectory	dir;
	node_ref	ref;
	char		name[B_FILE_NAME_LENGTH];
	int16*		nameOffset		= NULL;
	int16*		uniNameOffset	= NULL;
	int8*		parmsStart		= afpReply->GetCurrentPosPtr();
	time_t		ctime			= 0;
	
	if (afpDirBitmap & kFPDirAttribute)
	{
		int16	afpAttributes = 0;
		
		if (!AFP_SUCCESS(GetAFPAttributes(afpEntry, &afpAttributes)))
		{
			//
			//If there is currently no attribute stream, create it.
			//
			SetAFPAttributes(afpEntry, &afpAttributes);
		}

		afpReply->AddInt16(afpAttributes);
	}

	if (afpDirBitmap & kFPDirParentID)
	{
		//
		//Check for the special case directories as we need to make adjustments
		//to the ID's we pass back for them.
		//
		dir.SetTo(afpEntry);
		dir.GetNodeRef(&ref);
		
		if (ref.node == afpVolume->GetRootDirID())
		{
			afpReply->AddInt32(kParentOfRoot);
		}
		else if (ref.node == afpVolume->GetParentOfRootID())
		{
			afpReply->AddInt32(kNoParent);
		}
		else
		{
			dir.Unset();
			
			afpEntry->GetParent(&dir);
			
			if (dir.InitCheck() == B_OK)
			{
				dir.GetNodeRef(&ref);
				
				//
				//We check to see if the client is indeed looking at
				//one of the pre-defined constant dirs (kRootDirID &
				//kParentOfRoot).
				//
				if (ref.node == afpVolume->GetRootDirID())
				{	
					ref.node = kRootDirID;
				}
				
				afpReply->AddInt32(ref.node);
			}
			else
			{
				DBGWRITE(dbg_level_warning, "Failed getting parent\n");
				return( afpParmErr );
			}
		}
		
		dir.Unset();
	}
	
	if (afpDirBitmap & kFPDirCreateDate)
	{
		afpEntry->GetCreationTime(&ctime);		
		afpReply->AddInt32(TO_AFP_TIME(ctime));
	}

	if (afpDirBitmap & kFPDirModDate)
	{
		afpEntry->GetModificationTime(&ctime);
		afpReply->AddInt32(TO_AFP_TIME(ctime));
	}

	if (afpDirBitmap & kFPDirBackupDate)
	{
		afpReply->AddInt32(0x80000000);
	}

	if (afpDirBitmap & kFPDirFinderInfo)
	{
		FINDER_INFO	finfo;
		
		memset(&finfo, 0, sizeof(finfo));

		if (!AFP_SUCCESS(GetAFPFinderInfo(afpEntry, &finfo)))
		{
			//
			//If the file info stream didn't exist, then we create a
			//blank unitialized stream.
			//			
			finfo.fdLocation.x	= 20;
			finfo.fdLocation.y	= 20;
			
			SetAFPFinderInfo(afpEntry, &finfo);
		}
		
		finfo.fdFlags		= htons(finfo.fdFlags);
		finfo.fdLocation.x	= htons(finfo.fdLocation.x);
		finfo.fdLocation.y 	= htons(finfo.fdLocation.y);
		finfo.fdFldr		= htons(finfo.fdFldr);
		
		afpReply->AddRawData(&finfo, sizeof(finfo));
	}

	if (afpDirBitmap & kFPDirLongName)
	{
		nameOffset = (int16*)afpReply->GetCurrentPosPtr();
		afpReply->Advance(sizeof(int16));
	}

	if (afpDirBitmap & kFPDirID)
	{
		if (afpEntry->GetNodeRef(&ref) == B_OK)
		{
			//
			//We check to see if the client is indeed looking at
			//one of the pre-defined constant dirs (kRootDirID &
			//kParentOfRoot).
			//
			if (ref.node == afpVolume->GetRootDirID())
			{	
				ref.node = kRootDirID;
			}
			else if (ref.node == afpVolume->GetParentOfRootID())
			{
				ref.node = kParentOfRoot;
			}
			else if (ref.node > ULONG_MAX)
			{
				//
				//AFP cannot handle node id's greater than a 4 byte
				//number. We have no choice but to bail here.
				//
				DBGWRITE(dbg_level_warning, "ref.node too big, failing...\n");
				return( afpParmErr );
			}

			afpReply->AddInt32(ref.node);
		}
		else
		{
			DBGWRITE(dbg_level_warning, "Failed to get dirID\n");
			return( afpParmErr );
		}
	}
	
	if (afpDirBitmap & kFPDirOffCount)
	{
		dir.SetTo(afpEntry);
		
		if (dir.InitCheck() == B_OK)
		{
			afpReply->AddInt16(dir.CountEntries());
		}
		else
		{
			DBGWRITE(dbg_level_warning, "Failed to get dir offspring\n");
			return( afpParmErr );
		}
	}
	
	if (afpDirBitmap & kFPDirOwnerID)
	{
		AFP_USER_DATA	userData;
		int16			index = 0;
		uid_t			owner = 0;
		
		if (afpSession->IsAdmin())
		{
			//
			//If the this session belongs to an admin, then return
			//this admin as the owner of the directory.
			//
			
			afpSession->GetUserInfo(&userData);
			
			owner = userData.id;
		}
		else
		{
			//
			//Find the first admin in the list and set that as the owner.
			//
			while(afpGetIndUser(&userData, index) == B_OK)
			{
				if (userData.flags & kIsAdmin)
				{
					owner = userData.id;
					break;
				}
				
				index++;
			}
		}
		
		afpReply->push_num<uint32>(owner);
	}
	
	if (afpDirBitmap & kFPDirGroupID)
	{
		//
		//A group ID of zero means the dir is not associated with a group.
		//Group permissions will be ignored for this directory.
		//
		uid_t group = 0;
		
		afpReply->push_num<uint32>(group);
	}
	
	if (afpDirBitmap & kFPDirAccess)
	{
		int32	afpPerms 	= 0;
		int16	userType	= 0;
		mode_t	posixPerms	= 0;
		
		dir.SetTo(afpEntry);
		dir.GetPermissions(&posixPerms);
		
		if (afpSession->IsAdmin())
			userType = kUserType_Owner;
		else if (afpSession->IsGuest())
			userType = kUserType_Guest;
		else
			userType = kUserType_User;
		
		afpPerms = BtoAFPPermissions(userType, posixPerms);
		
		afpReply->AddInt32(afpPerms);
	}
	
	if (afpDirBitmap & kFPUnicodeName)
	{
		uniNameOffset = (int16*)afpReply->GetCurrentPosPtr();
		afpReply->Advance(sizeof(int16));
	}

	if (afpDirBitmap & kFPDirLongName)
	{
		if (afpEntry->GetName(name) == B_OK)
		{
			//
			//Longnames can only be up to 32 characters long. Truncate
			//the string if necessary.
			//
			if (strlen(name) > MAX_AFP_2_NAME)
			{
				CreateLongName(name, afpEntry);
			}
			
			*nameOffset = htons(afpReply->GetCurrentPosPtr() - parmsStart);
			afpReply->AddCStringAsPascal(name);
		}
	}
	
	if (afpDirBitmap & kFPUnicodeName)
	{
		if (afpEntry->GetName(name) == B_OK)
		{
			*uniNameOffset = htons(afpReply->GetCurrentPosPtr() - parmsStart);
			afpReply->AddUniString(name, true);
		}
	}

	return( AFP_OK );
}


/*
 * fp_GetFileParms()
 *
 * Description:
 *		Get the file parameters for the FPGetDirParms call.
 *
 * Returns: None
 */

AFPERROR fp_objects::fp_GetFileParms(
	afp_session*	afpSession,
	fp_volume*		afpVolume,
	BEntry* 		afpEntry,
	int16			afpFileBitmap,
	afp_buffer* 	afpReply
	)
{
	BDirectory		dir;
	node_ref		ref;
	time_t			ctime;
	char			name[B_FILE_NAME_LENGTH];
	uint16			afpForkRef		= 0;
	bool			afpRsrcOpen		= false;
	attr_info		info			= {0,0};
	int16*			longNameOffset 	= NULL;
	int16*			uniNameOffset	= NULL;
	int8*			parmsStart		= afpReply->GetCurrentPosPtr();
	OPEN_FORK_ITEM*	forkItem		= NULL;
	
	if (afpFileBitmap & kFPFileAttributes)
	{	
		int16	afpAttributes = 0;
		
		if (!AFP_SUCCESS(GetAFPAttributes(afpEntry, &afpAttributes)))
		{
			//
			//If there is currently no attribute stream, create it.
			//
			SetAFPAttributes(afpEntry, &afpAttributes);
		}
		
		//
		//We need to dynamically check and set the bits that tell
		//whether a data or resource fork is already open.
		//
		if (afpVolume->IsFileOpen(afpEntry, kDataFork)) {
			afpAttributes |= kFileDataForkOpen;
		}
		else {
			afpAttributes &= ~kFileDataForkOpen;
		}
		
		if (afpVolume->IsFileOpen(afpEntry, kResourceForkBit, &afpForkRef))
		{
			afpAttributes 	|= kFileRsrcForkOpen;
			
			//
			//If the resource fork is open, we get the ref num and get a
			//reference to its forkItem so we can get the right fork length.
			//
			forkItem 		= afpSession->GetForkItem(afpForkRef);
			afpRsrcOpen		= true;
		}
		else {
			afpAttributes &= ~kFileRsrcForkOpen;
		}
		
		afpReply->push_num(afpAttributes);
	}
	
	if (afpFileBitmap & kFPParentID)
	{
		afpEntry->GetParent(&dir);
		
		if (dir.InitCheck() == B_OK)
		{
			dir.GetNodeRef(&ref);
				
			//
			//We check to see if the client is indeed looking at
			//one of the pre-defined constant dirs (kRootDirID &
			//kParentOfRoot).
			//
			if (ref.node == afpVolume->GetRootDirID())
			{
				afpReply->push_num<uint32>(kRootDirID);
			}
			else if (ref.node == afpVolume->GetParentOfRootID())
			{
				afpReply->push_num<uint32>(kParentOfRoot);
			}
			else
			{
				if (ref.node > UINT32_MAX)
				{
					DBGWRITE(dbg_level_error, "Parent dir ID greater than UINT32_MAX!!!\n");
				}
				
				afpReply->push_num<uint32>(ref.node);
			}
						
			dir.Unset();
		}
		else
		{
			DBGWRITE(dbg_level_error, "Failed getting parent\n");
			return( afpParmErr );
		}
	}
	
	if (afpFileBitmap & kFPCreateDate)
	{
		afpEntry->GetCreationTime(&ctime);		
		afpReply->push_num<uint32>(TO_AFP_TIME(ctime));
	}
	
	if (afpFileBitmap & kFPModDate)
	{
		afpEntry->GetModificationTime(&ctime);
		afpReply->push_num<uint32>(TO_AFP_TIME(ctime));
	}
	
	if (afpFileBitmap & kFPBackupDate) {
		
		afpReply->push_num<uint32>(0x80000000);
	}
	
	if (afpFileBitmap & kFPFinderInfo)
	{
		FINDER_INFO	finfo;
		
		GetAFPFinderInfo(afpEntry, &finfo);
		
		finfo.fdFlags		= htons(finfo.fdFlags);
		finfo.fdLocation.x	= htons(finfo.fdLocation.x);
		finfo.fdLocation.y 	= htons(finfo.fdLocation.y);
		finfo.fdFldr		= htons(finfo.fdFldr);
		
		afpReply->AddRawData(&finfo, sizeof(finfo));
	}
	
	if (afpFileBitmap & kFPLongName)
	{
		longNameOffset = (int16*)afpReply->GetCurrentPosPtr();
		afpReply->Advance(sizeof(int16));
	}
	
	if (afpFileBitmap & kFPFileNum)
	{
		if (afpEntry->GetNodeRef(&ref) == B_OK)
		{
			if (ref.node > ULONG_MAX)
			{
				//
				//AFP cannot handle node id's greater than a 4 byte
				//number. We have no choice but to bail here.
				//
				DBGWRITE(dbg_level_error, "****Node ref > 4GB !!");
				return( afpParmErr );
			}
			
			afpReply->push_num<uint32>(ref.node);
		}
		else
		{
			DBGWRITE(dbg_level_error, "Failed to get fileID");
			return( afpParmErr );
		}
	}
	
	if (afpFileBitmap & kFPDFLen)
	{
		off_t fsize = 0;
				
		afpEntry->GetSize(&fsize);
		
		//
		//AFP 2.2 can only handle file sizes of 4GB
		//
		if (fsize > ULONG_MAX) {
		
			fsize = ULONG_MAX;
		}
		
		afpReply->push_num<uint32>(fsize);
	}
	
	if (afpFileBitmap & kFPRFLen)
	{
		off_t fsize = 0;
		
		//
		//Since we always keep the resource fork in memory for perf reasons,
		//we need to get the real size of the fork if it's open by accessing
		//the memory I/O buffer directly.
		//
		if (afpRsrcOpen)
		{
			fsize = forkItem->rsrcIO->BufferLength();
		}
		else
		{
			BNode	node(afpEntry);
					
			if (node.GetAttrInfo(AFP_RSRC_ATTRIBUTE, &info) == B_OK) {
				fsize = info.size;
			}
		}
				
		//
		//AFP 2.2 can only handle file sizes of 4GB.
		//
		if (fsize > ULONG_MAX) {
		
			fsize = ULONG_MAX;
		}

		afpReply->push_num<uint32>(fsize);
	}
	
	if (afpFileBitmap & kFPExtDataForkLen)
	{
		off_t fsize = 0;
				
		afpEntry->GetSize(&fsize);
		afpReply->push_num(fsize);
	}
		
	if (afpFileBitmap & kFPUnicodeName)
	{
		uniNameOffset = (int16*)afpReply->GetCurrentPosPtr();
		afpReply->Advance(sizeof(int16));
	}
		
	if (afpFileBitmap & kFPExtRsrcForkLen)
	{
		off_t fsize = 0;
		
		//
		//See kFPRFLen for details on why we do this.
		//
		if (afpRsrcOpen)
		{
			fsize = forkItem->rsrcIO->BufferLength();
		}
		else
		{
			BNode	node(afpEntry);
					
			if (node.GetAttrInfo(AFP_RSRC_ATTRIBUTE, &info) == B_OK) {
				fsize = info.size;
			}
		}
		
		//
		//I don't know what the client is doing here, but it needs an
		//extra 4 bytes inserted here to read the resource fork length
		//correctly. Is it possible it's looking for the launch limit? The
		//spec isn't clear.
		//
		afpReply->push_num<uint32>(0);
		
		afpReply->push_num(fsize);
	}
	
	//*****************************************************************
	//Stuff in the variable length information at the end of the block.
	//*****************************************************************
	
	if (afpFileBitmap & kFPLongName)
	{	
		if (afpEntry->GetName(name) == B_OK)
		{
			//
			//Longnames can only be up to 32 characters long. Truncate
			//the string if necessary.
			//
			if (strlen(name) > MAX_AFP_2_NAME)
			{
				CreateLongName(name, afpEntry);
			}
						
			*longNameOffset = htons(afpReply->GetCurrentPosPtr() - parmsStart);
			afpReply->AddCStringAsPascal(name);
		}
	}
	
	if (afpFileBitmap & kFPUnicodeName)
	{
		if (afpEntry->GetName(name) == B_OK)
		{
			*uniNameOffset = htons(afpReply->GetCurrentPosPtr() - parmsStart);
			afpReply->AddUniString(name, true);
		}
	}
	
	return( AFP_OK );
}


/*
 * fp_SetFileDirParms()
 *
 * Description:
 *		Get the file parameters for the FPGetDirParms call.
 *
 * Returns: None
 */

AFPERROR fp_objects::fp_SetFileDirParms(
	afp_session*	afpSession,
	afp_buffer*		afpRequest,
	BEntry* 		afpEntry,
	int16			afpBitmap
	)
{
	time_t		afpModTime		= 0;
	time_t		afpCreateTime	= 0;
	time_t		afpBackTime		= 0;
	int16		afpAttributes	= 0;
	uint8		setClearFlag	= 0;
	AFPERROR	afpError		= AFP_OK;
	
	//
	//See if we're adjusting the file/dir attributes (AFP attributes).
	//
	if (afpBitmap & kFPFileAttributes)
	{
		//
		//Grab the attributes from the attr stream of the file or dir.
		//
		afpError = GetAFPAttributes(afpEntry, &afpAttributes);
		
		if (AFP_SUCCESS(afpError))
		{
			int16	afpNewAttributes = 0;
			
			setClearFlag &= afpEntry->IsDirectory() ? kFileSetClearFlag : kFileSetClearFlag;
			afpNewAttributes = afpRequest->GetInt16();
			
			if (afpNewAttributes & setClearFlag)
			{
				if (afpNewAttributes & kFileInvisible)		afpAttributes |= kFileInvisible;
				if (afpNewAttributes & kFileSystem)			afpAttributes |= kFileSystem;
				if (afpNewAttributes & kFileWriteInhibit)	afpAttributes |= kFileWriteInhibit;
				if (afpNewAttributes & kFileRenameInhibit)	afpAttributes |= kFileRenameInhibit;
				if (afpNewAttributes & kFileDeleteInhibit)	afpAttributes |= kFileDeleteInhibit;
				if (afpNewAttributes & kFileNeedsBackup)	afpAttributes |= kFileNeedsBackup;
			}
			else
			{
				if (afpNewAttributes & kFileInvisible)		afpAttributes &= ~kFileInvisible;
				if (afpNewAttributes & kFileSystem)			afpAttributes &= ~kFileSystem;
				if (afpNewAttributes & kFileWriteInhibit)	afpAttributes &= ~kFileWriteInhibit;
				if (afpNewAttributes & kFileRenameInhibit)	afpAttributes &= ~kFileRenameInhibit;
				if (afpNewAttributes & kFileDeleteInhibit)	afpAttributes &= ~kFileDeleteInhibit;
				if (afpNewAttributes & kFileNeedsBackup)	afpAttributes &= ~kFileNeedsBackup;
			}
			
			afpError = SetAFPAttributes(afpEntry, &afpAttributes);
			
			if (!AFP_SUCCESS(afpError))
			{
				DBGWRITE(dbg_level_error, "Failed to save attributes! (%lu)\n", afpError);
			}
		}
		else
		{
			DBGWRITE(dbg_level_error, "Failed to get attributes! (%lu)\n", afpError);
		}
	}
	
	//
	//Extract the times if we're setting any now. We'll wait till
	//later to actually set them incase anything we do resets the
	//mod time.
	//
	if (afpBitmap & kFPCreateDate)	afpCreateTime 	= afpRequest->GetInt32();
	if (afpBitmap & kFPModDate)		afpModTime 		= afpRequest->GetInt32();
	if (afpBitmap & kFPBackupDate)	afpBackTime 	= afpRequest->GetInt32();
	
	if (afpBitmap & kFPFinderInfo)
	{
		FINDER_INFO	finfo;
		
		afpRequest->GetRawData(&finfo, sizeof(FINDER_INFO));
		
		finfo.fdFlags		= ntohs(finfo.fdFlags);
		finfo.fdLocation.x	= ntohs(finfo.fdLocation.x);
		finfo.fdLocation.y	= ntohs(finfo.fdLocation.y);
		finfo.fdFldr		= ntohs(finfo.fdFldr);
		
		afpError = SetAFPFinderInfo(afpEntry, &finfo);
		
		if (!AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_error, "Failed to set finder info!\n");
		}
	}
	
	if (afpEntry->IsDirectory())
	{
		if (afpBitmap & kFPDirOwnerID)
		{			
			auto owner = afpRequest->pull_num<uint32>();
			
			//Not supported by Haiku-OS
			//afpEntry->SetOwner(owner);
		}
		
		if (afpBitmap & kFPDirGroupID)
		{			
			auto group = afpRequest->pull_num<uint32>();

			//Not supported by Haiku-OS
			//afpEntry->SetGroup(group);
		}
		
		if ((afpBitmap & kFPDirAccess) && (afpSession->IsAdmin()))
		{
			int32	afpPerms 	= 0;
			
			afpPerms = afpRequest->GetInt32();
			afpEntry->SetPermissions(AFPtoBPermissions(afpPerms));
		}
	}

	//
	//Finally, set the correct times for the file/dir.
	//
	if (afpBitmap & kFPCreateDate)	afpEntry->SetCreationTime(FROM_AFP_TIME(afpCreateTime));
	if (afpBitmap & kFPModDate)		afpEntry->SetModificationTime(FROM_AFP_TIME(afpModTime));
	
	if (afpBitmap & kFPBackupDate)
	{
		//
		//Haiku-OS doesn't support a backup time.
		//
	}
	
	return( AFP_OK );
}


/*
 * AFPtoBPermissions()
 *
 * Description:
 *		Converts AFP to be permissions.
 *
 * Returns: None
 */

mode_t fp_objects::AFPtoBPermissions(
	int32			afpPerms
	)
{
	mode_t	b_perms = 0;
		
	if (afpPerms & kAccess_OS)	b_perms |= POSIX_AFP_ISOWNER;
	if (afpPerms & kAccess_OR)	b_perms |= POSIX_AFP_IROWNER;
	if (afpPerms & kAccess_OW)	b_perms |= POSIX_AFP_IWOWNER;
	
	if (afpPerms & kAccess_GS)	b_perms |= POSIX_AFP_ISUSER;
	if (afpPerms & kAccess_GR)	b_perms |= POSIX_AFP_IRUSER;
	if (afpPerms & kAccess_GW)	b_perms |= POSIX_AFP_IWUSER;
	
	if (afpPerms & kAccess_WS)	b_perms |= POSIX_AFP_ISGUEST;
	if (afpPerms & kAccess_WR)	b_perms |= POSIX_AFP_IRGUEST;
	if (afpPerms & kAccess_WW)	b_perms |= POSIX_AFP_IWGUEST;
	
	return( b_perms );
}


/*
 * BtoAFPPermissions()
 *
 * Description:
 *		Converts be to afp permissions.
 *
 * Returns: uint32 containing access bits.
 */

uint32 fp_objects::BtoAFPPermissions(
	int16			userType,
	mode_t 			posixPerms
	)
{
	uint32	afpPerms = 0;

	if (posixPerms & POSIX_AFP_IRGUEST)	afpPerms |= kAccess_WR;
	if (posixPerms & POSIX_AFP_IWGUEST)	afpPerms |= kAccess_WW;
	if (posixPerms & POSIX_AFP_ISGUEST)	afpPerms |= kAccess_WS;
	
	if (posixPerms & POSIX_AFP_IROWNER)	afpPerms |= kAccess_OR;
	if (posixPerms & POSIX_AFP_IWOWNER)	afpPerms |= kAccess_OW;
	if (posixPerms & POSIX_AFP_ISOWNER)	afpPerms |= kAccess_OS;
	
	//if (posixPerms & POSIX_AFP_IRUSER)	afpPerms |= kAccess_GR;
	//if (posixPerms & POSIX_AFP_IWUSER)	afpPerms |= kAccess_GW;
	//if (posixPerms & POSIX_AFP_ISUSER)	afpPerms |= kAccess_GS;
	
	//
	//User Access Rights are the rights of the logged on user
	//we're currently getting permissions for.
	//
	
	if (userType == kUserType_Guest)
	{
		if (posixPerms & POSIX_AFP_IRGUEST)	afpPerms |= kAccess_UAR;
		if (posixPerms & POSIX_AFP_IWGUEST)	afpPerms |= kAccess_UAW;
		if (posixPerms & POSIX_AFP_ISGUEST)	afpPerms |= kAccess_UAS;
	}
	else if (userType == kUserType_User)
	{
		if (posixPerms & POSIX_AFP_IRUSER)	afpPerms |= kAccess_UAR;
		if (posixPerms & POSIX_AFP_IWUSER)	afpPerms |= kAccess_UAW;
		if (posixPerms & POSIX_AFP_ISUSER)	afpPerms |= kAccess_UAS;
	}
	else if (userType == kUserType_Owner)
	{
		if (posixPerms & POSIX_AFP_IROWNER)	afpPerms |= kAccess_UAR;
		if (posixPerms & POSIX_AFP_IWOWNER)	afpPerms |= kAccess_UAW;
		if (posixPerms & POSIX_AFP_ISOWNER)	afpPerms |= kAccess_UAS;

		afpPerms |= kAccess_Owner;
	}
	
	return( afpPerms );
}


/*
 * GetAFPFinderInfo()
 *
 * Description:
 *		Retrieve the file info from the file's attribute stream.
 *
 * Returns: None
 */

AFPERROR fp_objects::GetAFPFinderInfo(
	BEntry* 		afpEntry,
	FINDER_INFO*	afpFInfo
	)
{
	BNode		node(afpEntry);
	char		name[B_FILE_NAME_LENGTH];
	ssize_t		bytesRead;
	
	//
	//The object had better exit at this point or we really
	//messed up!
	//
	if (node.InitCheck() != B_OK) 
	{
		DBGWRITE(dbg_level_error, "InitCheck() failed!\n");
		return( afpObjectNotFound );
	}
	
	//
	//Read in the FInfo structure from the attribute stream.
	//
	bytesRead = node.ReadAttr(
					AFP_FINFO_ATTRIBUTE,
					B_RAW_TYPE,
					0,
					afpFInfo,
					sizeof(FINDER_INFO)
					);
	
	if ((bytesRead < B_OK) || (bytesRead != sizeof(FINDER_INFO)) || memcmp(afpFInfo->fdType, "????", 4) == 0)
	{
		if (bytesRead == B_ENTRY_NOT_FOUND)
		{
			FINDER_INFO	finfo;
			
			afpEntry->GetName(name);
			FinderInfoBasedOnExtension(name, &finfo);
			
			// There is currently no attribute stream for this
			// file. Create one.
			SetAFPFinderInfo(afpEntry, &finfo);
			
			// Copy the new info which will basically clear everything
			// in the return value.
			memcpy(afpFInfo, &finfo, sizeof(finfo));
		}

		//
		//We failed to read in the data (doesn't exist).
		//
		DBGWRITE(dbg_level_trace, "Failed to read FInfo (%d)\n", bytesRead);
		return( afpObjectNotFound );
	}
	
	return( AFP_OK );
}


/*
 * SetAFPFinderInfo()
 *
 * Description:
 *		Set the file info in the file's attribute stream.
 *
 * Returns: None
 */

AFPERROR fp_objects::SetAFPFinderInfo(
	BEntry* 		afpEntry,
	FINDER_INFO*	afpFInfo
	)
{
	BNode		node(afpEntry);
	ssize_t		bytesWritten;
	
	//
	//The object had better exit at this point or we really
	//messed up!
	//
	if (node.InitCheck() != B_OK) {

		return( afpObjectNotFound );
	}

	bytesWritten = node.WriteAttr(
					AFP_FINFO_ATTRIBUTE,
					B_RAW_TYPE,
					0,
					afpFInfo,
					sizeof(FINDER_INFO)
					);

	if ((bytesWritten < B_OK) || (bytesWritten != sizeof(FINDER_INFO)))
	{
		//
		//We failed to read in the data (doesn't exist).
		//
		DBGWRITE(dbg_level_error, "Failed to write FInfo (%lu)\n", bytesWritten);
		return( afpMiscErr );
	}

	return( AFP_OK );
}


/*
 * GetAFPAttributes()
 *
 * Description:
 *		Retrieve the AFP attributes from the file/dir.
 *
 * Returns: None
 */

AFPERROR fp_objects::GetAFPAttributes(
	BEntry* 		afpEntry,
	int16*			afpAttributes
	)
{
	BNode		node(afpEntry);
	ssize_t		bytesRead;
	
	//
	//The object had better exit at this point or we really
	//messed up!
	//
	if (node.InitCheck() != B_OK) {
		
		return( afpObjectNotFound );
	}
	
	//
	//Read in the attributes from the attribute stream.
	//
	bytesRead = node.ReadAttr(
					AFP_ATTR_ATTRIBUTE,
					B_INT16_TYPE,
					0,
					afpAttributes,
					sizeof(int16)
					);
	
	if ((bytesRead < B_OK) || (bytesRead != sizeof(int16)))
	{
		if (bytesRead == B_ENTRY_NOT_FOUND)
		{
			int16 newAttributes = 0;
			
			//
			//There is currently no attribute stream for this
			//file. Create one.
			//
			SetAFPAttributes(afpEntry, &newAttributes);
			
			//
			//Return 0 for no attributes set.
			//
			*afpAttributes = 0;
		}			
		else
		{
			//
			//We failed to read in the data (doesn't exist).
			//
			DBGWRITE(dbg_level_error, "Failed to read attributes (%lu)\n", bytesRead);
			return( afpObjectNotFound );
		}
	}
	
	return( AFP_OK );
}


/*
 * SetAFPAttributes()
 *
 * Description:
 *		Set the file/dir attributes in the attribute stream.
 *
 * Returns: None
 */

AFPERROR fp_objects::SetAFPAttributes(
	BEntry* 		afpEntry,
	int16*			afpAttributes
	)
{
	BNode		node(afpEntry);
	ssize_t		bytesWritten;
	
	//
	//The object had better exit at this point or we really
	//messed up!
	//
	if (node.InitCheck() != B_OK) {
		
		return( afpObjectNotFound );
	}

	bytesWritten = node.WriteAttr(
					AFP_ATTR_ATTRIBUTE,
					B_INT16_TYPE,
					0,
					afpAttributes,
					sizeof(int16)
					);

	if ((bytesWritten < B_OK) || (bytesWritten != sizeof(int16)))
	{
		//
		//We failed to read in the data (doesn't exist).
		//
		DBGWRITE(dbg_level_error, "Failed to write attributes (%lu)\n", bytesWritten);
		return( afpMiscErr );
	}

	return( AFP_OK );
}


/*
 * CopyAttrs()
 *
 * Description:
 *		Copy all the attributes from one file to another.
 *
 * Returns: None
 */

status_t fp_objects::CopyAttrs(BNode& inFrom, BNode& inTo)
{
	char			attrname[64];
	status_t		err = 0;
	char*			buffer = NULL;
	off_t			bufferSize = 0;
	const off_t		kMinBufferSize = 1024;
	
	inFrom.RewindAttrs();
	inTo.RewindAttrs();
	
	while (B_NO_ERROR == inFrom.GetNextAttrName(attrname))
	{
		attr_info		fromInfo;
		attr_info		toInfo;
		ssize_t			size;

		if (!strcmp(attrname, AFP_ATTR_LONGNAME))
		{
			//
			//We don't want to copy the longname file attribute as
			//we'll need to create a new one and we don't want dupes.
			//
			continue;
		}
		
		err = inFrom.GetAttrInfo(attrname, &fromInfo);
		
		if (err == B_NO_ERROR)
		{
			if (buffer == NULL || fromInfo.size > bufferSize)
			{
				bufferSize = max_c(fromInfo.size, kMinBufferSize);
				delete[] buffer;
				buffer = NULL;
				buffer = new char[bufferSize];
				ASSERT(buffer != NULL);
			}
			
			ssize_t		sizeRead;
			
			sizeRead = inFrom.ReadAttr(attrname, fromInfo.type, 0, buffer, fromInfo.size);

			err = inTo.GetAttrInfo(attrname, &toInfo);
			// Attribute already exists
			if (err == B_NO_ERROR && fromInfo.size > toInfo.size)
				err = inTo.RemoveAttr(attrname);
			if (err == B_NO_ERROR || err == ENOENT)	// ENOENT means entry doesn't exist
			{
				// Attribute doesn't already exist
				size = inTo.WriteAttr(attrname, fromInfo.type, 0, buffer, fromInfo.size);
				err = size != fromInfo.size ? B_ERROR : B_NO_ERROR;		
			}
		}
	}

	delete[] buffer;
	
	return err;
}


/*
 * CopyFile()
 *
 * Description:
 *		Copy all the data from one file to another.
 *
 * Returns: None
 */

status_t fp_objects::CopyFile(BFile& inFrom, BFile& inTo)
{
	const size_t 	chunkSize = 64 * 1024;
	char*			buffer = new char[chunkSize];
	ssize_t			readSize = 0;
	status_t		err = B_OK;

	while ((readSize = inFrom.Read(buffer, chunkSize)) > 0)
	{
		inTo.Write(buffer, readSize);
	}

	delete [] buffer;
	
	err = CopyAttrs(inFrom, inTo);

	return err;
}




	



















