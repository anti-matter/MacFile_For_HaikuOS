#include <fs_attr.h>
#include <DataIO.h>
#include <Node.h>

#include "debug.h"
#include "commands.h"
#include "afpvolume.h"
#include "afp_session.h"
#include "dsi_connection.h"
#include "fp_volume.h"
#include "fp_rangelock.h"
#include "fp_objects.h"

/*
 * afp_session()
 *
 * Description:
 *
 * Returns:
 */

afp_session::afp_session(dsi_connection* dsiConnection)
{
	mOpenVolumes 		= new BList();
	mOpenFiles			= new BList();
	mOpenDesks			= new BList();

	mClientRequestID	= 0;
	mServerRequestID	= 0;
	mRefCount			= 0;
	mAFPVersion			= 0;
	mNextFileRef		= 1;
	mNextDeskRef		= 1;
	mIsAuthenticated	= false;
	mIDLength			= 0;
	mID					= NULL;
	mToken				= *reinterpret_cast<int32*>(this);
	mUAMLoginType		= 0;
	mClientIsSleeping	= false;
	mExtendedLoginBlob	= NULL;
	mExtendedLoginSize	= 0;

	mConnection			= dsiConnection;

	memset(mUserName, 0, sizeof(mUserName));

	SetLastTickleSent();
	SetLastTickleRecvd();
}


/*
 * ~afp_session()
 *
 * Description:
 *
 * Returns:
 */

afp_session::~afp_session()
{
	OPEN_FORK_ITEM*	forkitem	= NULL;
	OPEN_DESK_ITEM*	deskitem	= NULL;

	if (mOpenVolumes->CountItems())
	{
		fp_volume*	volume = NULL;

		while(mOpenVolumes->CountItems() > 0)
		{
			volume = (fp_volume*)mOpenVolumes->FirstItem();

			if (volume != NULL)
			{
				DBGWRITE(dbg_level_warning, "Force closing volume!\n");
				VolumeClosed(volume);
			}
			else
			{
				break;
			}
		}
	}

	if (mOpenFiles->CountItems())
	{
		while((forkitem = (OPEN_FORK_ITEM*)mOpenFiles->FirstItem()) != NULL)
		{
			CloseFile(forkitem->refnum);
		}
	}

	if (mOpenDesks->CountItems())
	{
		while((deskitem = (OPEN_DESK_ITEM*)mOpenDesks->FirstItem()) != NULL)
		{
			CloseDesktop(deskitem->refnum);
		}
	}

	delete mOpenVolumes;
	delete mOpenFiles;
	delete mOpenDesks;

	if (mID != NULL) {
		delete [] mID;
	}

	if (mExtendedLoginBlob != NULL) {
		EndExtendedLogin();
	}
}


/*
 * GetNextClientRequestID()
 *
 * Description:
 *		Returns the next available req ID for use in afp communication.
 *		Think of this as a sequence number for the AFP protocol.
 *
 * Returns: int16
 */

uint16 afp_session::GetNextClientRequestID(uint16 idFromClient)
{
	if (mClientRequestID == DSI_MAX_REQUEST_ID) {
		mClientRequestID = 0;
	}

	if (mClientRequestID == 0) {
		mClientRequestID = idFromClient;
	}else{
		mClientRequestID++;
	}

	return( mClientRequestID );
}


/*
 * GetNextServerRequestID()
 *
 * Description:
 *		Returns the next available req ID for use in afp communication.
 *		Think of this as a sequence number for the AFP protocol.
 *
 * Returns: int16
 */

uint16 afp_session::GetNextServerRequestID()
{
	uint16	id = mServerRequestID;

	if (mServerRequestID == DSI_MAX_REQUEST_ID) {
		mServerRequestID = 0;
	}
	else {

		mServerRequestID++;
	}

	return( id );
}

/*
 * VolumeOpened()
 *
 * Description:
 *		This is called when this session has opened an AFP volume via the
 *		FPOpenVol call.
 *
 * Returns:
 */

AFPERROR afp_session::VolumeOpened(fp_volume* volume)
{
	mLock.Lock();

	if (mOpenVolumes->CountItems() >= MAX_AFP_OPEN_VOLUMES)
	{
		//
		//The client has exceeded his max number of volumes allowed.
		//
		mLock.Unlock();
		return( afpParmErr );
	}
	else
	{
		mOpenVolumes->AddItem(volume);
	}

	mLock.Unlock();

	return( B_OK );
}


/*
 * VolumeClosed()
 *
 * Description:
 *		This is called when this session has closed an AFP volume via the
 *		FPCloseVol call.
 *
 * Returns:
 */

AFPERROR afp_session::VolumeClosed(fp_volume* volume)
{
	AFPERROR	afpError = B_OK;

	mLock.Lock();

	if (mOpenVolumes->HasItem(volume))
	{
		mOpenVolumes->RemoveItem(volume);
	}
	else
	{
		afpError = afpParmErr;
	}

	mLock.Unlock();
	return( afpError );
}


/*
 * HasVolumeOpen()
 *
 * Description:
 *		Returns whether or not this session currently has a particular
 *		volume open.
 *
 * Returns: true if so, false otherwise.
 */

bool afp_session::HasVolumeOpen(fp_volume* volume)
{
	bool	result;

	mLock.Lock();

	result = mOpenVolumes->HasItem(volume);

	mLock.Unlock();
	return( result );
}


/*
 * HasVolumeOpen()
 *
 * Description:
 *		Returns whether or not this session currently has a particular
 *		volume open.
 *
 * Returns: true if so, false otherwise. If volume is non-NULL, the volume
 *			object will be returned.
 */

bool afp_session::HasVolumeOpen(int16 volID, fp_volume** volume)
{
	fp_volume*	afpVolume 	= FindVolume(volID);
	bool		result		= false;

	if (afpVolume != NULL)
	{
		result = HasVolumeOpen(afpVolume);

		if (volume != NULL) {

			*volume = afpVolume;
		}
	}

	return( result );
}

/*
 * OpenFile()
 *
 * Description:
 *		The client is opening a file. Keep track of it.
 *
 * Returns:
 */

AFPERROR afp_session::OpenFile(
	fp_volume* volume,
	BEntry* 	fentry,
	int16 		mode,
	int8 		fork,
	uint16* 	refnum
	)
{
	BFile*			newFile 	= NULL;
	AFPERROR		afpError	= AFP_OK;
	uint32			openMode	= 0;
	OPEN_FORK_ITEM*	forkitem 	= NULL;

	mLock.Lock();

	if (mOpenFiles->CountItems() >= MAX_AFP_FILES_OPEN)
	{
		//
		//The client has exceeded his max number of open files allowed.
		//
		afpError = afpTooManyFilesOpen;
	}
	else
	{
		if ((mode & kReadMode) && (mode & kWriteMode))
			openMode = B_READ_WRITE;
		else if (mode & kReadMode)
			openMode = B_READ_ONLY;
		else if (mode & kWriteMode)
			openMode = B_WRITE_ONLY;

		//
		//Create the bit of memory that will help us track
		//open files for this session.
		//
		forkitem = (OPEN_FORK_ITEM*)malloc(sizeof(OPEN_FORK_ITEM));

		if (forkitem == NULL) {

			return( afpParmErr );
		}

		forkitem->refnum 	= mNextFileRef++;
		forkitem->forkopen	= fork;
		forkitem->entry		= new BEntry(*fentry);
		forkitem->volume	= volume;
		forkitem->mutex		= new BLocker();
		forkitem->brlList	= new BList();
		forkitem->file		= NULL;
		forkitem->rsrcIO	= NULL;
		forkitem->rsrcDirty	= false;

		if (fork == kDataFork)
		{
			//
			//Create the file object for this file. The constructor
			//opens the file for access.
			//
			newFile = new BFile(fentry, openMode);

			if (newFile != NULL)
			{
				if (newFile->InitCheck() != B_OK)
				{
					afpError = afpParmErr;
					delete newFile;
				}
			}

			forkitem->file = newFile;
		}
		else
		{
			//
			//We're opening the Macintosh resource fork. Since Be doesn't
			//deal with resource forks, we have to maintain a memory buffer
			//to hold the resource data.
			//

			forkitem->rsrcIO = OpenAndReadInResourceFork(forkitem->entry);
		}

		//
		//Add this fork to the open fork list for tracking.
		//
		mOpenFiles->AddItem(forkitem);

		//
		//The volume object keeps track of files that are
		//open on its volume.
		//
		volume->AddOpenFile(forkitem);

		*refnum 	= forkitem->refnum;
		afpError	= AFP_OK;
	}

	mLock.Unlock();

	return( afpError );
}


/*
 * OpenAndReadInResourceFork()
 *
 * Description:
 *		Open and read in the contents of the resource fork into
 *		a BMallocIO object.
 *
 * Returns: BMallocIO object if successful
 */

BMallocIO* afp_session::OpenAndReadInResourceFork(BEntry* entry)
{
	BNode		node(entry);
	int8*		readPtr		= NULL;
	BMallocIO*	io			= NULL;
	status_t	status		= 0;
	attr_info	info 		= {0,0};
	int32		actCount	= 0;

	io = new BMallocIO();

	if (io == NULL)
	{
		DPRINT(("[OpenAndReadInResourceFork]Failed to allocate BMallocIO object!!\n"));
		return( NULL );
	}

	//
	//We set the allocation block size to 1024 since that's what
	//the default filesystem block size is anyway.
	//
	io->SetBlockSize(1024);

	//
	//Get the current size of the resource fork on disk.
	//
	status = node.GetAttrInfo(AFP_RSRC_ATTRIBUTE, &info);

	//
	//If GetAttrInfo() failed, it could just be because we haven't
	//created a resource fork on the file yet. That's OK.
	//
	if ((status == B_OK) && (info.size > 0) && (io->SetSize(info.size) == B_OK))
	{
		//
		//Get a ptr to the buffer so we can read the data into it. I'm
		//not sure if this is legal or not.
		//
		readPtr = (int8*)io->Buffer();

		//
		//Read in the current resource fork contents so we can modify it.
		//
		actCount = node.ReadAttr(
						AFP_RSRC_ATTRIBUTE,
						B_RAW_TYPE,
						0,
						readPtr,
						info.size
						);

		if (actCount < B_OK)
		{
			//
			//We couldn't read from the resource fork for some reason.
			//
			DPRINT(("[OpenAndReadInResourceFork]Failed to read resource data!\n"));
		}
	}

	return( io );
}


/*
 * CloseAndWriteOutResourceFork()
 *
 * Description:
 *		Close the fork and write out the new resource fork data to
 *		the resource attribute.
 *
 * Returns: none
 */

void afp_session::CloseAndWriteOutResourceFork(OPEN_FORK_ITEM* forkItem, bool close)
{
	BNode		node(forkItem->entry);
	size_t		afpActCount	= 0;

	if (forkItem->rsrcDirty)
	{
		node.RemoveAttr(AFP_RSRC_ATTRIBUTE);

		//
		//Now write the data back to the resource stream of the file.
		//
		afpActCount = node.WriteAttr(
						AFP_RSRC_ATTRIBUTE,
						B_RAW_TYPE,
						0,
						forkItem->rsrcIO->Buffer(),
						forkItem->rsrcIO->BufferLength()
						);

		//
		//We don't check for an error here since there's nothing we can do
		//at this point since we're closing the file.
		//

		//
		//The resource fork isn't "dirty" anymore, update the bit.
		//
		forkItem->rsrcDirty = false;
	}

	if (close)
	{
		delete forkItem->rsrcIO;
		forkItem->rsrcIO = NULL;
	}
}


/*
 * CloseFile()
 *
 * Description:
 *		The client is closing the file.
 *
 * Returns:
 */

AFPERROR afp_session::CloseFile(uint16 refnum)
{
	OPEN_FORK_ITEM*	forkitem = NULL;

	mLock.Lock();

	forkitem = GetForkItem(refnum);
	if (forkitem == nullptr)
	{
		return afpIDNotFound;
	}

	DBGWRITE(dbg_level_trace, "Closing file with refnum: %lu\n", forkitem->refnum);

	forkitem->mutex->Lock();
	forkitem->volume->RemoveOpenFile(forkitem);

	if (forkitem->rsrcIO != NULL)
	{
		CloseAndWriteOutResourceFork(forkitem);
	}

	if (forkitem->file != NULL)
	{
		if (forkitem->file->Sync() != B_OK) {

			DPRINT(("[afp_session:CloseFile]Sync() failed!\n"));
		}

		delete forkitem->file;
	}

	if (forkitem->entry != NULL) {
		delete forkitem->entry;
	}

	forkitem->mutex->Unlock();
	delete forkitem->mutex;

	mOpenFiles->RemoveItem(forkitem);

	if (forkitem->brlList->CountItems())
	{
		fp_rangelock*	afpLock;

		while((afpLock = (fp_rangelock*)forkitem->brlList->FirstItem()) != NULL)
		{
			delete afpLock;
		}
	}

	free(forkitem);
	forkitem = NULL;

	mLock.Unlock();

	return( AFP_OK );
}


/*
 * GetForkItem()
 *
 * Description:
 *		Returns the fork item associated with a refnum.
 *
 * Returns:
 */

OPEN_FORK_ITEM* afp_session::GetForkItem(uint16 refnum)
{
	OPEN_FORK_ITEM*	forkitem	= NULL;
	int32			i			= 0;

	DBGWRITE(dbg_level_trace, "Getting fork item for ref: %lu\n", refnum);

	mLock.Lock();

	while((forkitem = (OPEN_FORK_ITEM*)mOpenFiles->ItemAt(i++)) != NULL)
	{
		if (forkitem->refnum == refnum)
		{
			mLock.Unlock();
			return( forkitem );
		}
	}

	mLock.Unlock();

	DBGWRITE(dbg_level_error, "Fork item for ref %lu not found in open files list!\n", refnum);

	return( NULL );
}


/*
 * GetBFileFromRef()
 *
 * Description:
 *		Returns the BFile object associated with a refnum.
 *
 * Returns:
 */

BFile* afp_session::GetBFileFromRef(uint16 refnum)
{
	OPEN_FORK_ITEM*	forkitem = NULL;

	forkitem = GetForkItem(refnum);

	if (forkitem != NULL)
	{
		return( forkitem->file );
	}

	return( NULL );
}

/*
 * OpenDesktop()
 *
 * Description:
 *		Records that a desktop file is now opened by this session.
 *
 * Returns:
 */

AFPERROR afp_session::OpenDesktop(
	BEntry*		entry,
	BFile*		file,
	int16		volID,
	uint16*		refnum
	)
{
	if (!HasVolumeOpen(volID))
	{
		DBGWRITE(dbg_level_warning, "Volume not opened\n");
		return afpParmErr;
	}

	OPEN_DESK_ITEM*	deskitem = NULL;

	if ((entry == NULL) || (file == NULL))
	{
		return( afpParmErr );
	}

	//
	//Create the memory block that tracks this open desktop file.
	//
	deskitem = (OPEN_DESK_ITEM*)malloc(sizeof(OPEN_DESK_ITEM));

	if (deskitem == NULL)
	{
		//
		//We failed to allocate the memory for a new desktop.
		//
		return( afpParmErr );
	}

	//
	//Record everything we'll need to know about this open desktop file.
	//
	deskitem->refnum	= mNextDeskRef++;
	deskitem->volID		= volID;
	deskitem->file		= file;
	deskitem->entry		= entry;

	*refnum = deskitem->refnum;

	mLock.Lock();

	mOpenDesks->AddItem(deskitem);

	mLock.Unlock();

	return( AFP_OK );
}


/*
 * CloseDesktop()
 *
 * Description:
 *		Stops tracking a (now closing) open desktop file.
 *
 * Returns:
 */

AFPERROR afp_session::CloseDesktop(uint16 refnum)
{
	OPEN_DESK_ITEM*	deskitem = NULL;

	deskitem = GetDeskItem(refnum);

	if (deskitem != NULL)
	{
		mLock.Lock();

		if (deskitem->file != NULL) {

			delete deskitem->file;
		}

		if (deskitem->entry != NULL) {

			delete deskitem->entry;
		}

		mOpenDesks->RemoveItem(deskitem);

		free(deskitem);
		deskitem = NULL;

		mLock.Unlock();
	}
	else
	{
		return( afpParmErr );
	}

	return( AFP_OK );
}


/*
 * GetDeskItem()
 *
 * Description:
 *		Returns the desk item associated with a refnum.
 *
 * Returns:
 */

OPEN_DESK_ITEM* afp_session::GetDeskItem(uint16 refnum)
{
	OPEN_DESK_ITEM*	deskitem	= NULL;
	int32			i			= 0;

	mLock.Lock();

	while((deskitem = (OPEN_DESK_ITEM*)mOpenDesks->ItemAt(i++)) != NULL)
	{
		if (deskitem->refnum == refnum)
		{
			mLock.Unlock();
			return( deskitem );
		}
	}

	mLock.Unlock();

	return( NULL );
}

/*
 * SetAFPVersion()
 *
 * Description:
 *		Set the AFP version this session is talking to us with.
 *
 * Returns: None
 */

void afp_session::SetAFPVersion(int8 afpVersion)
{
	mAFPVersion = afpVersion;
}


/*
 * GetAFPVersion()
 *
 * Description:
 *		Get the AFP version this client is using.
 *
 * Returns: int16, the AFP version.
 */

int8 afp_session::GetAFPVersion()
{
	return mAFPVersion;
}


/*
 * SetUAMLoginInfo()
 *
 * Description:
 *		Sets the login tinfo.
 *
 * Returns: nothing.
 */

void afp_session::SetUAMLoginInfo(AFP_USER_DATA* userInfo)
{
	if (userInfo != NULL) {
		strncpy(mUserName, userInfo->username, sizeof(mUserName));
	}
}


/*
 * GetUserInfo()
 *
 * Description:
 *		Return the user info data structure for this user.
 *
 * Returns: none
 */

AFPERROR afp_session::GetUserInfo(AFP_USER_DATA* userInfo)
{
	AFPERROR	afpError = AFP_OK;

	//
	//We update the user info in case it has been changed while
	//this session has been logged on.
	//
	afpError = afpGetUserDataByName(mUserName, userInfo);

	return( afpError );
}


/*
 * GetUAMLoginType()
 *
 * Description:
 *		Returns the uam used for login.
 *
 * Returns: int16, the AFP version.
 */

int8 afp_session::GetUAMLoginType()
{
	return mUAMLoginType;
}


/*
 * IsAdmin()
 *
 * Description:
 *		Returns whether this user has admin previleges.
 *
 * Returns: bool
 */

bool afp_session::IsAdmin()
{
	AFP_USER_DATA	userInfo;
	AFPERROR		afpError = AFP_OK;

	if (mUAMLoginType != afpUAMGuest)
	{
		//
		//We update the user info in case it has been changed while
		//this session has been logged on.
		//
		afpError = afpGetUserDataByName(mUserName, &userInfo);

		if (AFP_SUCCESS(afpError))
		{
			return( (userInfo.flags & kIsAdmin) != 0 );
		}
		else
		{
			//
			//The user has been deleted while he was logged on. Kill
			//the session.
			//
			KillSession();
		}
	}

	return( false );
}


/*
 * CanChangePassword()
 *
 * Description:
 *		Returns TRUE if the user is allowed to change his/her password.
 *
 * Returns: boolean
 */

bool afp_session::CanChangePassword()
{
	AFP_USER_DATA 	userInfo;
	AFPERROR		afpError	= AFP_OK;
	bool			result		= false;

	afpError = GetUserInfo(&userInfo);

	if (AFP_SUCCESS(afpError))
	{
		if (userInfo.flags & kCanChngPswd) {

			result = true;
		}
	}

	return( result );
}


/*
 * SetClientID()
 *
 * Description:
 *		Sets the client ID as received by FPGetSessionToken.
 *
 * Returns: int32 - The session token to be passed back to client. The
 *			return value will be -1 if we fail to store the ID.
 */

void afp_session::SetClientID(int32 idSize, int8* id)
{
	if (mID != NULL) {

		delete [] mID;
	}

	mID 		= id;
	mIDLength	= idSize;
}


/*
 * GetClientID()
 *
 * Description:
 *		Returns the client ID as set by SetClientID().
 *
 * Returns: none
 */

void afp_session::GetClientID(int32* idSize, int8** id)
{
	if (idSize != NULL)
		*idSize	= mIDLength;

	if (id != NULL)
		*id	= mID;
}


/*
 * KillSession()
 *
 * Description:
 *		Force kills the session.
 *
 * Returns: none
 */

void afp_session::KillSession()
{
	DPRINT(("[afp_session::KillSession]Killing session\n"));

	mConnection->SendAttention(ATTN_USER_DISCONNECT);
	mConnection->KillSession();
}


/*
 * BeginExtendedLogin()
 *
 * Description:
 *		Stores an object-owned blob of data to store user information between
 *		FPLogin() and FPLoginCont() functions. The format of the data is unknown
 *		and this class will dispose of the blob on exit if it still exists.
 *
 *		Call EndExtendedLogin() to dispose of and clear the data.
 *
 * Returns: a pointer to the blob
 */

void* afp_session::BeginExtendedLogin(
	uint32	inRequiredBlobSize
)
{
	if (mExtendedLoginBlob == NULL)
	{
		uint8*	p = new uint8[inRequiredBlobSize];

		if (p != NULL)
		{
			memset(p, 0, inRequiredBlobSize);

			mExtendedLoginBlob = p;
			mExtendedLoginSize = inRequiredBlobSize;

			return( p );
		}
	}

	return( NULL );
}


/*
 * EndExtendedLogin()
 *
 * Description:
 *		After calling BeginExtendedLogin(), this function cleans up the
 *		allocated memory *after* zeroing out all data in the buffer.
 *
 * Returns: none
 */

void afp_session::EndExtendedLogin()
{
	if (mExtendedLoginBlob != NULL)
	{
		memset(mExtendedLoginBlob, 0, mExtendedLoginSize);

		delete [] mExtendedLoginBlob;
		mExtendedLoginBlob = NULL;

		mExtendedLoginSize = 0;
	}
}






