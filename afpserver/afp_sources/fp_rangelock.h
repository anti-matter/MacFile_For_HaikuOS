#ifndef __fp_rangelock__
#define __fp_rangelock__

#include "afp.h"
#include "afp_session.h"


class fp_rangelock
{
public:
						fp_rangelock(OPEN_FORK_ITEM* forkItem);
	virtual				~fp_rangelock();
	
	static bool			RangeLocked(
							off_t 		rangeStart,
							off_t 		rangeEnd,
							BEntry* 	entry
							);
	
	static fp_rangelock*	SessionRangeLocked(
							off_t 				rangeStart,
							off_t 				rangeLength,
							OPEN_FORK_ITEM* 	forkItem
							);
	
	virtual AFPERROR	Lock(off_t offset, off_t len);
	virtual void		GetLockRange(off_t* start, off_t* end);
	virtual uint16		GetForkRef()		{ return mForkRef->refnum; }
	virtual BEntry*		GetLockEntry()		{ return mEntry;  }
	
private:

	BEntry*			mEntry;
	off_t			mStart;
	off_t			mEnd;
	OPEN_FORK_ITEM*	mForkRef;
};

#endif //__fp_rangelock__
