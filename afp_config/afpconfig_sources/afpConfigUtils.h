#include <Application.h>
#include <StorageKit.h>
#include <String.h>
#include <TextView.h>

#include "commands.h"

#define BE_SUCCESS(e)	((e) == B_OK)

extern char errString[24];
#define GET_BERR_STR(e) AFPGetErrorString(e)

int AFPGetVolumePathFromIndex(
	int16		volumeIndex,
	BString*	path
);

int AFPGetVolumeFlagsFromIndex(
	int16	volumeIndex,
	uint32*	volumeFlags
);

int AFPGetVolumeFlagsFromPath(
	BString*	path,
	uint32*		volumeFlags
);

int AFPSaveVolumeFlags(
	int16	volumeIndex,
	uint32	volumeFlags
);

int AFPRemoveShare(
	int16	volumeIndex
);

int AFPRemoveShare(
	BString*	path
);

int AFPAddShare(
	entry_ref 	newRef,
	BPath*		dirPath = NULL
);

int AFPSendMessage(
	const char* messageText,
	int32		messageLength
);

int AFPSaveHostnameToFile(
	const char*	hostname
);

int AFPGetHostnameFromFile(
	char*		hostname,
	int32		cbHostname
);

int AFPSaveLogonMessage(
	const char*	messageText,
	int32		messageLength
);

int AFPGetLogonMessage(
	char*		buffer,
	int32		cbBuffer
);

bool AFPGetGuestsAllowed();

int AFPSetGuestsAllowed(
	bool	allowed
);

int AFPGetIndUser(
	int16		userIndex,
	BString*	userName		= NULL,
	BString*	userPassword	= NULL,
	uint32*		userFlags		= 0
);

int AFPDeleteUser(
	const char*	userName
);

int AFPGetUserInfo(
	const char* userName,
	BString*	userPassword	= NULL,
	uint32*		userFlags		= 0
);

int AFPUpdateUserProperties(
	const char*		userName,
	const char*		userPassword,
	uint32			userFlags
);

int AFPAddUser(
	const char*		userName,
	const char*		userPassword,
	uint32			userFlags
);

int AFPGetBytesPerSecond(
	int32* bytesPerSecond
);

int AFPGetBytesSent(
	int64* bytesSent
);

int AFPGetBytesRecieved(
	int64* bytesReceived
);

int AFPGetPacketsProcessed(
	int32* packetsProcessed
);

int AFPGetUsersLoggedOn(
	int32* usersLoggedOn
);

uint32 AFPGetServerVersion();
bool AFPGetServerVersionString(char* vers, int16 cbString);

bool AFPServerIsRunning();

char* AFPGetErrorString(int32 error);

void SetTextViewFontSize(BTextView* textView, float size);

