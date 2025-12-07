#ifndef __afpaccess__
#define __afpaccess__

#include "afpGlobals.h"
#include "fp_volume.h"
#include "afp_session.h"
#include "fp_objects.h"

enum
{
	afpAccessRead,
	afpAccessWrite,
	afpAccessSearch
};

AFPERROR afpAccessCheck(
	afp_session*	afpSession,
	BEntry*			afpEntry,
	int8			afpAccess
);

AFPERROR afpCheckSearchAccess(
	afp_session*	afpSession,
	BEntry*			afpEntry
);

AFPERROR afpCheckReadAccess(
	afp_session*	afpSession,
	BEntry*			afpEntry
);

AFPERROR afpCheckWriteAccess(
	afp_session*	afpSession,
	fp_volume*		afpVolume,
	BEntry*			afpEntry
);


#endif
