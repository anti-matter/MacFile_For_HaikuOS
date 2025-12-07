#ifndef __afpGlobals__
#define __afpGlobals__

#include <Application.h>
#include <Window.h>
#include <Message.h>
#include <Debug.h>
#include <OS.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

extern int 	afpAppReturnValue;

//
//Here are some standard things that make coding easier
//
#define IN
#define OUT

#define AFP_OK			0L
#define AFP_SUCCESS(e)	((e) == AFP_OK)
#define AFP_FAILURE(e)	((e) != AFP_OK)

#if DEBUG

#define DPRINT(ARGS)	printf ARGS

#define BEGIN_PERF_MEASURE()	bigtime_t __curtime = real_time_clock_usecs();
#define END_PERF_MEASURE(s)		bigtime_t __nowtime = real_time_clock_usecs();	\
								DPRINT(("[PERFORMANCE]Function = %s, function time = %d\n", s, __nowtime - __curtime));

#else

#define DPRINT(ARGS)
#define BEGIN_PERF_MEASURE()
#define END_PERF_MEASURE(s)
#define HEXDUMP(p, len)

#endif

#if DEBUG

extern char errString[24];

#define GET_BERR_STR(e) GetBeErrorString(e)

inline char* GetBeErrorString(int32 error)
{
	switch(error)
	{
		case B_FILE_EXISTS:			strcpy(errString, "B_FILE_EXISTS");			break;
		case B_ENTRY_NOT_FOUND:		strcpy(errString, "B_ENTRY_NOT_FOUND");		break;
		case B_NAME_TOO_LONG:		strcpy(errString, "B_NAME_TOO_LONG");		break;
		case B_NOT_A_DIRECTORY:		strcpy(errString, "B_NOT_A_DIRECTORY");		break;
		case B_DIRECTORY_NOT_EMPTY:	strcpy(errString, "B_DIRECTORY_NOT_EMPTY");	break;
		case B_DEVICE_FULL:			strcpy(errString, "B_DEVICE_FULL");			break;
		case B_READ_ONLY_DEVICE:	strcpy(errString, "B_READ_ONLY_DEVICE");	break;
		case B_IS_A_DIRECTORY:		strcpy(errString, "B_IS_A_DIRECTORY");		break;
		case B_NO_MORE_FDS:			strcpy(errString, "B_NO_MORE_FDS");			break;
		case B_CROSS_DEVICE_LINK:	strcpy(errString, "B_CROSS_DEVICE_LINK");	break;
		case B_LINK_LIMIT:			strcpy(errString, "B_LINK_LIMIT");			break;
		case B_BUSTED_PIPE:			strcpy(errString, "B_BUSTED_PIPE");			break;
		case B_UNSUPPORTED:			strcpy(errString, "B_UNSUPPORTED");			break;
		case B_PARTITION_TOO_SMALL:	strcpy(errString, "B_PARTITION_TOO_SMALL");	break;
		case B_PERMISSION_DENIED:	strcpy(errString, "B_PERMISSION_DENIED");	break;
		case B_NOT_ALLOWED:			strcpy(errString, "B_NOT_ALLOWED");			break;
		case ECONNRESET:			strcpy(errString, "ECONNRESET");			break;
		case ENOTCONN:				strcpy(errString, "ENOTCONN");				break;
		case EBADF:					strcpy(errString, "EBADF");					break;
		case EADDRINUSE:			strcpy(errString, "EADDRINUSE");			break;
		case EWOULDBLOCK:			strcpy(errString, "EWOULDBLOCK");			break;
		case EINTR:					strcpy(errString, "EINTR");					break;
		case ENOPROTOOPT:			strcpy(errString, "ENOPROTOOPT");			break;
		case B_ERROR:				strcpy(errString, "B_ERROR");				break;
		case B_OK:					strcpy(errString, "B_OK");					break;
		
		default:
			strcpy(errString, "UNKNOWN");
			break;
	}
	
	return( errString );
}

#else

#define GET_ERR_STR(e)

#endif //DEBUG

#define PUSH_CSTRING(s,p) {									\
			*p = strlen(s);									\
			p += sizeof(int8);								\
			memcpy(p, s, strlen(s));						\
			p += strlen(s);									\
}

#endif //__afpGlobals__
