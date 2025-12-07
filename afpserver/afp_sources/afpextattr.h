#ifndef __afpextattr__
#define __afpextattr__

#include "fp_volume.h"
#include "afp_session.h"
#include "fp_objects.h"

enum {
   kXAttrNoFollow = 0x1,
   kXAttrCreate = 0x2,
   kXAttrReplace = 0x4
};

AFPERROR FPGetExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPSetExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPListExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPRemoveExtAttribute(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

#endif
