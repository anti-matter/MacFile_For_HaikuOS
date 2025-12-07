#ifndef __afp_buffer__
#define __afp_buffer__

#include <netinet/in.h>
#include "debug.h"
#include "byte_swap.h"
#include "afp.h"

constexpr uchar REPLACE_SLASH_CHAR = 0xb6; //'π';

class afp_buffer
{
public:
	afp_buffer(int8* afpRequestBuffer);
	afp_buffer(int8* afpBuffer, int32 cbbuffer);

	~afp_buffer();

	bool Reallocate(int32 newsize);

	//
	//The following methods are for retrieving data from
	//the AFP buffer.
	//
	AFPERROR	GetString(char* string, uint16 cbstring, bool isPath=false, int8 pathType=kLongNames);

	int8  GetInt8()		{ return pull_num<int8>();  }
	int16 GetInt16()	{ return pull_num<int16>(); }
	int32 GetInt32()	{ return pull_num<int32>(); }
	int64 GetInt64()	{ return pull_num<int64>(); }

	void GetRawData(void* buffer, int16 amount);

	//
	//These methods paste in data into an AFP buffer for sending
	//
	AFPERROR AddCStringAsPascal(char* string);
	AFPERROR AddUniString(char* string, bool isPath=false, bool isAFPName=true);
	AFPERROR GetUniString(char* string, uint16 cbstring, uint16* stringLen);

	bool EnoughSpace(int amountToAdd);

	template <class T>
	void push_num(T value);

	template <class T>
	T pull_num();

	void AddInt8(int8 value)	{ push_num(value); }
	void AddInt16(int16 value)	{ push_num(value); }
	void AddInt32(int32 value)	{ push_num(value); }
	void AddInt64(int64 value)	{ push_num(value); }

	AFPERROR AddRawData(void* data, int16 size);

	void AdvanceIfOdd(void);

	int32 GetDataLength() { return(mCurrentPos-mBuffer); }

	//
	//These are general purpose methods used for both putting and
	//getting of data from the buffer.
	//
	int8* GetCurrentPosPtr()	{ return( mCurrentPos ); }
	void  Advance(int32 amount)	{ mCurrentPos += amount; }
	int8* GetBuffer()			{ return( mBuffer );	 }
	int32 GetBufferSize()		{ return( mBufferSize ); }
	void  Rewind()				{ mCurrentPos = mBuffer; }

private:

	AFPERROR GetPascalString(char* string, uint16 cbstring, int16* sLen);
	AFPERROR GetUnicodeString(char* string, uint16 cbstring, int16* sLen, bool isPath);

	int8* mBuffer;
	int8* mCurrentPos;
	int32 mBufferSize;
};

inline void ConvertIllegalCharsBackToMac(unsigned char* string, int16 cbstring)
{
	int16	i;

	for (i = 0; i < cbstring; i++)
	{
		switch(string[i])
		{
			case REPLACE_SLASH_CHAR:
				string[i] = '/';
				break;

			default:
				break;
		}
	}
}

inline bool afp_buffer::EnoughSpace(int amountToAdd)
{
	if ((mCurrentPos - mBuffer) + amountToAdd > mBufferSize)
	{
		DBGWRITE(dbg_level_error, "Not enough space in buffer!\n");
		return false;
	}

	return true;
}

template <class T>
void afp_buffer::push_num(T value)
{
	if (!EnoughSpace(sizeof(value)))
	{
		return;
	}

	DBGWRITE(dbg_level_dump_out, "Pushing (value,size): %llu, %lu\n", value, sizeof(T));

	if (sizeof(T) > 1)
	{
		*reinterpret_cast<T*>(mCurrentPos) = byte_swap<T>(value);
	}
	else
	{
		*reinterpret_cast<T*>(mCurrentPos) = value;
	}

	Advance(sizeof(T));
}

template <class T>
T afp_buffer::pull_num()
{
	T result;

	if (sizeof(T) > 1)
	{
		result = byte_swap<T>(*reinterpret_cast<T*>(mCurrentPos));
	}
	else
	{
		result = *reinterpret_cast<T*>(mCurrentPos);
	}

	Advance(sizeof(T));

	return result;
}

#endif //__afp_buffer__
