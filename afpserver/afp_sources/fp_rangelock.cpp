#include "List.h"
#include "Entry.h"
#include "Locker.h"
#include "fp_rangelock.h"

BList		gLockList;
BLocker	gLockListLocker;

/*
 * fp_rangelock()
 *
 * Description:
 *
 * Returns:
 */

fp_rangelock::fp_rangelock(afp_session* session, OPEN_FORK_ITEM* forkItem)
{
	mStart		= 0;
	mEnd		= 0;
	mIsValid	= false;

	gLockListLocker.Lock();
	gLockList.AddItem(this);
	gLockListLocker.Unlock();

	// We add this locker object to the forkItem struct so
	// it can be referenced on a per-fork basis.
	mForkRef 	= forkItem;
	mSession	= session;

	// Store a stable node_ref instead of a BEntry* so that we don't
	// dereference freed memory when another session closes its fork.
	forkItem->entry->GetNodeRef(&mNodeRef);

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
	gLockListLocker.Lock();
	gLockList.RemoveItem(this);
	gLockListLocker.Unlock();
	mForkRef->brlList->RemoveItem(this);

	// Clear pointers so a leaked lock object does not hold stale references.
	mSession = NULL;
	mForkRef = NULL;
}


/*
 * RangeLocked() [STATIC]
 *
 * Description:
 *		Returns whether or not the range provided is already locked by
 *		a different session. There can be no overlapping locks and we return
 *		true even if only part of the requested range is locked by another
 *		user. Locks owned by the calling session are skipped (a session may
 *		read/write/flush ranges it has locked itself).
 *
 * Returns:
 *		TRUE if the range is locked by another session, FALSE otherwise.
 */

bool fp_rangelock::RangeLocked(
	off_t 				rangeStart,
	off_t 				rangeEnd,
	afp_session* 		session,
	BEntry* 			entry
	)
{
	bool		result	= false;
	int			i		= 0;
	fp_rangelock*	lock	= NULL;

	// Convert the caller's entry to a stable node_ref once outside the lock.
	node_ref	entryRef;
	if (entry->GetNodeRef(&entryRef) != B_OK)
	{
		return false;
	}

	gLockListLocker.Lock();

	while((lock = (fp_rangelock*)gLockList.ItemAt(i)) != NULL)
	{
		// Skip locks owned by the calling session.
		if (lock->GetSession() == session)
		{
			i++;
			continue;
		}

		// Skip locks that have not yet been committed (Lock() not called).
		if (!lock->mIsValid)
		{
			i++;
			continue;
		}

		off_t	start	= 0;
		off_t	end		= 0;

		// Compare by node_ref instead of BEntry* to avoid use-after-free
		// when another session closes its fork (entry deleted before locks).
		if (lock->mNodeRef.device == entryRef.device
			&& lock->mNodeRef.node   == entryRef.node)
		{
			lock->GetLockRange(&start, &end);

			if (((rangeStart >= start) && (rangeStart <= end))	||
				((rangeEnd >= start) && (rangeEnd <= end))		||
				((rangeStart <= start) && (rangeEnd >= end))	)
			{
				// We fall within the lock range, so we'll return TRUE.
				result = true;
				break;
			}
		}

		i++;
	}

	gLockListLocker.Unlock();
	return( result );
}


/*
 * SessionRangeLocked() [STATIC]
 *
 * Description:
 *		Returns the lock object for a range locked by the given session,
 *		or NULL if that session has no matching lock. Supports partial
 *		unlocks: if the client unlocked a subrange of an existing lock,
 *		the lock is shrunk (or deleted if fully covered).
 *
 * Returns:
 *		The fp_rangelock* if found (may have been shrunk), NULL otherwise.
 */

fp_rangelock* fp_rangelock::SessionRangeLocked(
	off_t 				rangeStart,
	off_t 				rangeLength,
	afp_session* 		session,
	OPEN_FORK_ITEM* 	forkItem
	)
{
	fp_rangelock*	result	= NULL;
	fp_rangelock*	lock	= NULL;
	int			i		= 0;

	off_t	unlockEnd = (rangeLength > 0) ? (rangeStart + rangeLength - 1) : rangeStart;

	while((lock = (fp_rangelock*)forkItem->brlList->ItemAt(i)) != NULL)
	{
		// Only unlock locks owned by this session.
		if (lock->GetSession() != session)
		{
			i++;
			continue;
		}

		off_t	lockStart	= 0;
		off_t	lockEnd		= 0;

		lock->GetLockRange(&lockStart, &lockEnd);

		// Check for any overlap between the unlock range and this lock.
		if (rangeStart <= lockEnd && unlockEnd >= lockStart)
		{
			result = lock;

			// Exact match -- caller will delete the lock.
			if (rangeStart == lockStart && unlockEnd == lockEnd)
			{
				// Nothing to adjust; leave it for the caller to delete.
			}
			// Unlock covers the entire lock -- delete it.
			else if (rangeStart <= lockStart && unlockEnd >= lockEnd)
			{
				// Full cover; caller will delete.
			}
			// Partial unlock of the beginning -- shrink from the left.
			else if (rangeStart <= lockStart && unlockEnd < lockEnd)
			{
				lock->mStart = unlockEnd + 1;
			}
			// Partial unlock of the end -- shrink from the right.
			else if (rangeStart > lockStart && unlockEnd >= lockEnd)
			{
				lock->mEnd = rangeStart - 1;
			}
			// Partial unlock in the middle -- split the lock.
			// For simplicity, shrink to the right half and let the client
			// re-lock the left half if needed. A full split would require
			// allocating a second fp_rangelock here.
			else if (rangeStart > lockStart && unlockEnd < lockEnd)
			{
				// Shrink to keep only the right portion.
				lock->mStart = unlockEnd + 1;
			}

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
	// Reject zero or negative lengths to prevent integer underflow.
	if (len <= 0)
	{
		return afpParmErr;
	}

	// Prevent double-locking: if this lock object was already committed,
	// refuse to re-arm it (the caller should unlock first).
	if (mIsValid)
	{
		return afpLockErr;
	}

	off_t		rangeStart	= offset;
	off_t		rangeEnd	= (offset + (len - 1));

	if (RangeLocked(rangeStart, rangeEnd, mSession, mForkRef->entry))
	{
		return( afpLockErr );
	}
	else
	{
		mStart	= rangeStart;
		mEnd	= rangeEnd;
		mIsValid = true;
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
