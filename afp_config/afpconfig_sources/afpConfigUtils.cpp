#include <stdio.h>
#include <Beep.h>

#include "afpConfigUtils.h"

char errString[24];


/*
 * AFPGetVolumePathFromIndex()
 *
 * Description:
 *	Given a volume index, returns the path the volume.
 *
 * Returns: Error or B_OK
 */

int AFPGetVolumePathFromIndex(
	int16		volumeIndex,
	BString*	path
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_GETVOLUMEPATH);
	
	if (volumeIndex < 0) {
		return( be_afp_invalidvolume );
	}
	
	message.AddInt16(AFP_PARAM_INT16, (int16)volumeIndex);
	messenger.SendMessage(&message, &message);
	
	//
	//Check for failure and bail if the server didn't return anything.
	//
	if (message.what == be_afp_success)
	{
		if (message.FindString(AFP_PARAM_PATHSTRING, path) != B_OK)
		{
			return( be_afp_failure );
		}	
	}
	else {
	
		return(message.what);
	}
	
	return( B_OK );
}


/*
 * AFPGetVolumeFlags()
 *
 * Description:
 *	Given an index to a shared volume returns the flags associated with it.
 *
 * Returns: Error or B_OK
 */

int AFPGetVolumeFlagsFromIndex(
	int16	volumeIndex,
	uint32*	volumeFlags
)
{
	BString		path;
	int			result = B_OK;
	
	result = (volumeFlags != NULL) ? B_OK : be_afp_paramerr;
	
	if (result == B_OK)
	{	
		result = AFPGetVolumePathFromIndex(volumeIndex, &path);
		
		if (result == B_OK) {
		
			result = AFPGetVolumeFlagsFromPath(&path, volumeFlags);
		}
	}
	
	return( result );
}


/*
 * AFPGetVolumeFlagsFromPath()
 *
 * Description:
 *	Given a path to a shared volume returns the flags associated with it.
 *
 * Returns: Error or B_OK
 */

int AFPGetVolumeFlagsFromPath(
	BString*	path,
	uint32*		volumeFlags
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_GETVOLFLAGS);
	int			result = B_OK;
	
	result = (volumeFlags != NULL) ? B_OK : be_afp_paramerr;
	
	if (result == B_OK)
	{
		//
		//Initialize our result code to failure for simplicity sake.
		//
		result = be_afp_failure;
		
		message.AddString(AFP_PARAM_PATHSTRING, path->String());
		messenger.SendMessage(&message, &message);
		
		if (message.what == be_afp_success)
		{
			if (message.FindInt32(AFP_PARAM_INT32, (int32*)volumeFlags) == B_OK)
			{
				result = B_OK;
			}
		}
	}
	
	return( result );
}


/*
 * AFPSaveVolumeFlags()
 *
 * Description:
 *	Given a path to a shared volume saves the flags associated to it.
 *
 * Returns: Error or B_OK
 */

int AFPSaveVolumeFlags(
	int16	volumeIndex,
	uint32	volumeFlags
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_SETVOLFLAGS);
	BString		path;
	int			result = B_OK;
	
	result = AFPGetVolumePathFromIndex(volumeIndex, &path);
	
	if (result == B_OK)
	{
		message.AddString(AFP_PARAM_PATHSTRING, path.String());
		message.AddInt32(AFP_PARAM_INT32, volumeFlags);
		
		messenger.SendMessage(&message, &message);
		
		if (message.what != be_afp_success) {
		
			result = be_afp_failure;
		}
	}
	
	return( result );
}


/*
 * AFPRemoveShare()
 *
 * Description:
 *	Stop sharing a directory.
 *
 * Returns: Error or B_OK
 */
 
int AFPRemoveShare(
	int16	volumeIndex
)
{
	BString	path;
	int		result = B_OK;
	
	result = AFPGetVolumePathFromIndex(volumeIndex, &path);
	
	if (result == B_OK)
	{
		return( AFPRemoveShare(&path) );
	}
	
	return( result );
}


/*
 * AFPRemoveShare()
 *
 * Description:
 *	Stop sharing a directory.
 *
 * Returns: Error or B_OK
 */

int AFPRemoveShare(
	BString*	path
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_RMVSHARE);
	int			result = B_OK;
	
	message.AddString(AFP_PARAM_PATHSTRING, path->String());
	messenger.SendMessage(&message, &message);

	switch(message.what)
	{
		case be_afp_success:
			result = B_OK;
			break;
					
		default:
			result = message.what;
			break;
	}
	
	return( result );
}


/*
 * AFPAddShare()
 *
 * Description:
 *	Tells the server to begin sharing a new volume.
 *
 * Returns: Error or B_OK
 */

int AFPAddShare(
	entry_ref 	newRef,
	BPath*		dirPath
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_ADDSHARE);
	BEntry		entry(&newRef);
	BPath		path;
	int			result = B_ERROR;
	
	if (entry.InitCheck() == B_OK)
	{
		BDirectory	dir(&entry);
		
		entry.GetPath(&path);
		
		//
		//AFP can only share directories, files and links will not be
		//and cannot be shared.
		//
		if (!entry.IsDirectory()) {
		
			return( be_afp_notadirectory );
		}
		
		//
		//We do not allow sharing of the root directory, this causes
		//many, many problems.
		//
		if (dir.IsRootDirectory()) {
		
			return( be_afp_isrootdir );
		}
		
		//
		//Create the new AFP share on this directory.
		//
		message.AddString(AFP_PARAM_PATHSTRING, path.Path());
		messenger.SendMessage(&message, &message);
		
		//
		//Message now contains the reply from the afp_server
		//
		switch(message.what)
		{
			case be_afp_success:
				if (dirPath != NULL) {
				
					dirPath->SetTo(path.Path());
				}
				
				result = B_OK;
				break;
							
			default:
				result = message.what;
				break;
		}
	}
	
	return( result );
}


/*
 * AFPSendMessage()
 *
 * Description:
 *	Sends a message to all connected users.
 *
 * Returns: Error or B_OK
 */

int AFPSendMessage(
	const char* messageText,
	int32		messageLength
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_SENDMSG);
	int			result = be_afp_failure;

	message.AddString(AFP_PARAM_STRING, messageText);
	messenger.SendMessage(&message, &message);

	result = (message.what == be_afp_success) ? B_OK : message.what;
	
	return( result );
}


/*
 * AFPSaveHostnameToFile()
 *
 * Description:
 *		Saves the new hostname to the configuration file and then
 *		sends an message to the afp_server notifying it of the new name.
 *
 * Returns: Error or B_OK
 */

int AFPSaveHostnameToFile(
	const char*	hostname
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_SETHOSTNAME);
	BPath		path;
	BDirectory	dir;
	BFile		file;
	char		fpath[256];
	int			result = be_afp_failure;
	
	//
	//The hostname cannot be null nor can it be zero length.
	//
	if ((hostname == NULL) || (strlen(hostname) <= 0))
	{
		return( be_afp_invlidehostnamelen );
	}
	
	//
	//First, find the path to the user settings where we'll save the
	//text message to.
	//
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{
		dir.SetTo(path.Path());
				
		if (dir.InitCheck() == B_OK)
		{
			sprintf(fpath, "%s/%s", path.Path(), AFP_HOSTNAME_FILE_NAME);
			
			//
			//Delete the file if it already exits.
			//
			if (dir.Contains(fpath, B_FILE_NODE))
			{
				BEntry	entry(fpath);
				entry.Remove();
			}
						
			if (dir.CreateFile(fpath, &file) == B_OK)
			{
				file.Write(hostname, strlen(hostname));
				result = B_OK;
			}
		}
	}
	
	//
	//Send an interapplication communication message to the afp_server
	//notifying it of the new hostname.
	//
	
	message.AddString(AFP_PARAM_STRING, hostname);
	messenger.SendMessage(&message, &message);
	
	result = (message.what == be_afp_success) ? B_OK : message.what;
	
	return( result );
}


/*
 * AFPGetHostnameFromFile()
 *
 * Description:
 *		Retrieves hostname from the configuration file.
 *
 * Returns: Error or B_OK
 */

int AFPGetHostnameFromFile(
	char*		hostname,
	int32		cbHostname
)
{
	BPath		path;
	BFile		file;
	status_t	result	= be_afp_failure;
	char		fpath[256];
	
	if ((hostname == NULL) || (cbHostname <= 0))
	{
		return( be_afp_paramerr );
	}
	
	memset(hostname, 0, cbHostname);
	
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{	
		sprintf(fpath, "%s/%s", path.Path(), AFP_HOSTNAME_FILE_NAME);
		file.SetTo(fpath, B_READ_WRITE);
				
		if (file.InitCheck() == B_OK)
		{
			ssize_t	amtRead = file.Read(hostname, cbHostname);
						
			if (amtRead > 0)
			{
				//
				//We have a successful read. Note that we don't need to
				//null terminate the buffer since we zeroed it out above.
				//
				
				result = be_afp_success;
			}
		}
	}
	
	if (result != be_afp_success)
	{
		//
		//We either couldn't find the file, or it was empty. This can
		//happen if this is the first time the afp_server is run and
		//the user had never configured it before.Try getting the hostname
		//directly from the afp_server, it will create a default one if
		//the file does not exist.
		//
		
		BMessenger	messenger(AFPServerSignature);
		BMessage	message(CMD_AFP_GETHOSTNAME);
		BString		hostString;
		
		messenger.SendMessage(&message, &message);
		
		result = message.what;
		
		if (result == be_afp_success)
		{
			//
			//The server was able to pass back the hostname to us.
			//
			
			if (message.FindString(AFP_PARAM_STRING, &hostString) == B_OK)
			{
				strncpy(hostname, hostString.String(), hostString.Length());
			}
			else {
				
				result = be_afp_failure;
			}
		}
	}
	
	return( result );
}


/*
 * AFPSaveLogonMessage()
 *
 * Description:
 *	Sets and saves the logon message.
 *
 * Returns: Error or B_OK
 */

int AFPSaveLogonMessage(
	const char*	messageText,
	int32		messageLength
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_UPDATELOGINMSG);
	BPath		path;
	BDirectory	dir;
	BFile		file;
	char		fpath[256];
	int			result = be_afp_failure;
	
	//
	//First, find the path to the user settings where we'll save the
	//text message to.
	//
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{
		dir.SetTo(path.Path());
				
		if (dir.InitCheck() == B_OK)
		{
			sprintf(fpath, "%s/%s", path.Path(), AFP_LOGONMSG_FILE_NAME);
			
			//
			//Delete the file if it already exits.
			//
			if (dir.Contains(fpath, B_FILE_NODE))
			{
				BEntry	entry(fpath);
				entry.Remove();
			}
						
			if (dir.CreateFile(fpath, &file) == B_OK)
			{
				file.Write(messageText, messageLength);
				result = B_OK;
			}
		}
	}
	
	message.AddString(AFP_PARAM_STRING, messageText);
	messenger.SendMessage(&message, &message);
	
	result = (message.what == be_afp_success) ? B_OK : message.what;
	
	return( result );
}


/*
 * AFPGetLogonMessage()
 *
 * Description:
 *	Gets the logon message from the file where it is saved.
 *
 * Returns: Error or B_OK
 */

int AFPGetLogonMessage(
	char*		buffer,
	int32		cbBuffer
)
{
	BPath		path;
	BFile		file;
	char		fpath[256];
	
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{	
		sprintf(fpath, "%s/%s", path.Path(), AFP_LOGONMSG_FILE_NAME);
		file.SetTo(fpath, B_READ_WRITE);
				
		if (file.InitCheck() == B_OK)
		{
			memset(buffer, 0, cbBuffer);
			file.Read(buffer, cbBuffer);
			
			return( B_OK );
		}
	}
	
	return( be_afp_failure );
}


/*
 * AFPGetGuestsAllowed()
 *
 * Description:
 *	Returns TRUE if guests are allowed to logon to the server.
 *
 * Returns: bool
 */

bool AFPGetGuestsAllowed()
{
	BString		ignore;
	uint32		flags;
	
	if (AFPGetUserInfo(AFP_GUEST_NAME, &ignore, &flags) == B_OK)
	{
		return( (flags & kUserEnabled) != 0 );
	}
	
	return( false );
}


/*
 * AFPSetGuestsAllowed()
 *
 * Description:
 *	Tells the server wheter or not to allow guests to sign in.
 *
 * Returns: Error or B_OK
 */

int AFPSetGuestsAllowed(
	bool	allowed
)
{
	BString		pswd;
	uint32		flags;
	
	if (AFPGetUserInfo(AFP_GUEST_NAME, &pswd, &flags) == B_OK)
	{
		if (allowed) {
		
			flags |= kUserEnabled;
		}
		else {
		
			flags &= ~kUserEnabled;
		}
	}
	
	return( AFPUpdateUserProperties(
					AFP_GUEST_NAME,
					pswd.String(),
					flags)
					);
}


/*
 * AFPGetIndUser()
 *
 * Description:
 *	Returns the user with properties at index.
 *
 * Returns: Error or B_OK
 */

int AFPGetIndUser(
	int16		userIndex,
	BString*	userName,
	BString*	userPassword,
	uint32*		userFlags
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_GETINDUSER);
	int			result	= B_OK;
		
	message.AddInt16(AFP_PARAM_INT16, userIndex);
	messenger.SendMessage(&message, &message);
		
	result = (message.what == be_afp_success) ? B_OK : message.what;
		
	if (result == B_OK)
	{
		if (userName != NULL) {
			message.FindString(AFP_PARAM_STRING_USERNAME, userName);
		}
		
		if (userPassword != NULL) {
			message.FindString(AFP_PARAM_STRING_PASSWORD, userPassword);
		}
		
		if (userFlags != NULL) {
			message.FindInt32(AFP_PARAM_INT32, (int32*)userFlags);
		}
	}
	
	return( result );
}


/*
 * AFPDeleteUser()
 *
 * Description:
 *	Delete a user from the user list.
 *
 * Returns: Error or B_OK
 */

int AFPDeleteUser(
	const char*	userName
)
{
	BMessenger		messenger(AFPServerSignature);
	BMessage		message(CMD_AFP_DELUSER);
	
	message.AddString(AFP_PARAM_STRING_USERNAME, userName);
	messenger.SendMessage(&message, &message);
	
	return((message.what == be_afp_success) ? B_OK : message.what);
}


/*
 * AFPGetUserInfo()
 *
 * Description:
 *	Get property information about a particular user.
 *
 * Returns: Error or B_OK
 */

int AFPGetUserInfo(
	const char* userName,
	BString*	userPassword,
	uint32*		userFlags
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_GETUSERINFO);
	int			result = B_OK;
	
	message.AddString(AFP_PARAM_STRING_USERNAME, userName);
	messenger.SendMessage(&message, &message);
	
	result = (message.what == be_afp_success) ? B_OK : message.what;
	
	if (result == B_OK)
	{
		if (userPassword != NULL) {
			message.FindString(AFP_PARAM_STRING_PASSWORD, userPassword);
		}
		
		if (userFlags != NULL) {
			message.FindInt32(AFP_PARAM_INT32, (int32*)userFlags);
		}
	}
	
	return( result );
}


/*
 * AFPUpdateUserProperties()
 *
 * Description:
 *	Update the properties for this user.
 *
 * Returns: Error or B_OK
 */

int AFPUpdateUserProperties(
	const char*		userName,
	const char*		userPassword,
	uint32			userFlags
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_UPDATEUSERINFO);
	
	//
	//Cannot have a blank username or password. Blank password is allowed
	//only for the guest account.
	//
	if (((strlen(userName) == 0) || (strlen(userPassword) == 0)) && (strcmp(userName, AFP_GUEST_NAME)))
	{
		return( be_afp_blankentry );
	}
	
	//
	//Add the username and password to the message block.
	//
	if (userName != NULL) {
	
		message.AddString(AFP_PARAM_STRING_USERNAME, userName);
	}
	
	if (userPassword != NULL) {
	
		message.AddString(AFP_PARAM_STRING_PASSWORD, userPassword);
	}
	
	//
	//Tell whether we're an admin or not.
	//
	message.AddInt32(AFP_PARAM_INT32, userFlags);
	
	//
	//Now send the message.
	//
	messenger.SendMessage(&message, &message);

	return((message.what == be_afp_success) ? B_OK : message.what);
}


/*
 * AFPAddUser()
 *
 * Description:
 *	Update the properties for this user.
 *
 * Returns: Error or B_OK
 */

int AFPAddUser(
	const char*		userName,
	const char*		userPassword,
	uint32			userFlags
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_ADDUSER);
	
	//
	//Add the username and password to the message block.
	//
	message.AddString(AFP_PARAM_STRING_USERNAME, userName);
	message.AddString(AFP_PARAM_STRING_PASSWORD, userPassword);
	
	//
	//Tell whether we're an admin or not.
	//
	message.AddInt32(AFP_PARAM_INT32, userFlags);
	
	//
	//Now send the message.
	//
	messenger.SendMessage(&message, &message);

	return((message.what == be_afp_success) ? B_OK : message.what);
}

/*
 * AFPGetBytesPerSecond()
 *
 * Description:
 *		Supplies the bytes per second being transfered in/out of the server.
 *
 * Returns:
 *		B_OK if successfull, otherwise error code
 */

int AFPGetBytesPerSecond(
	int32* bytesPerSecond
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage	message(CMD_AFP_GETBYTESPERSECOND);
	int32		bps = 0;
	
	if (bytesPerSecond == NULL) {
		return( be_afp_paramerr );
	}
	
	*bytesPerSecond = 0;
	
	messenger.SendMessage(&message, &message);
	
	if (message.what == be_afp_success)
	{
		if (message.FindInt32(AFP_PARAM_INT32, &bps) == B_OK)
		{
			if (bps < 0) {
				bps = 0;
			}
			
			*bytesPerSecond = bps;
		}
	}
	
	return((message.what == be_afp_success) ? B_OK : message.what);
}


/*
 * AFPGetBytesSent()
 *
 * Description:
 *		Supplies the total bytes sent by the server.
 *
 * Returns:
 *		B_OK if successfull, otherwise error code
 */

int AFPGetBytesSent(
	int64* bytesSent
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage 	message(CMD_AFP_GETSENTBYTES);
	int64		bytes = 0;
	
	if (bytesSent == NULL) {
		return( be_afp_paramerr );
	}
	
	*bytesSent = 0;
	
	messenger.SendMessage(&message, &message);
	
	if (message.what == be_afp_success)
	{
		if (message.FindInt64(AFP_PARAM_INT64, &bytes) == B_OK)
		{
			*bytesSent = bytes;
		}
	}
	
	return((message.what == be_afp_success) ? B_OK : message.what);	
}


/*
 * AFPGetBytesRecieved()
 *
 * Description:
 *		Supplies the total bytes received by the server.
 *
 * Returns:
 *		B_OK if successfull, otherwise error code
 */

int AFPGetBytesRecieved(
	int64* bytesReceived
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage 	message(CMD_AFP_GETRECVBYTES);
	int64		bytes = 0;
	
	messenger.SendMessage(&message, &message);
	
	if (bytesReceived == NULL) {
		return( be_afp_paramerr );
	}
	
	*bytesReceived = 0;
	
	if (message.what == be_afp_success)
	{
		if (message.FindInt64(AFP_PARAM_INT64, &bytes) == B_OK)
		{
			*bytesReceived = bytes;
		}
	}
	
	return((message.what == be_afp_success) ? B_OK : message.what);	
}


/*
 * AFPGetPacketsProcessed()
 *
 * Description:
 *		Supplies the total DSI packets processed by the server.
 *
 * Returns:
 *		B_OK if successfull, otherwise error code
 */

int AFPGetPacketsProcessed(
	int32* packetsProcessed
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage 	message(CMD_AFP_GETDSIPACKETS);
	int32		bytes32 = 0;
	
	if (packetsProcessed == NULL) {
		return( be_afp_paramerr );
	}
	
	*packetsProcessed = 0;
	
	messenger.SendMessage(&message, &message);
	
	if (message.what == be_afp_success)
	{
		if (message.FindInt32(AFP_PARAM_INT32, &bytes32) == B_OK)
		{
			*packetsProcessed = bytes32;
		}
	}
	
	return((message.what == be_afp_success) ? B_OK : message.what);	
}


/*
 * AFPGetUsersLoggedOn()
 *
 * Description:
 *		Supplies the total number of users currently logged onto the server.
 *
 * Returns:
 *		B_OK if successfull, otherwise error code
 */

int AFPGetUsersLoggedOn(
	int32* usersLoggedOn
)
{
	BMessenger	messenger(AFPServerSignature);
	BMessage 	message(CMD_AFP_GETUSERSLOGGEDIN);
	int32		bytes32;
	
	if (usersLoggedOn == NULL) {
		return( be_afp_paramerr );
	}

	*usersLoggedOn = 0;
	
	messenger.SendMessage(&message, &message);
	
	if (message.what == be_afp_success)
	{
		if (message.FindInt32(AFP_PARAM_INT32, &bytes32) == B_OK)
		{
			*usersLoggedOn = bytes32;
		}
	}

	return((message.what == be_afp_success) ? B_OK : message.what);	
}


/*
 * AFPServerIsRunning()
 *
 * Description:
 *		Returns TRUE if the afp_server is up and running. Currently, we just look
 *		for the # of logged on users and check for failure.
 *
 * Returns:
 *		TRUE if server is running.
 */

bool AFPServerIsRunning()
{
	int32 users;
	
	return( AFPGetUsersLoggedOn(&users) == B_OK );
}


/*
 * AFPGetServerVersion()
 *
 * Description:
 *		Returns in integer representing the version of currently running server.
 *		The integer is a hex representation, for example, 0x0151 would be interpreted
 *		as 1.5.1.
 *
 * Returns:
 *		UInt16 - 0 == error getting the version
 */

uint32 AFPGetServerVersion()
{
	BMessenger	messenger(AFPServerSignature);
	BMessage 	message(CMD_AFP_SERVER_VERSION);
	uint32		result = 0;
	
	messenger.SendMessage(&message, &message);
	
	if (message.what == be_afp_success)
	{
		int32	tmp = 0;
		
		if (message.FindInt32(AFP_PARAM_INT32, &tmp) == B_OK) {
			
			result = (uint32)tmp;
		}
	}
	
	return( result );
}


/*
 * AFPGetServerVersionString()
 *
 * Description:
 *		Returns a null terminated string representing the version of currently running server.
 *
 * Returns:
 *		bool - TRUE == success getting string, FALSE otherwise.
 */

bool AFPGetServerVersionString(char* vers, int16 cbString)
{
	uint32		version 	= AFPGetServerVersion();
	const int8	verStrLen	= 6;
	
	memset(vers, 0, cbString);
	
	if ((version > 0) && (cbString > verStrLen))
	{
		sprintf(vers, "%d.%d.%d",
					(version & 0xFF000000) >> 24,
					(version & 0x00FF0000) >> 16,
					(version & 0x0000FF00) >> 8
					);
		
		//
		//If there is a beta version ,the append the beta tag to the string
		//
		
		if ((version & 0x000000FF) != 0)
		{
			char tmp[16];
			
			memset(tmp, 0, sizeof(tmp));
			sprintf(tmp, " Beta %d", (version & 0x000000FF));
			strcat(vers, tmp);
		}
		
		return( true );
	}
	
	return( false );
}


/*
 * AFPGetErrorString()
 *
 * Description:
 *		Returns a string describing the error code passed.
 *
 * Returns:
 */

char* AFPGetErrorString(int32 error)
{
	switch(error)
	{
		case B_FILE_EXISTS:				strcpy(errString, "B_FILE_EXISTS");				break;
		case B_ENTRY_NOT_FOUND:			strcpy(errString, "B_ENTRY_NOT_FOUND");			break;
		case B_NAME_TOO_LONG:			strcpy(errString, "B_NAME_TOO_LONG");			break;
		case B_NOT_A_DIRECTORY:			strcpy(errString, "B_NOT_A_DIRECTORY");			break;
		case B_DIRECTORY_NOT_EMPTY:		strcpy(errString, "B_DIRECTORY_NOT_EMPTY");		break;
		case B_DEVICE_FULL:				strcpy(errString, "B_DEVICE_FULL");				break;
		case B_READ_ONLY_DEVICE:		strcpy(errString, "B_READ_ONLY_DEVICE");		break;
		case B_IS_A_DIRECTORY:			strcpy(errString, "B_IS_A_DIRECTORY");			break;
		case B_NO_MORE_FDS:				strcpy(errString, "B_NO_MORE_FDS");				break;
		case B_CROSS_DEVICE_LINK:		strcpy(errString, "B_CROSS_DEVICE_LINK");		break;
		case B_LINK_LIMIT:				strcpy(errString, "B_LINK_LIMIT");				break;
		case B_BUSTED_PIPE:				strcpy(errString, "B_BUSTED_PIPE");				break;
		case B_UNSUPPORTED:				strcpy(errString, "B_UNSUPPORTED");				break;
		case B_PARTITION_TOO_SMALL:		strcpy(errString, "B_PARTITION_TOO_SMALL");		break;
		case B_PERMISSION_DENIED:		strcpy(errString, "B_PERMISSION_DENIED");		break;
		case B_NOT_ALLOWED:				strcpy(errString, "B_NOT_ALLOWED");				break;
		case ECONNRESET:				strcpy(errString, "ECONNRESET");				break;
		case ENOTCONN:					strcpy(errString, "ENOTCONN");					break;
		case EBADF:						strcpy(errString, "EBADF");						break;
		case EADDRINUSE:				strcpy(errString, "EADDRINUSE");				break;
		case EWOULDBLOCK:				strcpy(errString, "EWOULDBLOCK");				break;
		case EINTR:						strcpy(errString, "EINTR");						break;
		case ENOPROTOOPT:				strcpy(errString, "ENOPROTOOPT");				break;
		case B_ERROR:					strcpy(errString, "B_ERROR");					break;
		case B_OK:						strcpy(errString, "B_OK");						break;
		
		//case be_afp_failure:			strcpy(errString, "AFP_API_FAILURE");			break;
		case be_afp_usernotfound:		strcpy(errString, "user not found");			break;
		case be_afp_useralreadyexists:	strcpy(errString, "user already exists");		break;
		case be_afp_fileoperationfailed:strcpy(errString, "file operation failed");		break;
		case be_afp_invalidvolume:		strcpy(errString, "invalid volume");			break;
		case be_afp_sharealreadyexits:	strcpy(errString, "share already exits");		break;
		case be_afp_paramerr:			strcpy(errString, "parameter error");			break;
		case be_afp_notadirectory:		strcpy(errString, "not a directory");			break;
		case be_afp_isrootdir:			strcpy(errString, "is root directory");			break;
		case be_afp_blankentry:			strcpy(errString, "blank entry not allowed");	break;

		default:
			strcpy(errString, "UNKNOWN");
			break;
	}
	
	return( errString );
}


/*
 * SetTextViewFontSize()
 *
 * Description:
 *		Helper method for setting the font size in a TextViewControl.
 *
 * Returns:
 */

void SetTextViewFontSize(BTextView* textView, float size)
{
	BFont font;
	textView->GetFontAndColor(0, &font, NULL);
	font.SetSize(size);
	textView->SetFontAndColor(0, 256, &font, B_FONT_SIZE, NULL);
}


	











 
 
 
