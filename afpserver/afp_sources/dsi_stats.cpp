#include "dsi_stats.h"


/*
 * dsi_stats()
 *
 * Description:
 *		Initialize the stats object.
 *
 * Returns: None
 */

dsi_stats::dsi_stats()
{
	mBytesSent				= 0;
	mBytesReceived			= 0;
	
	mTimeHack				= real_time_clock();
	mTimeHackSentBytes		= 0;
	mTimeHackRecvBytes		= 0;
	mLastBPS				= 0;
	
	mDSIPacketsProcessed	= 0;
}


/*
 * ~dsi_stats()
 *
 * Description:
 *		Free the stats object.
 *
 * Returns: None
 */

dsi_stats::~dsi_stats()
{
}


/*
 * Net_UpdateBytesSent()
 *
 * Description:
 *		Update the number of bytes sent by the network objects.
 *
 * Returns: None
 */

void dsi_stats::Net_UpdateBytesSent(uint32 inBytesSent)
{
	mBytesSent += inBytesSent;
}


/*
 * Net_UpdateBytesReceived()
 *
 * Description:
 *		Update the number of bytes received by the network objects.
 *
 * Returns: None
 */

void dsi_stats::Net_UpdateBytesReceived(uint32 inBytesReceived)
{
	mBytesReceived += inBytesReceived;
}


/*
 * Net_BytesPerSecond()
 *
 * Description:
 *		Calculate and return the number of bytes per second the
 *		the network objects are processing per second.
 *
 *		NOTE: Don't call this more than once a second or the
 *		results will be inaccurate.
 *
 * Returns: None
 */

int32 dsi_stats::Net_BytesPerSecond()
{
	int32		result		= 0;
	bigtime_t	currentTime = real_time_clock();
	int32		elapsedTime	= currentTime - mTimeHack;
	int64		sendBPS		= (mBytesSent - mTimeHackSentBytes);
	int64		recvBPS		= (mBytesReceived - mTimeHackRecvBytes);
		
	//
	//Protect against divide by zero errors.
	//
	if (elapsedTime <= 0) {
		
		return( mLastBPS );
	}
	
	result 		= (sendBPS + recvBPS) / elapsedTime;
	mLastBPS	= result;
	
	//
	//Reset the counters for the next time we're called.
	//
	mTimeHack 			= currentTime;
	mTimeHackSentBytes	= mBytesSent;
	mTimeHackRecvBytes	= mBytesReceived;

	return( result );
}


/*
 * Stat_BeginOperation()
 *
 * Description:
 *		Call this function just before an operation begins. This
 *		is one part of a system to track the cost of an operation (e.g.
 *		reading from the filesystem. Be sure to call Stat_EndOperation()
 *		immediately upon read operation completion to get the correct
 *		calculation.
 *
 * Returns: None
 */

void dsi_stats::Stat_BeginOperation()
{
	mLastOpTime		= 0;
	mOpStartTime	= real_time_clock_usecs();
}


/*
 * Stat_EndOperation()
 *
 * Description:
 *		See Stat_BeginOperation().
 *
 * Returns: None
 */

void dsi_stats::Stat_EndOperation()
{
	bigtime_t	currentTime = real_time_clock_usecs();
	mLastOpTime = currentTime - mOpStartTime;
}






