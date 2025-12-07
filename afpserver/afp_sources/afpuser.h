#ifndef __afpuser__
#define __afpuser__

#include "afp.h"

int8 afpGetUAMType(
	char* uamString
);

int8 afpGetAFPVersion(
	char* afpString
);

AFPERROR FPLogin(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPContLogin(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPLogout(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPChangePswd(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);


#endif //__afpuser__
