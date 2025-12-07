#include <stdio.h>
#include <errno.h>
#include <memory>

#include <OS.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <Autolock.h>

#include "debug.h"
#include "dsi_connection.h"
#include "afp_buffer.h"
#include "dsi_scavenger.h"
#include "dsi_stats.h"
#include "afpreplay.h"
#include "afpdesk.h"

extern dsi_scavenger*	gAFPSessionMgr;
extern dsi_stats		gAFPStats;

/*
 * dsi_connection()
 *
 * Description:
 *		Initialize the afp connection object.
 *
 * Returns: None
 */

dsi_connection::dsi_connection(int socket, thread_id th_id)
{
	DBGWRITE(dbg_level_trace, "Enter\n");

	mSocket 				= socket;
	mThreadId				= th_id;
	mSession				= std::make_unique<afp_session>(this);
	mSessionOpen			= false;
	mReceiveBuffer			= std::make_unique<int8[]>(RECV_BUFFER_SIZE);
	mBytesInReceiveBuffer	= 0;
	mAttentionQuantumSize	= 0;
	mContinueRecv			= true;
	mReplayCache			= std::make_unique<BList>(AFP_REPLAY_CACHE_SIZE);

	gAFPSessionMgr->TrackConnection(this);
}


/*
 * ~dsi_connection()
 *
 * Description:
 *		Initialize the afp connection object.
 *
 * Returns: None
 */

dsi_connection::~dsi_connection()
{
	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//We need to remove the dependancy on the scanvernger thread.
	//
	gAFPSessionMgr->StopTracking(this);

	shutdown(mSocket, SHUT_RDWR);

	AFPReplayEmptyCache(mReplayCache.get());

	DBGWRITE(dbg_level_trace, "Delete completed\n");
}


/*
 * Send()
 *
 * Description:
 *		Make the socket call to send the data on the network to
 *		the remote client.
 *
 * Returns: none
 */

void dsi_connection::Send(
		int8*		sendBuffer,
		int32		sendBufferSize
		)
{
	//Can only send one at a time.
	std::lock_guard<std::mutex> guard(mSendMutex);

	uint32	offset 		= 0;
	int32	bytesSent	= 0;
	int32	bufferSize 	= sendBufferSize;

	if (sendBuffer == NULL)
	{
		//
		//Error: the caller send us a null pointer, bail out now.
		//

		return;
	}

	mSession->SetLastTickleSent();

	do
	{
		bytesSent = send(
						mSocket,
						(uint8*)&sendBuffer[offset],
						bufferSize,
						0
						);

		if (bytesSent < 0)
		{
			if (errno == EWOULDBLOCK)
			{
				continue;
			}
			else {
				break;
			}
		}

		if (bytesSent < (uint32)bufferSize)
		{
			offset += bytesSent;
			bufferSize -= bytesSent;

			continue;
		}

		DBG_DUMP_BUFFER((char*)sendBuffer, bytesSent, dbg_level_dump_out);

		break;

	}while(true);

	if (bytesSent < 0)
	{
		DBGWRITE(dbg_level_error, "send() failed! (%s)\n", GET_BERR_STR(errno));
		KillSession();
	}

	//
	//Keep stats for how many bytes the server has sent.
	//
	gAFPStats.Net_UpdateBytesSent(bytesSent + offset);
}


/*
 * ServerConnection()
 *
 * Description:
 *		This thread is where all the network stuff happens between the
 *		client and this server.
 *
 * Returns: None
 */

status_t ServerConnection(void* data)
{
	auto connection_data = (ConnectionData*)data;
	auto connection	= std::make_unique<dsi_connection>(connection_data->socket, connection_data->threadId);

	//
	//Receive data from the client for all eternity...
	//
	DBGWRITE(dbg_level_trace, "Executing receive loop for %s...\n", connection_data->thread_name);
	connection->Receive();

	delete connection_data;

	DBGWRITE(dbg_level_info, "Thread terminated!!\n");

	return( B_OK );
}


/*
 * Receive()
 *
 * Description:
 *		Receive data from the client until socket is closed.
 *
 * Returns: none
 */

void dsi_connection::Receive()
{
	int32			bytesReceived			= 0;
	int32			afpDataLen				= 0;
	uint32			bytesReceiveInBuffer	= 0;

	if ((mReceiveBuffer == NULL) || (mSession == NULL))
	{
		//
		//We failed to initialize properly, bail!
		//
		return;
	}

	while(mContinueRecv)
	{
		DBGWRITE(dbg_level_trace, "Waiting for recv(), bytes already in buffer: %u\n", bytesReceiveInBuffer);

		fd_set fd;
		struct timeval tv;

		FD_ZERO(&fd);
		FD_SET(mSocket, &fd);

		//Give the scavenger thread enough time to dispose of lost
		//connections gracefully.
		tv.tv_usec = 0;
		tv.tv_sec = SESSION_DEAD_INTERVAL + 10;

		auto select_val = select(mSocket + 1, &fd, NULL, NULL, &tv);
		switch (select_val)
		{
			case 0:
			{
				DBGWRITE(dbg_level_error, "Timeout in select()\n");

				mContinueRecv = false;
				close(mSocket);

				return;
			}

			case -1:
			{
				DBGWRITE(dbg_level_error, "Error in select(): %d (%s)\n", errno, GET_BERR_STR(errno));

				mContinueRecv = false;
				close(mSocket);

				return;
			}

			default:
				//Fall through.
				break;
		}

		bytesReceived = recv(
							mSocket,
							&mReceiveBuffer[bytesReceiveInBuffer],
							RECV_BUFFER_SIZE - bytesReceiveInBuffer,
							0
							);

		if (bytesReceived < 0)
		{
			if (errno != EWOULDBLOCK)
			{
				//
				//We had some random error, it's likely the connection is dead,
				//so we do the safe thing and terminate the connection.
				//
				DBGWRITE(dbg_level_error, "Unexpected error from recv() (errno == %s)\n", GET_BERR_STR(errno));

				//
				//Close the socket down, returning will terminate the thread.
				//
				close(mSocket);

				return;
			}

			DBGWRITE(dbg_level_trace, "EWOULDBLOCK\n");

			continue;
		}

		DBGWRITE(dbg_level_trace, "recv()'d %ld bytes\n", bytesReceived);

		if (bytesReceived == 0 && !mSession->ClientIsSleeping())
		{
			DBGWRITE(dbg_level_warning, "Connection closed by client, or timeout.\n");

			shutdown(mSocket, SHUT_RDWR);
			return;
		}

		//
		//Keep stats of how many bytes we've received.
		//
		gAFPStats.Net_UpdateBytesReceived(bytesReceived);

		bytesReceiveInBuffer += bytesReceived;

		if (bytesReceiveInBuffer < DSI_HEADER_SIZE)
		{
			//
			//If we didn't get a full DSI header, there's nothing
			//we can do with the data we've gotten so far.
			//
			continue;
		}

		afpDataLen 	= ntohl(*(int32*)&mReceiveBuffer[DSI_OFFSET_DATALEN]);

		if (bytesReceiveInBuffer < (size_t)(DSI_HEADER_SIZE + afpDataLen))
		{
			//
			//We have a full DSI header, but we don't have all the
			//afp payload yet. Keep trying...
			//
			continue;
		}

		do
		{
			//
			//OK, we finally have the full DSI header and AFP payload.
			//
			ProcessReceivedBytes();

			//
			//There is another (or part of another) DSI request in the same buffer. Loop
			//through to process this additional data.
			//
			if (bytesReceiveInBuffer > (size_t)(DSI_HEADER_SIZE + afpDataLen))
			{
				bytesReceiveInBuffer -= (DSI_HEADER_SIZE + afpDataLen);

				DBGWRITE(dbg_level_trace, "Processing overflow bytes!!! (%d)\n", bytesReceiveInBuffer);

				memcpy(
					mReceiveBuffer.get(),
					&mReceiveBuffer[DSI_HEADER_SIZE + afpDataLen],
					bytesReceiveInBuffer
					);

				//
				//If we already have a dsi header, see if we already
				//have the entire afp packet.
				//
				if (bytesReceiveInBuffer >= DSI_HEADER_SIZE)
				{
					afpDataLen 	= ntohl(*(int32*)&mReceiveBuffer[DSI_OFFSET_DATALEN]);
				}
			}
			else
			{
				//
				//Zero this sucker out so we start fresh for the next receive.
				//
				bytesReceiveInBuffer = 0;
			}
		}while(bytesReceiveInBuffer >= (size_t)(DSI_HEADER_SIZE + afpDataLen));
	}
}


/*
 * ProcessReceivedBytes()
 *
 * Description:
 *		We have a full DSI header and AFP payload. Process it...
 *
 * Returns: None
 */

void dsi_connection::ProcessReceivedBytes()
{
	int8		dsiFlags, dsiCommand;
	int16		dsiRequestID;
	int32		dsiDataOffset, dsiDataLength;
	int32		afpDataSize		= 0;
	int8		afpVers 		= mSession->GetAFPVersion();
	AFPERROR	afpError 		= AFP_OK;

	DBGWRITE(dbg_level_trace, "Enter\n");

	//
	//Keep track of the number of DSI packets we process.
	//
	gAFPStats.DSI_IncrementPacketsProcessed();

	//
	//Extract the entire 16 byte DSI header from the receive buffer.
	//
	dsiFlags 		= *(int8*)&mReceiveBuffer[DSI_OFFSET_FLAGS];
	dsiCommand		= *(int8*)&mReceiveBuffer[DSI_OFFSET_COMMAND];
	dsiRequestID	= ntohs(*(uint16*)&mReceiveBuffer[DSI_OFFSET_REQUESTID]);
	dsiDataOffset	= ntohl(*(int32*)&mReceiveBuffer[DSI_OFFSET_DATAOFFSET]);
	dsiDataLength	= ntohl(*(int32*)&mReceiveBuffer[DSI_OFFSET_DATALEN]);

	//
	//We've heard from this client, so bump the tickle time.
	//
	mSession->SetLastTickleRecvd();

	if (dsiFlags == DSI_REQUEST_FLAG)
	{
		//
		//We save the expected request ID in a member variable since we'll need it
		//later for storing the reply in the replay cache.
		//
		mExpectedDSIClientRequestID = mSession->GetNextClientRequestID(dsiRequestID);

		if (mExpectedDSIClientRequestID != dsiRequestID)
		{
			//
			//If our session is AFP3.3 or later, check the replay cache, this might be
			//a request from the client to replay a lost reply.
			//
			if (afpVers >= afpVersion33)
			{
				AFPReplayCacheItem* rci = NULL;

				rci = AFPReplaySearchForReply(
									dsiRequestID,
									(int8)mReceiveBuffer[DSI_OFFSET_DATASTART],
									mReplayCache.get()
									);

				if (rci != NULL)
				{
					//
					//We found an item in the cache, now we just resend this reply
					//to the client and exit.
					//
					DBGWRITE(dbg_level_trace, "Matching reply found in the replay cache!!!\n");

					//
					//NOTE: new DSI header information will be written to the beginning of the buffer.
					//
					FormatAndSendReply(
							rci->reply,
							dsiCommand,
							AFP_OK,
							rci->replySize - DSI_HEADER_SIZE,
							true
							);
				}
			}

			//
			//The request ID we got from the client is not the one
			//we were expecting. We've gotten out of sequence!
			//
			DBGWRITE(dbg_level_error, "Bad req ID, expected %d received %d\n", mExpectedDSIClientRequestID, dsiRequestID);

			//
			//Closing the endpoing will force the connection object to delete
			//thereby closing down the sesion entirely.
			//
			KillSession();
			return;
		}

		std::unique_ptr<int8[]> reply_buffer;

		//
		//We need to generate the reply buffer for the afp data to
		//be packed into. Note that we don't reply to tickle requests
		//so we don't allocate a return buffer.
		//
		if (dsiCommand != DSI_CMD_Tickle)
		{
			reply_buffer = std::make_unique<int8[]>(SEND_BUFFER_SIZE);

			if (reply_buffer == NULL)
			{
				DBGWRITE(dbg_level_error, "Failed to allocted reply buffer!\n");
				return;
			}
		}

		//
		//To prevent hacking, we make sure that DSIOpenSession has been called
		//before processing any other AFP commands.
		//
		if (	(!mSessionOpen) &&
				((dsiCommand != DSI_CMD_OpenSession) && (dsiCommand != DSI_CMD_GetStatus))	)
		{
			//
			//If we are in a weird situation and we received a tickle here,
			//we must consider that we don't have a return buffer to send an
			//error to the caller.
			//
			if (dsiCommand != DSI_CMD_Tickle)
			{
				//
				//No session opened, return error to the caller.
				//
				FormatAndSendReply(reply_buffer.get(), dsiCommand, afpParmErr, 0);
			}

			return;
		}

		switch(dsiCommand)
		{
			case DSI_CMD_OpenSession:
				afpError = dsi_OpenSession(
								&reply_buffer[DSI_OFFSET_DATASTART],
								&afpDataSize
								);

				FormatAndSendReply(reply_buffer.get(), dsiCommand, afpError, afpDataSize);
				break;

			case DSI_CMD_CloseSession:
				DBGWRITE(dbg_level_info, "DSICloseSession command received\n");

				mSessionOpen = false;

				FormatAndSendReply(reply_buffer.get(), dsiCommand, AFP_OK, 0);
				KillSession();
				break;

			case DSI_CMD_Tickle:
				DBGWRITE(dbg_level_trace, "Tickle received\n");
				break;

			case DSI_CMD_Command:

				switch((uint8)mReceiveBuffer[DSI_OFFSET_DATASTART])
				{
					//
					//For performance reasons, we handle read calls from here
					//in the DSI layer. This saves the overhead of the FPDispatch()
					//routine for this high performance function.
					//

					case afpRead:
					case afpReadExt:
						afpError = FPRead(
										mSession.get(),
										&mReceiveBuffer[DSI_OFFSET_DATASTART],
										&reply_buffer[DSI_OFFSET_DATASTART],
										&afpDataSize
										);

						FormatAndSendReply(reply_buffer.get(), dsiCommand, afpError, afpDataSize);
						break;

					default:
						afpError = FPDispatchCommand(
										mSession.get(),
										&mReceiveBuffer[DSI_OFFSET_DATASTART],
										&reply_buffer[DSI_OFFSET_DATASTART],
										&afpDataSize
										);

						FormatAndSendReply(reply_buffer.get(), dsiCommand, afpError, afpDataSize);

						if ((!mSession->IsAuthenticated()) && (afpError != afpAuthContinue))
						{
							DBGWRITE(dbg_level_trace, "Authentication gone, killing session\n");
							KillSession();
						}
						break;
				}
				break;

			case DSI_CMD_GetStatus:
				DBGWRITE(dbg_level_trace, "DSIGetStatus command received\n");

				//
				//Call the AFP function directly that packs the server
				//info data.
				//
				afpError = FPGetSrvrInfo(
								mSession.get(),
								&mReceiveBuffer[DSI_OFFSET_DATASTART],
								&reply_buffer[DSI_OFFSET_DATASTART],
								&afpDataSize
								);

				FormatAndSendReply(reply_buffer.get(), dsiCommand, afpError, afpDataSize);

				//
				//Per the AFP spec, we close the connection immediately following
				//the GetStatus call.
				//
				KillSession();
				break;

			case DSI_CMD_Write:
				//
				//For performance reasons, writes are handles directly from the
				//DSI layer. This is as per the DSI specification from Apple.
				//

				switch((uint8)mReceiveBuffer[DSI_OFFSET_DATASTART])
				{
					case afpWrite:
					case afpWriteExt:
						afpError = FPWrite(
										mSession.get(),
										&mReceiveBuffer[DSI_OFFSET_DATASTART],
										&reply_buffer[DSI_OFFSET_DATASTART],
										&afpDataSize
										);
						break;

					case afpAddIcon:
						afpError = FPAddIcon(
										mSession.get(),
										&mReceiveBuffer[DSI_OFFSET_DATASTART],
										&reply_buffer[DSI_OFFSET_DATASTART],
										&afpDataSize
										);
						break;

					default:
						ASSERT(0);
						break;
				}

				FormatAndSendReply(reply_buffer.get(), dsiCommand, afpError, afpDataSize);
				break;

			default:
				DBGWRITE(dbg_level_error, "Bad DSI command from client (%d)\n", dsiCommand);
				break;
		}
	}
	else if (dsiFlags == DSI_REPLY_FLAG)
	{
		//
		//We have a reply to a request we sent to the client.
		//
	}
	else
	{
		//
		//Big time problems, this isn't a request or a reply. We have
		//no choice but to bail.
		//
		DBGWRITE(dbg_level_trace, "Bad DSI type from client (%d)\n", dsiFlags);
		KillSession();
	}
}


/*
 * FormatAndSendRequest()
 *
 * Description:
 *		Format the DSI header and send a request to the
 *		client. This is usually a tickle request.
 *
 * Returns: None
 */

void dsi_connection::FormatAndSendRequest(
	int8* 	reqBuffer,
	int8 	dsiCommand,
	int32	afpDataSize,
	int32	reqBufferSize
	)
{
	if (reqBuffer == NULL)
	{
		return;
	}

	afp_buffer	request(reqBuffer);

	request.AddInt8(DSI_REQUEST_FLAG);
	request.AddInt8(dsiCommand);
	request.AddInt16(mSession->GetNextServerRequestID());
	request.AddInt32(0);
	request.AddInt32(afpDataSize);
	request.AddInt32(0);

	Send(reqBuffer, reqBufferSize);
}


/*
 * FormatAndSendReply()
 *
 * Description:
 *		Format the DSI header and send the payload data back
 *		to the client.
 *
 * Returns: None
 */

void dsi_connection::FormatAndSendReply(
	int8* 	replyBuffer,
	int8 	dsiCommand,
	int32 	afpError,
	int32	afpDataSize,
	bool	fromCache
	)
{
	PrepareDSIHeaderForReply(
			replyBuffer,
			dsiCommand,
			afpError,
			afpDataSize
			);

	//Add this reply to the replay cache for AFP3.3
	if ((!fromCache) && (mSession->GetAFPVersion() >= afpVersion33) &&
		((dsiCommand == DSI_CMD_Command) || (dsiCommand == DSI_CMD_Write)))
	{
		AFPReplayAddReply(
				mExpectedDSIClientRequestID,
				replyBuffer,
				DSI_HEADER_SIZE+afpDataSize,
				mReplayCache.get()
				);
	}

	//DBG_DUMP_BUFFER((char*)replyBuffer, DSI_HEADER_SIZE+afpDataSize, dbg_level_trace);

	Send(replyBuffer, DSI_HEADER_SIZE+afpDataSize);
}


/*
 * PrepareDSIHeaderForReply()
 *
 * Description:
 *		Prepare the DSI header in the buffer for a reply.
 *
 * Returns: None
 */

void dsi_connection::PrepareDSIHeaderForReply(
	int8* 			replyBuffer,
	int8 			dsiCommand,
	int32			afpError,
	int32 			afpDataSize
	)
{
	//
	//We're replying, set the correct flag and command we're
	//replying to.
	//
	*((int8*)&replyBuffer[DSI_OFFSET_FLAGS])		= DSI_REPLY_FLAG;
	*((int8*)&replyBuffer[DSI_OFFSET_COMMAND])		= dsiCommand;

	//
	//Insert the request ID and error code into the header.
	//
	*((int16*)&replyBuffer[DSI_OFFSET_REQUESTID])	= htons(mSession->GetCurrentRequestID());
	*((int32*)&replyBuffer[DSI_OFFSET_ERRORCODE])	= htonl(afpError);

	//
	//Add the size of the AFP data we're including.
	//
	*((int32*)&replyBuffer[DSI_OFFSET_DATALEN])		= htonl(afpDataSize);

	//
	//This is the reserved field in the header, it must be zero.
	//
	*((int32*)&replyBuffer[DSI_OFFSET_RESERVED])	= 0;

	DBGWRITE(dbg_level_trace, "Client request id: %lu\n", htons(*((int16*)&replyBuffer[DSI_OFFSET_REQUESTID])));
}


/*
 * KillSession()
 *
 * Description:
 *		Post a message to tell this object to kill the session with
 *		the network client.
 *
 * Returns: None
 */

void dsi_connection::KillSession()
{
	DBGWRITE(dbg_level_info, "Killing connection...\n");

	if (mContinueRecv)
	{
		//
		//Signal the connection thread to shutdown
		//
		mContinueRecv = false;

		close(mSocket);
		shutdown(mSocket, SHUT_RDWR);
	}
}


/*
 * SendAttention()
 *
 * Description:
 *		Sends an attention message to the client.
 *
 * Returns: none
 */

void dsi_connection::SendAttention(uint16 attnMessage)
{
	auto buffer = std::make_unique<int8[]>(DSI_HEADER_SIZE + sizeof(int16));

	if (buffer != NULL)
	{
		DBGWRITE(dbg_level_info, "Sending attention (0x%x)\n", attnMessage);

		//
		//Attetion messages are simple 2 bytes parameters. We just
		//manually blast it into our buffer at the data offset.
		//

		*((int16*)&buffer[DSI_OFFSET_DATASTART]) = htons(attnMessage);

		//
		//Call the worker routine that will format and send off our
		//attention message to the client.
		//

		FormatAndSendRequest(
				buffer.get(),
				DSI_CMD_Attention,
				sizeof(int16),
				DSI_HEADER_SIZE + sizeof(int16)
				);
	}
}


/*
 * SendTickle()
 *
 * Description:
 *		Sends an attention message to the client.
 *
 * Returns: none
 */

void dsi_connection::SendTickle()
{
	auto buffer = std::make_unique<int8[]>(DSI_HEADER_SIZE);

	FormatAndSendRequest(
			buffer.get(),
			DSI_CMD_Tickle,
			0,
			DSI_HEADER_SIZE
			);
}


/*
 * dsi_OpenSession()
 *
 * Description:
 *		Performs the DSI Open session command.
 *
 * Returns: None
 */

AFPERROR dsi_connection::dsi_OpenSession(
	int8* 	afpReplyBuffer,
	int32*	afpDataSize
	)
{
	afp_buffer	afpReply(afpReplyBuffer);
	int8		option;
	int8		option_size;

	DBGWRITE(dbg_level_trace, "DSIOpenSession command received\n");

	option		= mReceiveBuffer[DSI_OFFSET_DATASTART];
	option_size	= mReceiveBuffer[DSI_OFFSET_DATASTART+sizeof(int8)];

	//
	//We expect these options to have the size of a long.
	//
	if (option_size == sizeof(int32))
	{
		switch(option)
		{
			case kClientAttentionQuantum:
				mAttentionQuantumSize = ntohl(*((int32*)&mReceiveBuffer[DSI_OFFSET_DATASTART+sizeof(int16)]));
				DBGWRITE(dbg_level_info, "Attention size = %lu\n", mAttentionQuantumSize);
				break;

			default:
				ASSERT(0);
				break;
		}
	}

	//Include the server request quanta option
	afpReply.AddInt8(kServerRequestQuanta);
	afpReply.AddInt8(sizeof(int32));
	afpReply.AddInt32(SRVR_REQUEST_QUANTUM_SIZE);

	//Include the replay cache size option
	afpReply.AddInt8(kServerReplayCacheSize);
	afpReply.AddInt8(sizeof(int32));
	afpReply.AddInt32(AFP_REPLAY_CACHE_SIZE);

	*afpDataSize = afpReply.GetDataLength();

	mSessionOpen = true;

	return( AFP_OK );
}



