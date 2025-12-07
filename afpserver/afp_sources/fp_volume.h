#ifndef __fp_volume__
#define __fp_volume__

#include <Path.h>
#include <Directory.h>
#include <BlockCache.h>

#include "afpGlobals.h"
#include "afp_session.h"
#include "afp_buffer.h"

//
//Server specific flags to keep track of volume options.
//
enum
{
	kAFPReadOnly	= 0x02
};

class fp_volume
{
public:
						fp_volume(BPath* path, uint32 srvrVolFlags=0);
	virtual				~fp_volume();
		
	//
	//The volume object keeps track of open files. This is
	//how we know how to set the data or rsrc fork open
	//bits in the file's attributes.
	//
	virtual void		AddOpenFile(OPEN_FORK_ITEM* forkitem);
	virtual bool		IsFileOpen(BEntry* entry, int8 fork, uint16* ref=NULL);
	virtual void		RemoveOpenFile(OPEN_FORK_ITEM* forkitem);
	
	virtual const char*	GetVolumeName() 				{ return(mPath->Leaf()); }
	virtual int8		GetVolumeFlags()				{ return(mVolumeFlags); }
	virtual void		SetVolumeFlags(uint32 flags)	{ mVolumeFlags = flags; }
	virtual int16		GetVolumeID()					{ return(mVolumeID); }
	virtual BPath*		GetPath()						{ return(mPath); }
	virtual BDirectory*	GetDirectory()					{ return(mDirectory); }
	virtual uint32		GetRootDirID()					{ return(mRootDirID); }
	virtual uint32		GetParentOfRootID()				{ return(mParentOfRootID); }
	virtual bool		IsDirty()						{ return(mIsDirty); }
	virtual void		MakeDirty()			{ mLock.Lock(); mIsDirty = true; mLock.Unlock();}
	virtual void		MakeClean()			{ mLock.Lock(); mIsDirty = false; mLock.Unlock();}
	
	virtual AFPERROR	fp_GetVolParms(int16 volBitmap, afp_buffer& afpBuffer);
	virtual AFPERROR 	fp_OpenVolume(afp_session* session);
	virtual AFPERROR	fp_CloseVolume(afp_session* session);
	
private:
		BPath*			mPath;
		BDirectory*		mDirectory;
		char			mVolumeName[MAX_VOLNAME_LEN+1];
		int16			mOpenedRefCount;
		int16			mVolumeID;
		int8			mVolumeFlags;
		
		uint32			mRootDirID;
		uint32			mParentOfRootID;
		
		bool			mIsDirty;
		
		BList*			mOpenFiles;
		BLocker			mLock;
};

#endif //__fp_volume__
