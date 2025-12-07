#include "debug.h"
#include "afp.h"
#include "afpreplay.h"
#include "afpvolume.h"
#include "afpaccess.h"
#include "afp_buffer.h"
#include "fp_volume.h"
#include "fp_objects.h"

#define DIR_COUNT_ALLOC_SIZE	25

/*
 * AFPReplayAddReply()
 *
 * Description:
 *		Add a new afp reply to the replay cache.
 *
 * Returns: nothing
 */

void AFPReplayAddReply(
	int16 		dsiRequestID, 
	int8* 		replyBuffer,
	int32		replySize,
	BList*		replayCache
	)
{
	int32 cacheCount 		= replayCache->CountItems();
	AFPReplayCacheItem*	rci = NULL;
		
	//
	//First determine if we need to remove an old item from the cache
	//to make room for the new one.
	//
	if (cacheCount >= AFP_REPLAY_CACHE_SIZE)
	{
		rci = (AFPReplayCacheItem*)replayCache->FirstItem();
		
		if (rci != NULL)
		{
			replayCache->RemoveItem(rci);
			delete rci;
			rci = NULL;
		}
	}
	
	//
	//Create the new replay cache buffer
	//
	rci = new AFPReplayCacheItem;
	
	if (rci != NULL)
	{
		rci->requestID = dsiRequestID;
		rci->replySize = replySize;
		
		memcpy(rci->reply, replyBuffer, replySize);
	}
	
	replayCache->AddItem(rci);
}


/*
 * AFPReplaySearchForReply()
 *
 * Description:
 *		Search through our cache for a reply that has the supplied DSI request ID
 *		and a matching afpCommand byte.
 *
 * Returns: The the reply cache struct containing all the info needed to resend, or NULL
 */

AFPReplayCacheItem* AFPReplaySearchForReply(
	int16	requestID,
	int8	afpCommand,
	BList*	replayCache
	)
{
	AFPReplayCacheItem* item = NULL;
	int32 i = 0;
	
	//
	//Perform a linear search, nothing fancy here, keep the buffer small.
	//
	item = (AFPReplayCacheItem*)replayCache->ItemAt(i);
	
	do
	{
		if (item != NULL)
		{
			if ((item->requestID == requestID) && (item->reply[0] == afpCommand))
			{
				//
				//We found a match, return the reply in the cache.
				//
				return(item);
			}
		}
		
		i++;
		item = (AFPReplayCacheItem*)replayCache->ItemAt(i);
		
	}while(item != NULL);
	
	return(NULL);
}


/*
 * AFPReplayEmptyCache()
 *
 * Description:
 *		Clear out all memory associated with a replay cache for a particular
 *		AFP session.
 *
 * Returns: nothing
 */

void AFPReplayEmptyCache(BList* replayCache)
{
	AFPReplayCacheItem* item = NULL;
	int32 i = 0;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	item = (AFPReplayCacheItem*)replayCache->ItemAt(i);
	
	do
	{
		if (item != NULL)
		{
			DBGWRITE(dbg_level_trace, "Deleting replay entry for request id: %d\n", item->requestID);
			delete item;
		}
		
		i++;
		item = (AFPReplayCacheItem*)replayCache->ItemAt(i);
			
	}while(item != NULL);
}


/*
 * AFPTraverseAndSyncDir()
 *
 * Description:
 *		Traverses a directory and syncs all files to storage.
 *
 * Returns: AFPERROR
 */

AFPERROR AFPTraverseAndSyncDir(BDirectory* rootDir)
{
	BEntry 		entry;
	BList		directories(DIR_COUNT_ALLOC_SIZE);
	BDirectory*	dir = NULL;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	directories.AddItem(rootDir);
	
	do
	{
		dir = (BDirectory*)directories.FirstItem();
		
		if (dir == NULL)
		{
			//
			//This shouldn't happen, but we check anyway just to be safe.
			//	
			break;
		}
		
		dir->Rewind();
		
		while(dir->GetNextEntry(&entry, false) == B_OK)
		{
			if (entry.IsDirectory())
			{
				BDirectory *newDir = new BDirectory(&entry);
				
				//
				//Store the newly found directory in our list so we
				//get to it later.
				//
				directories.AddItem(newDir);
			}
			else
			{
				BNode node(&entry);
									
				#ifdef DEBUG
				BPath p;
				entry.GetPath(&p);
				DBGWRITE(dbg_level_trace, "Syncing file: %s\n", p.Path());
				#endif
				
				node.Sync();
			}
		}
		
		//
		//We don't need the allocated BDirectory anymore, so get
		//rid of it now, it's also how we track if we're done or not
		//
		directories.RemoveItem(dir);
		
		//
		//We don't own the passed in root dir, so we can't delete it.
		//
		if (dir != rootDir) {
			
			delete dir;
		}
		
	}while(directories.CountItems() > 0);
	
	DBGWRITE(dbg_level_trace, "Exit\n");
	
	return( AFP_OK );
}


/*
 * FPSyncDir()
 *
 * Description:
 *		Commits to stable storage all files and directories associated
 *		within the directory specified.
 *
 * Returns: AFPERROR
 */
 
AFPERROR FPSyncDir(
 	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer	afpReply(afpReplyBuffer);
	afp_buffer	afpRequest(afpReqBuffer);
	BEntry		afpEntry;
	fp_volume*	afpVolume		= NULL;
	int16		afpVolumeID		= 0;
	int32		afpDirID		= 0;
	AFPERROR	afpError		= AFP_OK;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	//
	//The first word contains the afp command and the pad byte.
	//
	afpRequest.Advance(sizeof(int16));
	
	//
	//Extract the volID and dirID for the directory to sync
	//
	afpVolumeID		= afpRequest.GetInt16();
	afpDirID		= afpRequest.GetInt32();
	
	//
	//Get a pointer to the volume object and check for validity
	//
	afpVolume = FindVolume(afpVolumeID);
	
	if (afpVolume == NULL)
	{
		//
		//The volume ID is not valid, bail and return error code.
		//
		DBGWRITE(dbg_level_trace, "Volume not found (%d)\n", afpVolumeID);
		return( afpObjectNotFound );
	}

	//
	//Get the file entry that points to the directory we need to flush
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
	}
	else
	{
		//
		//Check to make sure we are allowed to write on the volume and in the dir.
		//
		afpError = afpCheckWriteAccess(afpSession, afpVolume, &afpEntry);
		
		if (!AFP_SUCCESS(afpError))
		{
			DBGWRITE(dbg_level_warning, "User doesn't have write access!\n");
		}
		else if (afpEntry.IsDirectory())
		{
			BDirectory dir(&afpEntry);
						
			if (dir.InitCheck() == B_OK)
			{
				//
				//Traverse through the directory heirarchy rooted with this
				//directory and flush all file/dir content to disk.
				//
				afpError = AFPTraverseAndSyncDir(&dir);
			}
			else
			{
				DBGWRITE(dbg_level_error, "InitCheck() failed on target directory!\n");
				afpError = afpParmErr;
			}
		}
		else //not a directory
		{	
			DBGWRITE(dbg_level_warning, "A non-directory was supplied!\n");
			afpError = afpParmErr;
		}
	}
	
	DBGWRITE(dbg_level_trace, "Returning: %lu\n", afpError);
	
	return( afpError );
}


/*
 * FPSyncFork()
 *
 * Description:
 *		Commits to stable storage all data associated with the
 *		specified file fork.
 *
 * Returns: AFPERROR
 */
 
 AFPERROR FPSyncFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	AFPERROR	afpError = AFP_OK;
	
	DBGWRITE(dbg_level_trace, "Enter\n");
	
	afpError = FPFlushFork(afpSession, afpReqBuffer, afpReplyBuffer, afpDataSize);
	
	DBGWRITE(dbg_level_trace, "Returning: %lu\n", afpError);

	return( afpError );
}


