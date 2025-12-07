#include "afpvolume.h"
#include "debug.h"
#include "dsi_scavenger.h"
#include "fp_volume.h"


extern std::mutex volume_blist_mutex;
extern std::unique_ptr<BList> volume_blist;

/*
 * dsi_scavenger()
 *
 * Description:
 *		Constructor
 *
 * Returns:
 */

dsi_scavenger::dsi_scavenger()
{
	thread_id	newID;
	
	mOpenConnections = new BList();
	
	newID = spawn_thread(
				dsi_scavenger::ScavengerThread,
				"afp_session_mgr",
				B_NORMAL_PRIORITY,
				this
				);
				
	resume_thread( newID );
}


/*
 * ~dsi_scavenger()
 *
 * Description:
 *		Destructor
 *
 * Returns:
 */

dsi_scavenger::~dsi_scavenger()
{
	if (mOpenConnections != NULL) {
	
		delete mOpenConnections;
	}
}


/*
 * TrackConnection()
 *
 * Description:
 *		Destructor
 *
 * Returns:
 */

void dsi_scavenger::TrackConnection(dsi_connection* connection)
{
	std::lock_guard<std::mutex> guard(mMutex);
	
	if (connection != NULL) {
	
		mOpenConnections->AddItem(connection);
	}
}


/*
 * StopTracking()
 *
 * Description:
 *		Destructor
 *
 * Returns:
 */

void dsi_scavenger::StopTracking(dsi_connection* connection)
{
	std::lock_guard<std::mutex> guard(mMutex);
	
	if (connection != NULL) {
	
		mOpenConnections->RemoveItem(connection);
	}
}


/*
 * SessionMgr() [STATIC]
 *
 * Description:
 *		This thread performs 2 functions. It is both a scavanger, looking
 *		for orphaned sessions and a thread that notifies when a volume's
 *		contents has changed and notifies the clients who have the volume
 *		mounted.
 *
 * Returns: None
 */

int32 dsi_scavenger::ScavengerThread(void* data)
{
	dsi_scavenger*		manager 		= (dsi_scavenger*)data;
	dsi_connection*		connection		= NULL;
	afp_session*		session			= NULL;
	fp_volume*			volume			= NULL;
	int32				recvInterval	= 0;
	int32				sentInterval	= 0;
	int32				now				= 0;
	int32				i, j;
	
	while(true)
	{
		snooze(SEND_TICKLE_INTERVAL*1000000);
		
		std::lock_guard<std::mutex> guard(manager->mMutex);
		
		now = real_time_clock();
		i	= 0;
		
		while((connection = (dsi_connection*)manager->mOpenConnections->ItemAt(i++)) != NULL)
		{
			session = connection->GetAFPSessionObject();
			
			if (session != NULL)
			{
				//
				//If we haven't sent any packets to the client for a while,
				//we'll want to "tickle" him so he doesn't things we've
				//forgoten all about him.
				//
				sentInterval = (now - session->GetLastTickleSent());
				
				if ((sentInterval > SEND_TICKLE_INTERVAL) && (!session->ClientIsSleeping()))
				{
					connection->SendTickle();
				}
				
				//
				//Now check to make sure we've heard from the client in
				//in a reasonable amount of time.
				//
				recvInterval = (now - session->GetLastTickleRecvd());
				
				if (recvInterval > SESSION_DEAD_INTERVAL)
				{
					if (!session->ClientIsSleeping())
					{
						connection->KillSession();
						DBGWRITE(dbg_level_warning, "Client died! Killed connection\n");
					}
					else
					{
						//
						//The client is asleep. Check to see if its been sleeping past
						//the time we allow them to sleep before killing.
						//
						DBGWRITE(dbg_level_trace, "Client is sleeping\n");
						
						if (recvInterval > SESSION_SLEEPING_INTERVAL)
						{
							//
							//The client has been sleeping for too long. Kill it.
							//
							connection->KillSession();
							DBGWRITE(dbg_level_warning, "Killing sleeping client\n");
						}
					}
				}
				else
				{
					j = 0;
					
					//
					//If we're still alive, see if there are dirty volumes that
					//need to be reported to clients via the attention mechanism.
					//					
					std::lock_guard lock(volume_blist_mutex);
							
					while((volume = (fp_volume*)volume_blist->ItemAt(j++)) != NULL)
					{						
						if (volume->IsDirty())
						{							
							if (session->HasVolumeOpen(volume))
							{
								connection->SendAttention(ATTN_SERVER_NOTIFY);						
							}
						}
					}
				}
			}
		}
		
		//
		//We've just been through all sessions, now make sure all
		//the volumes are marked as clean.
		//
		MarkAllVolumesClean();
				
	} //while(true)
}


/*
 * SendGlobalAttention()
 *
 * Description:
 *		Sends an attention command globally to all connected sessions.
 *		This is good for server messages or server shutdowns.
 *
 * Returns: None
 */

void dsi_scavenger::SendGlobalAttention(uint16 attentionMsg)
{
	dsi_connection*	connection	= NULL;
	int				i			= 0;

	std::lock_guard<std::mutex> guard(mMutex);
	
	//
	//Loop through the connection objects and send them
	//the passed attention code.
	//
		
	while((connection = (dsi_connection*)mOpenConnections->ItemAt(i++)) != NULL)
	{
		DBGWRITE(dbg_level_trace, "Sending global attention (0x%x)\n", attentionMsg);
		
		//
		//Call the connection object to handle the dirty work of
		//sending the attention to this session.
		//
		
		connection->SendAttention(attentionMsg);
	}
}


/*
 * FindSessionByID()
 *
 * Description:
 *		Finds a session using the ID supplied by the client via
 *		FPGetSessionToken().
 *
 * Returns: None
 */

afp_session* dsi_scavenger::FindSessionByID(int32 idSize, int8* id)
{
	dsi_connection*	connection	= NULL;
	afp_session*	session		= NULL;
	afp_session*	result		= NULL;
	int32			cSize		= 0;
	int8*			cID			= NULL;
	int				i			= 0;

	std::lock_guard<std::mutex> guard(mMutex);
	
	while((connection = (dsi_connection*)mOpenConnections->ItemAt(i++)) != NULL)
	{
		session = connection->GetAFPSessionObject();
		
		if (session != NULL)
		{			
			session->GetClientID(&cSize, &cID);
						
			if ((cSize == idSize) && (cID != NULL) && (!memcmp(id, cID, idSize)))
			{
				result = session;
				break;
			}
		}
	}
		
	return( result );
}


/*
 * FindSessionByToken()
 *
 * Description:
 *		Finds a session using the session token.
 *
 * Returns: None
 */

afp_session* dsi_scavenger::FindSessionByToken(int32 token)
{
	dsi_connection*	connection	= NULL;
	afp_session*	session		= NULL;
	afp_session*	result		= NULL;
	int				i			= 0;

	std::lock_guard<std::mutex> guard(mMutex);
		
	while((connection = (dsi_connection*)mOpenConnections->ItemAt(i++)) != NULL)
	{
		session = connection->GetAFPSessionObject();
		
		if (session != NULL)
		{			
			if (session->GetToken() == token)
			{
				result = session;
				break;
			}
		}
	}
		
	return( result );
}























