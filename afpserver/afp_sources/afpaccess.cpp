#include <StorageKit.h>

#include "debug.h"
#include "afpGlobals.h"
#include "afpaccess.h"
#include "fp_objects.h"


/*
 * afpAccessCheck()
 *
 * Description:
 *		Checks to see if this session has the desired access to a
 *		file object.
 *
 * Returns: AFPERROR
 */
 
AFPERROR afpAccessCheck(
	afp_session*	afpSession,
	BEntry*			afpEntry,
	int8			afpAccess
)
{
	BDirectory	parent;
	mode_t		posixPerms		= 0;
	AFPERROR	afpResult		= afpAccessDenied;

	if (	(afpSession == NULL)
				|| (afpEntry == NULL)
				|| (afpSession->IsAuthenticated() == false)	)
	{
		//
		//We need these parameters to do a valid access check. We also need
		//the session to actually have been authenticated.
		//
		
		DBGWRITE(dbg_level_warning, "User is not authenticated\n");
		return( afpParmErr );
	}

	if ((afpAccess < afpAccessRead) || (afpAccess > afpAccessSearch))
	{
		//
		//No supported flag was set, pass back parameter error to the caller.
		//
		
		DBGWRITE(dbg_level_warning, "Bad access flags\n");
		return( afpParmErr );
	}
	
	//
	//If the afpEntry is itself a directory, we get the permissions
	//for that directory instead of the parent.
	//
	if (afpEntry->IsDirectory())
	{
		BDirectory dir;
		
		dir.SetTo(afpEntry);
		dir.GetPermissions(&posixPerms);
	}
	else {
		
		afpEntry->GetParent(&parent);
		parent.GetPermissions(&posixPerms);
	}
	
	if (afpSession->IsAdmin())
	{
		//
		//Administrators always get full access to everything.
		//
			
		afpResult = AFP_OK;
		
		DBGWRITE(dbg_level_trace, "Admin access check returning: %d\n", afpResult);
	}
	else if (afpSession->IsUser())
	{
		switch(afpAccess)
		{
			case afpAccessSearch:
				afpResult = (posixPerms & POSIX_AFP_ISUSER) ? AFP_OK : afpAccessDenied;
				break;
		
			case afpAccessRead:
				afpResult = (posixPerms & POSIX_AFP_IRUSER) ? AFP_OK : afpAccessDenied;
				break;
			
			case afpAccessWrite:		
				afpResult = (posixPerms & POSIX_AFP_IWUSER) ? AFP_OK : afpAccessDenied;
				break;
			
			default:
				break;
		}
		
		DBGWRITE(dbg_level_trace, "User access check returning: %d\n", afpResult);
	}
	else //guest
	{
		switch(afpAccess)
		{
			case afpAccessSearch:
				afpResult = (posixPerms & POSIX_AFP_ISGUEST) ? AFP_OK : afpAccessDenied;
				break;
		
			case afpAccessRead:
				afpResult = (posixPerms & POSIX_AFP_IRGUEST) ? AFP_OK : afpAccessDenied;
				break;
			
			case afpAccessWrite:		
				afpResult = (posixPerms & POSIX_AFP_IWGUEST) ? AFP_OK : afpAccessDenied;
				break;
			
			default:
				break;
		}
		
		DBGWRITE(dbg_level_trace, "Guest access check returning: %d\n", afpResult);
	}
	
	return( afpResult );
}


/*
 * afpCheckSearchAccess()
 *
 * Description:
 *		Checks to see if this session has read access on a volume and/or
 *		directory.
 *
 * Returns: AFPERROR
 */

AFPERROR afpCheckSearchAccess(
	afp_session*	afpSession,
	BEntry*			afpEntry
)
{
	return( afpAccessCheck(afpSession, afpEntry, afpAccessSearch) );
}

/*
 * afpCheckReadAccess()
 *
 * Description:
 *		Checks to see if this session has read access on a volume and/or
 *		directory.
 *
 * Returns: AFPERROR
 */

AFPERROR afpCheckReadAccess(
	afp_session*	afpSession,
	BEntry*			afpEntry
)
{	
	return( afpAccessCheck(afpSession, afpEntry, afpAccessRead) );
}


/*
 * afpCheckWriteAccess()
 *
 * Description:
 *		Checks to see if this session's user has write access on a volume
 *		and/or directory.
 *
 * Returns: AFPERROR
 */

AFPERROR afpCheckWriteAccess(
	afp_session*	afpSession,
	fp_volume*		afpVolume,
	BEntry*			afpEntry
)
{
	int16	afpAttributes = 0;
	
	if (afpVolume != NULL)
	{
		//
		//Check to make sure we are allowed to write on the volume.
		//
		if ((afpVolume->GetVolumeFlags() & kAFPReadOnly) != 0)
		{
			//
			//The volume we're attempting to write to is read only.
			//
			DBGWRITE(dbg_level_warning, "Volume is locked! (%s)\n", afpVolume->GetVolumeName());
			return( afpVolLocked );
		}
	}
	
	return( afpAccessCheck(afpSession, afpEntry, afpAccessWrite) );
}

