#include <Entry.h>
#include <Volume.h>
#include <Directory.h>
#include <netinet/in.h>

#include "debug.h"
#include "afp.h"
#include "fp_volume.h"

uint16 gNextVolumeID = 1;

/*
 * fp_volume()
 *
 * Description:
 *		Constructor. Initializes a new shared AFP volume with the given
 *		path and server flags. Assigns a unique volume ID, creates the
 *		root BDirectory, initializes the open-files list, and resolves
 *		the root directory's node ID and its parent's node ID for later
 *		file-ID lookups.
 *
 * Returns: N/A (constructor)
 */

fp_volume::fp_volume(BPath* path, uint32 srvrVolFlags)
{
	BDirectory		dir;

	if (path == nullptr) {
		DBGWRITE(dbg_level_error, "Null path passed to fp_volume constructor\n");
		return;
	}

	mOpenedRefCount = 0;
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
 *		Destructor. Releases the dynamically allocated mPath, mDirectory,
 *		and mOpenFiles objects owned by this volume instance.
 *
 * Returns: N/A (destructor)
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
 *		Adds an OPEN_FORK_ITEM to this volume's list of open file forks.
 *		The list is used to track which files are currently open so that
 *		file attribute bits (data fork / resource fork open flags) can be
 *		set correctly in AFP responses. Protected by mLock for thread safety.
 *
 * Returns: N/A
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
 *		Searches this volume's open-files list for a fork item whose BEntry
 *		matches the given entry and whose fork type matches (or is a data
 *		fork alias of a resource-open file). If found and ref is non-NULL,
 *		the file's reference number is written through to *ref. Thread-safe
 *		via mLock.
 *
 * Returns: true if the file/fork is currently open on this volume, false otherwise.
 */

bool fp_volume::IsFileOpen(BEntry* entry, int8 fork, uint16* ref)
{
	OPEN_FORK_ITEM* forkitem	= NULL;
	int32			i			= 0;

	mLock.Lock();

	while((forkitem = (OPEN_FORK_ITEM*)mOpenFiles->ItemAt(i++)) != NULL)
	{
		if ((*(forkitem->entry) == *entry) && (forkitem->forkopen == fork || (forkitem->forkopen == kDataFork && forkitem->isResFile)))
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
 *		Removes an OPEN_FORK_ITEM from this volume's open-files list,
 *		corresponding to a file fork that has been closed. Thread-safe
 *		via mLock.
 *
 * Returns: N/A
 */

void fp_volume::RemoveOpenFile(OPEN_FORK_ITEM* forkitem)
{
	mLock.Lock();

	mOpenFiles->RemoveItem(forkitem);

	mLock.Unlock();
}


/*
 * fp_GetVolParms()
 *
 * Description:
 *		Gathers volume parameters for the AFP FPOpenVol and FPGetVolParms
 *		responses. Based on the volBitmap requested fields, it writes to
 *		afpBuffer: volume attributes (read-only, Unicode support, ACLs,
 *		etc.), signature (fixed directory ID), creation/modification dates,
 *		volume ID, free/total bytes (32-bit and 64-bit variants), block
 *		size, and the volume name as a pascal string. Handles AFP 2.x
 *		4GB size limits by clamping to 0x7FFFFFFF where needed (avoids
 *
 *		crashing older AFP clients that interpret UINT32_MAX as -1).
 * Returns: AFP_OK on success, afpParmErr (-5019) if entry ref or BVolume
 *		initialization fails.
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
		//For AFP 2.x clients, we cannot report volume sizes larger than 4GB.
		//Use 0x7FFFFFFF instead of UINT32_MAX — older AFP clients (e.g.
		//AppleShare Client 3.7.x) interpret 0xFFFFFFFF as -1 and crash.
		//
		off_t freeBytesClamped = freeBytes;

		if (freeBytes >= 0x7FFFFFFF)
		{
			freeBytesClamped = 0x7FFFFFFF;
		}

		DBGWRITE(dbg_level_info, "Disk bytes free: %llu\n", freeBytesClamped);

		afpBuffer.push_num<uint32>(freeBytesClamped);
	}

	if (volBitmap & kFPVolBytesTotalBit)
	{
		//
		//The total number of bytes (free + used) on the AFP volume as a uint32.
		//Algebraically: freeBytes + (capacity - freeBytes) == capacity.
		//Use unclamped freeBytes so the formula is correct for disks >4GB.
		//Guard against negative freeBytes from filesystem errors.
		//Use 0x7FFFFFFF instead of UINT32_MAX — older AFP clients (e.g.
		//AppleShare Client 3.7.x) interpret 0xFFFFFFFF as -1 and crash.
		//
		off_t bytesTotal = capacity;

		if (bytesTotal < 0 || bytesTotal > 0x7FFFFFFF)
		{
			bytesTotal = (bytesTotal < 0) ? 0 : 0x7FFFFFFF;
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
 *		Implements the FPOpenVol AFP command for this volume. Registers
 *		this volume as opened in the given session via session->VolumeOpened(),
 *		and increments mOpenedRefCount on success. Used by a Mac client to
 *		switch its active volume context.
 *
 * Returns: AFP_OK on success, or an AFP error code if registration fails.
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
 *		Implements the FPCloseVol AFP command for this volume. Unregisters
 *		this volume from the given session via session->VolumeClosed(), and
 *		decrements mOpenedRefCount on success. Used by a Mac client to
 *		switch away from this volume.
 *
 * Returns: AFP_OK on success, or an AFP error code if unregistration fails.
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



