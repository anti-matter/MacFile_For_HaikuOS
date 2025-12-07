#include <string.h>
#include <UTF8.h>
#include <NetDebug.h>

#include "afp_buffer.h"

/*
 * afp_buffer()
 *
 * Description:
 *		Initializer for afp buffer class.
 *
 * Returns: None
 */

afp_buffer::afp_buffer(int8* afpBuffer)
{
	mBuffer		= afpBuffer;
	mCurrentPos	= afpBuffer;
	mBufferSize	= 0x7FFFFFFF;
}


afp_buffer::afp_buffer(int8* afpBuffer, int32 cbbuffer)
{
	mBuffer		= afpBuffer;
	mCurrentPos	= afpBuffer;
	mBufferSize	= cbbuffer;
}


/*
 * ~afp_buffer()
 *
 * Description:
 *		Destructor for afp buffer class.
 *
 * Returns: None
 */

afp_buffer::~afp_buffer()
{
}


/*
 * Reallocate()
 *
 * Description:
 *		Reallocate the buffer, usually because it needs to be
 *		bigger. This call kills anything currently in the buffer!
 *
 * Returns: None
 */

bool afp_buffer::Reallocate(int32 newsize)
{
	free( mBuffer );
	
	mBuffer		= NULL;
	mCurrentPos	= NULL;
	mBufferSize	= 0;
	
	mBuffer = (int8*)malloc(newsize);
	
	if (mBuffer != NULL)
	{
		mCurrentPos	= mBuffer;
		mBufferSize	= newsize;
		
		return( true );
	}
	
	return( false );
}


/*
 * GetString()
 *
 * Description:
 *		Extracts a pascal style or unicode string from the afp buffer and
 *		returns it as a c-style string (null terminated). This function also
 *		advances the current buffer pointer to the end of the string
 *		and the next item in the buffer.
 *
 * Returns: None
 */

AFPERROR afp_buffer::GetString(
	char* 			string,
	uint16 			cbstring,
	bool 			isPath,
	int8 			pathType
	)
{
	AFPERROR	afpError	= AFP_OK;
	int16		stringLen	= 0;
	
	switch(pathType)
	{
		case kShortNames:
			afpError = afpCallNotSupported;
			break;
			
		case kLongNames:
			afpError = GetPascalString(string, cbstring, &stringLen);
			break;
			
		case kUnicodeNames:			
			afpError = GetUnicodeString(string, cbstring, &stringLen, isPath);
			break;
		
		default:
			DPRINT(("[afp:GetString]Bad string type! (%d)\n", pathType));
			afpError = afpParmErr;
			break;
	}
	
	if ((AFP_SUCCESS(afpError)) && (isPath))
	{
		//
		//If we're dealing with an AFP pathname, then we need to convert
		//the path separators to Be style ones.
		//
		uint8	i;
		
		for (i = 0; i < stringLen; i++)
		{
			switch(string[i])
			{
				//
				//If we have a slash the first time through, then this is
				//an error and unallowed filename character.
				//
				case '/': string[i] = REPLACE_SLASH_CHAR; break;
				
				//
				//Convert Mac specific path separator to Be specific.
				//
				case ':': string[i] = '/'; break;
				
				default:
					break;
			}
		}
	}
	
	return( afpError );
}


/*
 * GetUnicodeString()
 *
 * Description:
 *		Retrieve a unicode string from the request buffer and return
 *		it as a c-style string. This is a worker routine for
 *		GetString() and shouldn't be called directly.
 *
 *		This function basically converts an AFPName to a c-style string.
 *
 * Returns: None
 */

AFPERROR afp_buffer::GetUnicodeString(char* string, uint16 cbstring, int16* sLen, bool isPath)
{
	int32		stringLen	= 0;
	int32		hint		= 0;
	AFPERROR	afpError	= AFP_OK;
		
	memset(string, 0, cbstring);

	if (isPath) {
		hint = GetInt32();
	}
	
	stringLen	= GetInt16();
		
	//
	//Check to make sure we were given a big enough buffer
	//to copy the string into.
	//
	if ((stringLen + sizeof(char)) > cbstring)
	{
		DPRINT(("[afp_buffer:GetUnicodeString]String buffer too small! (%u vs %ld)\n", cbstring, stringLen));		
		return( afpParmErr );
	}
	
	if (stringLen > 0)
	{
		status_t	status 	= B_OK;
		int32		destLen	= cbstring;
		
		status = convert_from_utf8(
						B_MAC_ROMAN_CONVERSION,
						(char*)GetCurrentPosPtr(),
						&stringLen,
						string,
						&destLen,
						0
						);
						
		afpError = (status == B_OK) ? AFP_OK : afpParmErr;
		
		if (AFP_SUCCESS(afpError))
		{
			Advance(stringLen);
			*sLen = destLen;
		}
	}
	
	return( afpError );
}


/*
 * GetPascalString()
 *
 * Description:
 *		Retrieve a pascal string from the request buffer and return
 *		it as a c-style string. This is a worker routine for
 *		GetString() and shouldn't be called directly.
 *
 * Returns: None
 */

AFPERROR afp_buffer::GetPascalString(char* string, uint16 cbstring, int16* sLen)
{
	uint8	stringLen	= GetInt8();
	
	memset(string, 0, cbstring);
	
	//
	//Check to make sure we were given a big enough buffer
	//to copy the string into.
	//
	if ((stringLen + sizeof(char)) > cbstring)
	{
		DPRINT(("[afp_buffer:GetPascalString]String buffer too small! (%d)\n", stringLen));		
		return( afpParmErr );
	}
	
	if (stringLen > 0)
	{
		//
		//Now copy the string into the supplied buffer.
		//
		GetRawData(string, stringLen);
		
		//
		//Add a null terminator on the end of the new string.
		//
		string[stringLen] = '\0';
	}
	
	*sLen = stringLen;
	
	return( AFP_OK );
}


/*
 * GetUniString()
 *
 * Description:
 *		Retrieve a normal unicode string (not AFPName) from the request buffer.
 *
 * Returns: None
 */

AFPERROR afp_buffer::GetUniString(char* string, uint16 cbstring, uint16* sLen)
{
	int32		stringLen	= *sLen;
	AFPERROR	afpError 	= AFP_OK;
		
	memset(string, 0, cbstring);
			
	//
	//Check to make sure we were given a big enough buffer
	//to copy the string into.
	//
	if ((stringLen + sizeof(char)) > cbstring)
	{
		DPRINT(("[afp_buffer:GetUniString]String buffer too small! (%d vs %ld)\n", cbstring, stringLen));		
		return( afpParmErr );
	}
	
	if (stringLen > 0)
	{
		status_t	status 	= B_OK;
		int32		destLen	= cbstring;
		
		status = convert_from_utf8(
						B_MAC_ROMAN_CONVERSION,
						(char*)GetCurrentPosPtr(),
						&stringLen,
						string,
						&destLen,
						0
						);
						
		afpError = (status == B_OK) ? AFP_OK : afpParmErr;
		
		if (AFP_SUCCESS(afpError))
		{
			Advance(stringLen);
			*sLen = destLen;
		}
	}
	
	return( afpError );
}


/*
 * GetRawData()
 *
 * Description:
 *		Extracts an raw data from the buffer and advances the
 *		buffer pointer.
 *
 * Returns: void
 */

void afp_buffer::GetRawData(void* buffer, int16 amount)
{
	memcpy(buffer, mCurrentPos, amount);
	mCurrentPos += amount;
}


/*
 * AddCStringAsPascal()
 *
 * Description:
 *		Converts a C style string into a pascal string and inAdds
 *		it into the afp buffer.
 *
 * Returns: AFPERROR
 */

AFPERROR afp_buffer::AddCStringAsPascal(char* string)
{
	if (string != NULL)
	{
		uint8 	cbstring = strlen(string);
		
		if (((mCurrentPos - mBuffer) + cbstring + 1) <= mBufferSize)
		{
			*mCurrentPos++ = cbstring;
			memcpy(mCurrentPos, string, cbstring);
			
			ConvertIllegalCharsBackToMac((uchar*)mCurrentPos, cbstring);
		
			mCurrentPos += cbstring;
			
			return( AFP_OK );
		}
	}
	
	return( afpParmErr );
}


/*
 * AddUniString()
 *
 * Description:
 *		Add a unicode string to the buffer.
 *
 *		This function basically converts a c-style string to an AFPName.
 *
 * Returns: None
 */

AFPERROR afp_buffer::AddUniString(char* string, bool isPath, bool isAFPName)
{
	char			destString[1024];
	status_t		status		= B_OK;
	int32			cbstring 	= strlen(string);
	int32			destSize	= cbstring * sizeof(wchar_t);
	
	if (string != NULL)
	{
		if (((mCurrentPos - mBuffer) + (cbstring*2) + 2) <= mBufferSize)
		{
			if ((isPath) && (isAFPName))
			{
				//
				//If its a pathname, we need to reconvert the chars back
				//to Mac specific.
				//
				ConvertIllegalCharsBackToMac((uchar*)string, cbstring);
				
				//
				//This is supposed to be the encoding hint. We need to
				//build an encoding mapping table before we can use this.
				//for now, 0 == MAC_ROMAN encoding.
				//
				AddInt32(0);
			}
			
			status = convert_to_utf8(
							B_MAC_ROMAN_CONVERSION,
							string,
							&cbstring,
							destString,
							&destSize,
							0
							);
							
			if (status == B_OK)
			{
				if (isAFPName)
				{
					AddInt16(destSize);
					AddRawData(destString, destSize);
				}
				else
				{
					AddRawData(destString, destSize + sizeof(char));
				}
								
				return( AFP_OK );
			}
			else
			{
				//
				//If conversion failed, we want to write a 0 to the buffer
				//to show that there is 0 sized string.
				//
				AddInt16(0);
			}
		}
	}
	
	return( afpParmErr );
}


/*
 * AddRawData()
 *
 * Description:
 *		InAdds raw data into the buffer and advances the
 *		buffer pointer.
 *
 * Returns: AFPERROR
 */

AFPERROR afp_buffer::AddRawData(void* data, int16 size)
{
	if (((mCurrentPos - mBuffer) + size) <= mBufferSize)
	{
		memcpy(mCurrentPos, data, size);
		mCurrentPos += size;
		
		return( AFP_OK );
	}
	
	return( afpParmErr );
}


/*
 * AdvanceIfOdd()
 *
 * Description:
 *		Advance 1 byte if we are currently pointing at an odd position
 *		in the buffer.
 *
 * Returns: none
 */

void afp_buffer::AdvanceIfOdd(void)
{
	if ((GetCurrentPosPtr() - GetBuffer()) % 2) {
		Advance(sizeof(int8));
	}
}






