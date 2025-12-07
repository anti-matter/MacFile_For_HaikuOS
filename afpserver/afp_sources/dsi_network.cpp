#include <OS.h>
#include <stdio.h>
#include <strings.h>

#include "debug.h"
#include "afpmsg.h"
#include "afphostname.h"
#include "dsi_network.h"
#include "dsi_connection.h"
#include "dsi_scavenger.h"

bool 			gServerRunning	= true;
dsi_scavenger*	gAFPSessionMgr 	= NULL;


/*
 * afpInitializeServerNetworking()
 *
 * Description:
 *		Initializes the server networking for this AFP server. It spawns the
 *		thread that will listen for incoming connections.
 *
 * Returns: None
 */

void afpInitializeServerNetworking()
{
	thread_id	newID;
	
	gServerRunning = true;
	
	afp_Initialize(); //afpmsg.cpp
	
	//
	//This will initilalize the hostname that we use. We don't need to
	//do this, actually. But, we do it anyway to reduce any potential
	//for a delay in the first logon to the server.
	//
	afp_GetHostname(NULL, 0);
		
	newID = spawn_thread(
				afpSrvrConnectThread,
				"afp_listen",
				B_NORMAL_PRIORITY,
				NULL
				);
	
	resume_thread( newID );
	
	gAFPSessionMgr = new dsi_scavenger();
}


/*
 * afpSrvrConnectThread()
 *
 * Description:
 *		This thread sits around and listens for connections then spawns
 *		a new thread and network object to handle them.
 *
 * Returns: None
 */

status_t afpSrvrConnectThread(void* data)
{
	#pragma unused(data)
	
	int					listenSocket;
	int					newSocket;
	sockaddr_in			sa;
	uint				saSize;
	int					id;
	char				threadname[64];
	
	do
	{
		DBGWRITE(dbg_level_info, "Initializing listening socket\n");
		
		snooze(1000000);
		
		listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		
		if (listenSocket < 0)
		{
			//
			//We failed to create the new socket.
			//
			DBGWRITE(dbg_level_error, "Failed to create socket\n");
			continue;
		}
		
		//
		//Set the address format for the bind operation.
		//
		memset(&sa, 0, sizeof(sa));
		
		sa.sin_family		= AF_INET;
		sa.sin_port			= htons(AFP_TCP_PORT);
		sa.sin_addr.s_addr	= INADDR_ANY;
		
		int yes = 1;
		
		if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
		{
			DBGWRITE(dbg_level_error, "setsockopt() failed creating listening socket.\n");
		}
		
		if (bind(listenSocket, (struct sockaddr*)&sa, sizeof(sa)) < 0)
		{
			//
			//We failed to bind, damit.
			//
			DBGWRITE(dbg_level_error, "Failed to bind()!\n");
			
			shutdown(listenSocket, SHUT_RDWR);
			continue;
		}
				
		//
		//Now tell the socket to begin listening for new connections
		//
		if (listen(listenSocket, 25) < 0)
		{
			DBGWRITE(dbg_level_error, "Failed to listen()!\n");
			
			shutdown(listenSocket, SHUT_RDWR);
			continue;
		}
		
		DBGWRITE(dbg_level_info, "Ready to accept connections %s...\n");
		
		id = 1;
		
		while(gServerRunning)
		{
			thread_id		newID = 0;
			int				val   = 1;
			
			DBGWRITE(dbg_level_trace, "Waiting for a new connection...\n");
			
			saSize 		= sizeof(sa);
			newSocket 	= accept(listenSocket, (struct sockaddr*)&sa, &saSize);
			
			if (newSocket < 0)
			{
				//
				//We had an error listening on the socket.
				//
				DBGWRITE(dbg_level_error, "Failed to accept()!\n");
				break;
			}
									
			DBGWRITE(dbg_level_trace, "Connected\n");
			
			if (newSocket)
			{
				sprintf(threadname, "afp_connection[%d]", id++);
				
				ConnectionData* connection_data = new ConnectionData(newSocket, newID, threadname);
				
				newID = spawn_thread(
							ServerConnection,
							threadname,
							B_DISPLAY_PRIORITY,
							connection_data
							);
								
				resume_thread(newID);
			}
			else
			{
				DBGWRITE(dbg_level_error, "Refused new connection (no more memory?)\n");
				shutdown(newSocket, SHUT_RDWR);
			}
		}
		
		shutdown(listenSocket, SHUT_RDWR);
		
	}while(true);
	
	return( B_OK );
}
















