#ifndef __dsi_scavenger__
#define __dsi_scavenger__

#include <mutex>

#include "afpGlobals.h"
#include "afp.h"
#include "dsi_connection.h"

//
//The server will send tickles to the client every 15 seconds
//unless the server has recently replied or communicated with
//the client.
//
#define SEND_TICKLE_INTERVAL		15		//seconds

//
//After the following time has elapsed, we will kill the session
//and assume the client has died.
//
#define SESSION_DEAD_INTERVAL		120		//seconds

//
//This is how long we keep sleeping clients around before freeing
//the resources its taking.
//
#define SESSION_SLEEPING_INTERVAL	(60*60*24)


class dsi_scavenger
{
public:
							dsi_scavenger();
	virtual					~dsi_scavenger();
	
	virtual void			TrackConnection(dsi_connection* connection);
	virtual void			StopTracking(dsi_connection* connection);
	
	virtual void			SendGlobalAttention(uint16 attentionMsg);
	
	virtual afp_session*	FindSessionByID(int32 idSize, int8* id);
	virtual afp_session*	FindSessionByToken(int32 token);
	
	virtual int32			NumOpenSessions() { return mOpenConnections->CountItems(); }
	
private:
	
	static int32		ScavengerThread(void* data);
	
	//Guard the list that keeps track of open connections.
	std::mutex			mMutex;
	BList*				mOpenConnections;
};

#endif //__dsi_scavenger__
