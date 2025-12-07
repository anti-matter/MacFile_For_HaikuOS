#include <Entry.h>
#include <Volume.h>
#include <Directory.h>
#include <netinet/in.h>

#include "debug.h"
#include "afp.h"
#include "fp_volume.h"

int16 gNextVolumeID = 1;

/*
 * fp_volume()
 *
 * Description:
 *
 * Returns:
 */

fp_volume::fp_volume(BPath* path, uint32 srvrVolFlags)
{
	BDirectory		dir;

	mVolumeID 		= gNextVolumeID++;
	mVolumeFlags	= srvrVolFlags;
	mPath			= path;
	mRootDirID		= 0;
	mParentOfRootID	= 0;
	mDirectory 		= new BDirectory(path->Path());
	mIsDirty		= false;

	//
	//Get the root and parent of root node id's so we can
	//find them later.
	//
	if (mDirectory->InitCheck() == B_OK)
	{
		BEntry		parentOfRoot;
		BEntry		root;
		node_ref	nodeRef;

		mDirectory->GetNodeRef(&nodeRef);

		mRootDirID = nodeRef.node;

		mDirectory->GetEntry(&root);
		root.GetParent(&parentOfRoot);

		if (parentOfRoot.InitCheck() == B_OK)
		{
			parentOfRoot.GetNodeRef(&nodeRef);
			mParentOfRootID = nodeRef.node;
		}
	}

	mOpenFiles = new BList();
}


/*
 * ~fp_volume()
 *
 * Description:
 *
 * Returns:
 */

fp_volume::~fp_volume()
{
	delete mPath;
	delete mDirectory;
	delete mOpenFiles;
}


/*
 * AddOpenFile()
 *
 * Description:
 *
 * Returns:
 */

void fp_volume::AddOpenFile(OPEN_FORK_ITEM* forkitem)
{
	mLock.Lock();

	mOpenFiles->AddItem(forkitem);

	mLock.Unlock();
}


/*
 * IsFileOpen()
 *
 * Description:
 *
 * Returns:
 */

bool fp_volume::IsFileOpen(BEntry* entry, int8 fork, uint16* ref)
{
	OPEN_FORK_ITEM* forkitem	= NULL;
	int32			i			= 0;

	mLock.Lock();

	while((forkitem = (OPEN_FORK_ITEM*)mOpenFiles->ItemAt(i++)) != NULL)
	{
		if ((*(forkitem->entry) == *entry) && (forkitem->forkopen == fork))
		{
			if (ref != NULL) {

				*ref = forkitem->refnum;
			}

			mLock.Unlock();
			return( true );
		}
	}

	mLock.Unlock();

	return( false );
}


/*
 * RemoveOpenFile()
 *
 * Description:
 *
 * Returns:
 */

void fp_volume::RemoveOpenFile(OPEN_FORK_ITEM* forkitem)
{
	mLock.Lock();

	mOpenFiles->RemoveItem(forkitem);

	mLock.Unlock();
}


/*
 * GetVolumeParameters()
 *
 * Description:
 *		Get the volume parameters as returned in FPOpenVol and
 *		FPGetVolParms.
 *
 * Returns: AFPERROR
 */

AFPERROR fp_volume::fp_GetVolParms(int16 volBitmap, afp_buffer& afpBuffer)
{
	BEntry		entry(mPath->Path());
	entry_ref	afpVolumeRef;
	time_t		ctime;
	int8*		volNamePtr	= NULL;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//First in the buffer goes the volume bitmap we were passed.
	//
	afpBuffer.push_num(volBitmap);

	//
	//Obtain the entry_ref for the Be Volume we're sitting on
	//so we can obtain information about it.
	//
	if (entry.GetRef(&afpVolumeRef) != B_OK)
	{
		DBGWRITE(dbg_level_error, "Failed to get ref for afp volume\n");
		return( afpParmErr );
	}

	BVolume	volume(afpVolumeRef.device);

	if (volume.InitCheck() != B_OK)
	{
		DBGWRITE(dbg_level_error, "InitCheck() failed for BVolume\n");
		return( afpParmErr );
	}

	if (volBitmap & kFPVolAttributeBit)
	{
		uint16	volAttributes = (	kFPVolSupportsFileIDs		|
									kFPVolSupportsUnicodeNames	|
									kDefaultPrivsFromParent		|
									kNoExchangeFiles 			|
									kSupportsExtAttrs			|
									kSupportsTMLockSteal		|
									kFPVolSupportsBlankAccessPrivileges
								);

		DBGWRITE(dbg_level_trace, "Getting vol kFPVolAttributeBit\n");

		if ((volume.IsReadOnly()) || (mVolumeFlags & kAFPReadOnly)) {

			volAttributes |= kFPVolReadOnly;
		}

		afpBuffer.push_num(volAttributes);
	}

	if (volBitmap & kFPVolSignatureBit)
	{
		afpBuffer.push_num<uint16>(kFixedDirectoryID);
	}

	if (volBitmap & kFPVolCreateDateBit)
	{
		entry.GetCreationTime(&ctime);
		afpBuffer.push_num<uint32>(TO_AFP_TIME(ctime));
	}

	if (volBitmap & kFPVolModDateBit)
	{
		entry.GetModificationTime(&ctime);
		afpBuffer.push_num<uint32>(TO_AFP_TIME(ctime));
	}

	if (volBitmap & kFPVolBackupDateBit)
	{
		afpBuffer.push_num<uint32>(0x80000000);
	}

	if (volBitmap & kFPVolIDBit)
	{
		afpBuffer.push_num(mVolumeID);
	}

	off_t freeBytes = volume.FreeBytes();
	off_t capacity 	= volume.Capacity();

	if (volBitmap & kFPVolBytesFreeBit)
	{
		//
		//For AFP 2.1 and older clients, we cannot report volume
		//sizes larger than 4GB.
		//

		if (freeBytes >= UINT32_MAX)
		{
			freeBytes = UINT32_MAX;
		}

		DBGWRITE(dbg_level_info, "Disk bytes free: %llu\n", freeBytes);

		afpBuffer.push_num<uint32>(freeBytes);
	}

	if (volBitmap & kFPVolBytesTotalBit)
	{
		//
		//The total number of bytes (free + used) on the AFP volume as a uint32.
		//
		off_t bytesTotal = freeBytes + (capacity - freeBytes);

		if (bytesTotal > UINT32_MAX)
		{
			bytesTotal = UINT32_MAX;
		}

		DBGWRITE(dbg_level_info, "Disk bytes total: %llu\n", bytesTotal);

		afpBuffer.push_num<uint32>(bytesTotal);
	}

	if (volBitmap & kFPVolNameBit)
	{
		//
		//Save the position were we'll store the offset to the
		//volume name.
		//
		volNamePtr = afpBuffer.GetCurrentPosPtr();
		afpBuffer.Advance(sizeof(int16));
	}

	//
	//The following 3 bits will only be set if the caller
	//is using AFP 2.2 or later.
	//

	if (volBitmap & kFPVolExtBytesFree)
	{
		afpBuffer.push_num<int64>(freeBytes);
	}

	if (volBitmap & kFPVolExtBytesTotal)
	{
		afpBuffer.push_num<int64>(capacity);
	}

	if (volBitmap & kFPVolBlockSize)
	{
		afpBuffer.push_num<uint32>(1024);
	}

	if (volBitmap & kFPVolNameBit)
	{
		char name[MAX_AFP_NAME];

		strcpy(name, mPath->Leaf());

		*((int16*)volNamePtr) = htons(afpBuffer.GetDataLength()-sizeof(int16));
		afpBuffer.AddCStringAsPascal(name);
	}

	return( AFP_OK );
}


/*
 * fp_OpenVolume()
 *
 * Description:
 *		This implements the FPOpenVolume call for this volume for
 *		AFP access.
 *
 * Returns: AFPERROR
 */

AFPERROR fp_volume::fp_OpenVolume(afp_session* session)
{
	AFPERROR	afpError = B_OK;

	afpError = session->VolumeOpened(this);

	if (AFP_SUCCESS(afpError))
	{
		mOpenedRefCount++;
	}

	return( afpError );
}


/*
 * fp_CloseVolume()
 *
 * Description:
 *		This implements the FPCloseVolume call and closes this
 *		volume for access to this session.
 *
 * Returns: AFPERROR
 */

AFPERROR fp_volume::fp_CloseVolume(afp_session* session)
{
	AFPERROR	afpError = B_OK;

	afpError = session->VolumeClosed(this);

	if (AFP_SUCCESS(afpError))
	{
		mOpenedRefCount--;
	}

	return( afpError );
}



