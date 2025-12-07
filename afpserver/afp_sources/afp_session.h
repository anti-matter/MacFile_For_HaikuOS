#ifndef __afp_session__
#define __afp_session__

#include <List.h>
#include <Locker.h>
#include <File.h>
#include <Entry.h>

#include "afpGlobals.h"
#include "afp.h"
#include "afplogon.h"

class fp_volume;
class dsi_connection;

#define AFP_SESSION_TOKEN_SIZE	sizeof(int32)

typedef struct
{
	uint16			refnum;
	BFile*			file;
	int16			forkopen;	//either kResourceForkBit or kDataFork
	BEntry*			entry;
	fp_volume*		volume;
	BList*			brlList;
	BLocker*		mutex;
	BMallocIO*		rsrcIO;		//Cache that holds entire rsrc fork data
	bool			rsrcDirty;	//Flag as to whether fork is "dirty" and needs to be written to disk
}OPEN_FORK_ITEM;

typedef struct
{
	uint16		refnum;
	int16		volID;
	BFile*		file;
	BEntry*		entry;
}OPEN_DESK_ITEM;


class afp_session
{
public:
						afp_session(dsi_connection* dsiConnection);
	virtual				~afp_session();
	
	virtual uint16		GetNextClientRequestID(uint16 idFromClient);
	virtual uint16		GetCurrentRequestID() { return mClientRequestID; }
	
	virtual uint16		GetNextServerRequestID(void);

	//Volume methods
	virtual AFPERROR	VolumeOpened(fp_volume* volume);
	virtual AFPERROR	VolumeClosed(fp_volume* volume);
	
	virtual bool		HasVolumeOpen(fp_volume* volume);
	virtual bool		HasVolumeOpen(int16 volID, fp_volume** volume=NULL);

	//File methods
	virtual AFPERROR		OpenFile(
								fp_volume* volume,
								BEntry* 	fentry,
								int16 		mode,
								int8 		fork,
								uint16* 	refnum
								);
	
	virtual BMallocIO* 		OpenAndReadInResourceFork(BEntry* entry);
	virtual void			CloseAndWriteOutResourceFork(OPEN_FORK_ITEM* forkItem, bool close=true);
	virtual AFPERROR		CloseFile(uint16 refnum);
	virtual OPEN_FORK_ITEM*	GetForkItem(uint16 refnum);
	virtual BFile*			GetBFileFromRef(uint16 refnum);
	
	//Desktop methods
	virtual AFPERROR		OpenDesktop(
								BEntry*		entry,
								BFile*		file,
								int16		volID,
								uint16*		refnum
								);
						
	virtual AFPERROR		CloseDesktop(uint16 refnum);
	virtual OPEN_DESK_ITEM* GetDeskItem(uint16 refnum);
	
	//Token info
	virtual void		SetClientID(int32 idSize, int8* id);
	virtual void		GetClientID(int32* idSize, int8** id);
	virtual void		SetAFPTimeStamp(uint32 timeStamp)	{ mAFPTimeStamp = timeStamp; }
	virtual uint32		GetAFPTimeStamp()					{ return mAFPTimeStamp; }
	virtual int32		GetToken()							{ return mToken; }
	virtual void		KillSession();
	virtual void*		BeginExtendedLogin(
								uint32	inRequiredBlobSize
								);
	virtual void		EndExtendedLogin();
	virtual void*		GetExtendedLoginInfo() { return mExtendedLoginBlob; }
			
	virtual int32		GetLastTickleRecvd()	{ return mLastTickleRecvd; }
	virtual void		SetLastTickleRecvd()	{ mLastTickleRecvd = real_time_clock(); mClientIsSleeping = false; }
	
	virtual int32		GetLastTickleSent()		{ return mLastTickleSent; }
	virtual void		SetLastTickleSent()		{ mLastTickleSent = real_time_clock(); }
	
	virtual void		SetAFPVersion(int8 afpVersion);
	virtual int8		GetAFPVersion();
	virtual void		SetUAMLoginInfo(AFP_USER_DATA* userInfo);
	virtual AFPERROR	GetUserInfo(AFP_USER_DATA* userInfo);
	virtual int8		GetUAMLoginType();
	virtual void		SetUAMLoginType(int8 utype) { mUAMLoginType = utype; }
	virtual char*		GetUserName() 	{ return mUserName; }
	virtual bool		IsAdmin();
	virtual bool		CanChangePassword();
	virtual bool		IsUser()		{ return( (GetUAMLoginType() != afpUAMGuest) && (!IsAdmin()) ); }
	virtual bool		IsGuest()		{ return( GetUAMLoginType() == afpUAMGuest ); }
	
	virtual void		IncRefCount() { mRefCount++; }
	virtual int16		GetRefCount() { return mRefCount; }
	
	virtual bool		IsAuthenticated() { return mIsAuthenticated; }
	virtual void		SetIsAuthenticated(bool authStatus) { mIsAuthenticated = authStatus; }
	
	virtual void		SetClientIsSleeping(bool isSleeping) 	{ mClientIsSleeping = isSleeping; }
	virtual bool		ClientIsSleeping()						{ return mClientIsSleeping; }
	
	virtual void		Lock()		{ mLock.Lock(); }
	virtual void		Unlock()	{ mLock.Unlock(); }
		
private:

	int32			mLastTickleRecvd;
	int32			mLastTickleSent;
	uint16			mClientRequestID;
	uint16			mServerRequestID;
	int16			mRefCount;
	int8			mAFPVersion;
	int8			mUAMLoginType;
	uint16			mNextFileRef;
	uint16			mNextDeskRef;
	bool			mIsAuthenticated;
	char			mUserName[128];
	
	BList*			mOpenFiles;
	BList*			mOpenVolumes;
	BList*			mOpenDesks;
	
	BLocker			mLock;
	
	dsi_connection*	mConnection;
	
	//
	//This is for the new FPZzzz command which tells the server
	//when the client is going to sleep.
	//
	bool			mClientIsSleeping;
	
	//
	//These are used so the AFP client can reference this session in
	//the event of a client crash or disconnection.
	//
	int32			mIDLength;
	int8*			mID;
	
	int32			mToken;
	uint32			mAFPTimeStamp;
	
	//
	//For extended UAMs we need to store some extra information
	//
	uint8*			mExtendedLoginBlob;
	uint32			mExtendedLoginSize;
};







#endif //__afp_session__
