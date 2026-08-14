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
 *		INT32_MAX size limits by clamping to INT32_MAX where needed.
 *
 * Returns: AFP_OK on success, afpParmErr (-5019) if entry ref or BVolume
 *		initialization fails.
 */

AFPERROR fp_volume::fp_GetVolParms(int16 volBitmap, int8 afpVersion, afp_buffer& afpBuffer)
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
		// Match netatalk: start with 0, only set flags the client supports.
		uint16	volAttributes = kFPVolSupportsFileIDs | kSupportsExtAttrs;
;

		DBGWRITE(dbg_level_trace, "Getting vol kFPVolAttributeBit\n");

		if ((volume.IsReadOnly()) || (mVolumeFlags & kAFPReadOnly)) {
			volAttributes |= kFPVolReadOnly;
		}

		//
		//Only advertise AFP 3.x features to clients that support them.
		//AppleShare Client 3.7.4 (AFP 2.2) crashes when it sees unknown
		//volume attribute flags, so we must not set them for older clients.
		//
		if (afpVersion >= afpVersion30)
		{
			DBGWRITE(dbg_level_trace, "Setting AFP 3.x volume attributes for AFP version %d\n", afpVersion);

			volAttributes |= (	kFPVolSupportsUnicodeNames			|
								kDefaultPrivsFromParent				|
								kNoExchangeFiles 					|
								kSupportsTMLockSteal
							);
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
		DBGWRITE(
			dbg_level_trace,
			"Volume creation Unix=%lld AFP(host)=%#010x\n",
			static_cast<long long>(ctime),
			static_cast<unsigned>(TO_AFP_TIME(ctime)));

		afpBuffer.push_num<uint32>(TO_AFP_TIME(ctime));
	}

	if (volBitmap & kFPVolModDateBit)
	{
		entry.GetModificationTime(&ctime);
		DBGWRITE(
			dbg_level_trace,
			"Volume creation Unix=%lld AFP(host)=%#010x\n",
			static_cast<long long>(ctime),
			static_cast<unsigned>(TO_AFP_TIME(ctime)));
			
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
		//sizes larger than INT32_MAX.
		//
		off_t freeBytesClamped = freeBytes;

		if (freeBytes >= INT32_MAX)
		{
			freeBytesClamped = INT32_MAX;
		}

		DBGWRITE(dbg_level_trace, "kFPVolBytesFreeBit: %llu\n", freeBytesClamped);

		afpBuffer.push_num<uint32>(freeBytesClamped);
	}

	if (volBitmap & kFPVolBytesTotalBit)
	{
		//
		//The total number of bytes (free + used) on the AFP volume as an int32 (AFP 2.x clamped to INT32_MAX).
		//Algebraically: freeBytes + (capacity - freeBytes) == capacity.
		//Use unclamped freeBytes so the formula is correct for disks >INT32_MAX.
		//Guard against negative freeBytes from filesystem errors.
		//
		off_t bytesTotal = capacity;

		if (bytesTotal < 0 || bytesTotal > INT32_MAX)
		{
			bytesTotal = (bytesTotal < 0) ? 0 : INT32_MAX;
		}

		DBGWRITE(dbg_level_trace, "kFPVolBytesTotalBit: %llu\n", bytesTotal);

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
		DBGWRITE(dbg_level_trace, "kFPVolExtBytesFree: %llu\n", freeBytes);
		afpBuffer.push_num<int64>(freeBytes);
	}

	if (volBitmap & kFPVolExtBytesTotal)
	{
		DBGWRITE(dbg_level_trace, "kFPVolExtBytesTotal: %llu\n", capacity);
		afpBuffer.push_num<int64>(capacity);
	}

	if (volBitmap & kFPVolBlockSize)
	{
		DBGWRITE(dbg_level_trace, "kFPVolBlockSize: 1024\n");
		afpBuffer.push_num<uint32>(1024);
	}

	if (volBitmap & kFPVolNameBit)
	{
		auto offset = afpBuffer.GetDataLength()-sizeof(int16);

		*((int16*)volNamePtr) = htons(offset);
		afpBuffer.AddCStringAsPascal(mPath->Leaf());

		DBGWRITE(dbg_level_trace, "kFPVolNameBit: offset=%d, name=%s\n", offset, mPath->Leaf());
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



