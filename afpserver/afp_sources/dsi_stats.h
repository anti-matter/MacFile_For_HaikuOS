#ifndef __dsi_stats__
#define __dsi_stats__

#include <OS.h>

typedef enum
{
	READ_OPERATION	= 1,
	WRITE_OPERATION,
	NET_OPERATION,
	GENERIC_OPERATION
}STAT_OPERATION;

class dsi_stats
{
public:
						dsi_stats();
	virtual				~dsi_stats();
	
	virtual void		Net_UpdateBytesSent(uint32 inBytesSent);
	virtual void		Net_UpdateBytesReceived(uint32 inBytesReceived);
	virtual int32		Net_BytesPerSecond();
	
	virtual int64		Net_BytesSent()		{ return mBytesSent; 		}
	virtual int64		Net_BytesRecv()		{ return mBytesReceived;	}
	
	virtual void		DSI_IncrementPacketsProcessed()	{ mDSIPacketsProcessed++; }
	virtual uint32		DSI_NumPacketsProcessed()		{ return mDSIPacketsProcessed; }
	
	virtual void 		Stat_BeginOperation();
	virtual void 		Stat_EndOperation();
	virtual uint32		Stat_LastOpTime()				{return mLastOpTime;}
	
private:
	//
	//Track the raw transfered bytes to and from the server and
	//all AFP clients.
	//
	int64				mBytesSent;
	int64				mBytesReceived;
	
	//
	//Saves the last time we took a snap shot of the network time.
	//
	uint32				mTimeHack;
	uint64				mTimeHackSentBytes;
	uint64				mTimeHackRecvBytes;
	int32				mLastBPS;
	
	//
	//Keep track of the DSI packets we process.
	//
	uint32				mDSIPacketsProcessed;
	
	//
	//Time read operations
	//
	bigtime_t			mOpStartTime;
	uint32				mLastOpTime;
};



#endif //__dsi_stats__
