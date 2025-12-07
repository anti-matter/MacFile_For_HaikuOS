#ifndef __afpmsg__
#define __afpmsg__

#include "afpGlobals.h"
#include "afp.h"
#include "afp_session.h"

AFPERROR FPGetServerMessage(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

void afp_Initialize(void);
status_t afp_SetLogonMessage(const char* logonMsg);
status_t afp_SetServerMessage(const char* serverMsg);

#endif //__afpmsg__
