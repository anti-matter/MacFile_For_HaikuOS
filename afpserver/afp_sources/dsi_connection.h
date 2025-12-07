#ifndef __BAFPConnection__
#define __BAFPConnection__

#include <mutex>
#include <memory>
#include <Looper.h>
#include <BlockCache.h>

#include "afp.h"
#include "afp_session.h"

//
//This is the largest request that the server can receive
//from the client. It does not include the DSI header size
//or the AFP command.
//
#define SRVR_REQUEST_QUANTUM_SIZE	(INT16_MAX - DSI_HEADER_SIZE)

#define RECV_BUFFER_SIZE		(UINT16_MAX) 
#define SEND_BUFFER_SIZE		(UINT16_MAX)
#define OVERFLOW_BUFFER_SIZE	4096

//
//AFP over TCP/IP port number
//
#define AFP_TCP_PORT			548

//
//Offsets for the DSI header fields
//
#define DSI_OFFSET_FLAGS		0
#define DSI_OFFSET_COMMAND		1
#define DSI_OFFSET_REQUESTID	2
#define DSI_OFFSET_ERRORCODE	4
#define DSI_OFFSET_DATAOFFSET	4
#define DSI_OFFSET_DATALEN		8
#define DSI_OFFSET_RESERVED		12
#define DSI_OFFSET_DATASTART	16
#define DSI_HEADER_SIZE			16

#define AFP_MAX_CMD_SIZE		512

//For AFP3.2+, Replay cache size
#define AFP_REPLAY_CACHE_SIZE		32

//
//DSIOpen option fields
//
#define kServerRequestQuanta		0x00
#define kServerReplayCacheSize		0x02
#define kClientAttentionQuantum		0x01

//
//DSI commands
//
enum
{
	DSI_CMD_CloseSession	= 1,
	DSI_CMD_Command,
	DSI_CMD_GetStatus,
	DSI_CMD_OpenSession,
	DSI_CMD_Tickle,
	DSI_CMD_Write,
	DSI_CMD_Attention 		= 8
};

//DSI flag possibilities
#define DSI_REQUEST_FLAG	0x00
#define DSI_REPLY_FLAG		0x01

//Maximum request ID
#define DSI_MAX_REQUEST_ID	UINT16_MAX

struct ConnectionData
{
	int socket;
	thread_id threadId;
	char thread_name[64];
	
	ConnectionData(int sock, thread_id th_id, char* th_name) :
		socket(sock),
		threadId(th_id)
	{
		strcpy(thread_name, th_name);
	}
};

class dsi_connection
{
public:
							dsi_connection(int socket, thread_id th_id);
	virtual					~dsi_connection();
			
	virtual void 			Send(
								int8*		sendBuffer,
								int32		sendBufferSize
								);

	virtual void			Receive();
	virtual void			ProcessReceivedBytes();
	virtual void			FormatAndSendRequest(
								int8* 	reqBuffer,
								int8 	dsiCommand,
								int32	afpDataSize,
								int32	reqBufferSize
								);
	virtual void			FormatAndSendReply(
								int8* 			replyBuffer,
								int8 			dsiCommand,
								int32 			afpError,
								int32			afpDataSize,
								bool			fromCache=false
								);
	virtual void			PrepareDSIHeaderForReply(
								int8*			replyBuffer,
								int8 			dsiCommand,
								int32			afpError,
								int32 			afpDataSize
								);
	virtual void			KillSession();
	virtual void			SendAttention(uint16 attnMessage);
	virtual void			SendTickle();

	virtual AFPERROR		dsi_OpenSession(
								int8* 	afpReplyBuffer,
								int32*	afpDataSize
								);
	
	virtual afp_session*	GetAFPSessionObject()		{return mSession.get();}
	virtual int32			GetAttnQuantumSize()		{return mAttentionQuantumSize;}
		
private:
	
	int mSocket;
	thread_id mThreadId;
	int16 mExpectedDSIClientRequestID;
	
	//
	//This is the AFP session associated with this network connection.
	//
	std::unique_ptr<afp_session> mSession;
	bool mSessionOpen;
	
	//
	//This is the buffer we use for recieving data from the client.
	//
	std::unique_ptr<int8[]> mReceiveBuffer;
	size_t mBytesInReceiveBuffer;
	int32 mAttentionQuantumSize;
	
	//
	//The receive thread will continue as long as this is true.
	//
	bool mContinueRecv;
	
	//
	//This BList serves as the replay cache for AFP3.3 and later connections
	//
	std::unique_ptr<BList> mReplayCache;
	
	//
	//Only one send at a time...
	//
	std::mutex mSendMutex;
};

status_t ServerConnection(void* data);


#endif //__BAFPConnection__
