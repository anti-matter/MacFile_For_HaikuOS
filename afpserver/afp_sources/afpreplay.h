#include "dsi_connection.h"

#ifndef __afpreplay__
#define __afpreplay__

typedef struct
{
	int16	requestID;
	int32	replySize;
	int8	reply[SEND_BUFFER_SIZE];
}AFPReplayCacheItem;


void AFPReplayAddReply(
	int16 		dsiRequestID, 
	int8* 		replyBuffer,
	int32		replySize,
	BList*		replayCache
	);

AFPReplayCacheItem* AFPReplaySearchForReply(
	int16	requestID,
	int8	afpCommand,
	BList*	replayCache
	);
	
void AFPReplayEmptyCache(
	BList* 		replayCache
	);

AFPERROR FPSyncDir(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPSyncFork(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
 
 #endif //__afpreplay__
