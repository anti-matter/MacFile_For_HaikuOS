#include "List.h"
#include "Entry.h"
#include "fp_rangelock.h"

BList	gLockList;

/*
 * fp_rangelock()
 *
 * Description:
 *
 * Returns:
 */

fp_rangelock::fp_rangelock(OPEN_FORK_ITEM* forkItem)
{
	mStart		= -1;
	mEnd		= -1;
	
	gLockList.AddItem(this);
	
	//
	//We add this locker object to the forkItem struct so
	//it can be referenced on a per-fork basis.
	//
	mForkRef 	= forkItem;
	mEntry		= forkItem->entry;
	
	forkItem->brlList->AddItem(this);
}


/*
 * ~fp_rangelock()
 *
 * Description:
 *
 * Returns:
 */

fp_rangelock::~fp_rangelock()
{
	gLockList.RemoveItem(this);
	mForkRef->brlList->RemoveItem(this);
}


/*
 * RangeLocked() [STATIC]
 *
 * Description:
 *		Returns whether or not the range provided is already locked by 
 *		another session. There can be no overlapping locks and we return
 *		true even if only part of the requested range is locked by another
 *		user.
 *
 * Returns:
 *		TRUE if the range is locked, FALSE otherwise.
 */

bool fp_rangelock::RangeLocked(
	off_t 		rangeStart,
	off_t 		rangeEnd,
	BEntry* 	entry
	)
{
	bool		result	= false;
	int			i		= 0;
	fp_rangelock*	lock	= NULL;
	
	while((lock = (fp_rangelock*)gLockList.ItemAt(i)) != NULL)
	{
		off_t	start	= 0;
		off_t	end		= 0;
		
		if (*lock->GetLockEntry() == *entry)
		{
			lock->GetLockRange(&start, &end);
						
			if (((rangeStart >= start) && (rangeStart <= end))	||
				((rangeEnd >= start) && (rangeEnd <= end))		||
				((rangeStart <= start) && (rangeEnd >= end))	)
			{
				//
				//We fall within the lock range, so we'll return TRUE.
				//
				result = true;
				break;
			}
		}
		
		i++;
	}
	
	return( result );
}


/*
 * SessionRangeLocked() [STATIC]
 *
 * Description:
 *		Returns whether or not the range provided is locked by a session.
 *		There can be no overlapping locks and we return true even if only
 *		part of the requested range is locked by another user.
 *
 * Returns:
 *		TRUE if the range is locked, FALSE otherwise.
 */

fp_rangelock* fp_rangelock::SessionRangeLocked(
	off_t 				rangeStart,
	off_t 				rangeLength,
	OPEN_FORK_ITEM* 	forkItem
	)
{
	fp_rangelock*	result	= NULL;
	fp_rangelock*	lock	= NULL;
	int			i		= 0;
	
	while((lock = (fp_rangelock*)forkItem->brlList->ItemAt(i)) != NULL)
	{
		off_t	start	= 0;
		off_t	end		= 0;
		
		lock->GetLockRange(&start, &end);
		
		if ((rangeStart == start) && (rangeLength == ((end-start)+1)))
		{
			//
			//We fall within the lock range, so we'll return TRUE.
			//
			result = lock;
			break;
		}
		
		i++;
	}
	
	return( result );
}


/*
 * Lock()
 *
 * Description:
 *
 * Returns:
 */

AFPERROR fp_rangelock::Lock(off_t offset, off_t len)
{
	off_t		rangeStart	= offset;
	off_t		rangeEnd	= (offset + (len-1));
	
	if (RangeLocked(rangeStart, rangeEnd, mForkRef->entry))
	{
		return( afpLockErr );
	}
	else
	{
		mStart	= rangeStart;
		mEnd	= rangeEnd;
	}
	
	return( AFP_OK );
}


/*
 * GetLockRange()
 *
 * Description:
 *
 * Returns:
 */

void fp_rangelock::GetLockRange(off_t* start, off_t* end)
{
	*start	= mStart;
	*end	= mEnd;
}












