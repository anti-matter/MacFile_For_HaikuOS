#include <net_settings.h>
#include <String.h>
#include <Message.h>
#include <StorageKit.h>

#include "commands.h"
#include "afphostname.h"

//
//This is the computer/hostname that this afpserver will
//return to mac clients.
//
char	gComputerName[MAX_HOSTNAME_LEN];
bool	gComputerNameInitialized = false;


/*
 * afp_SetHostname()
 *
 * Description:
 *		Sets the hostname this server will report to mac users.
 *
 * Returns:
 *		B_OK if successful, B_ERROR otherwise
 */

status_t afp_SetHostname(const char* hostname)
{
	int16 len;
	
	if (hostname == NULL) {
		
		return( B_ERROR );
	}	
	
	len = strlen(hostname);
	
	if (len > MAX_HOSTNAME_LEN)
	{
		return( B_ERROR );
	}
	else
	{
		DPRINT(("[afphostname]Setting new hostname to: %s\n", hostname));
		
		memset(gComputerName, 0, sizeof(gComputerName));
		strncpy(gComputerName, hostname, len);
		
		gComputerNameInitialized = true;
	}
	
	return( B_OK );
}


/*
 * afp_GetHostname()
 *
 * Description:
 *		Returns the hostname this server will report to mac users.
 *
 * Returns:
 *		B_OK if successful, B_ERROR is the supplied buffer was too small
 */

status_t afp_GetHostname(char* hostname, int32 cbHostname)
{
	//
	//If gComputerName is null, then that means the name hasn't
	//been initialized yet. Call the routine to get the default
	//value from the net settings.
	//
	if (gComputerNameInitialized == false)
	{
		char temp_hostname[MAX_HOSTNAME_LEN];
		
		afp_GetHostnameFromSettingsFile(temp_hostname, sizeof(temp_hostname));
		
		if (strlen(temp_hostname) > 0)
		{
			afp_SetHostname(temp_hostname);
		}
		else
		{
			//
			//We encountered some catostrophic error, a server name
			//cannot be found anywhere in the system.
			//
			
			return( B_ERROR );
		}
	}
	
	if ((hostname != NULL) && (cbHostname >= (int32)strlen(gComputerName)))
	{
		DPRINT(("[afphostname]Returning hostname: %s\n", gComputerName));
		
		strcpy(hostname, gComputerName);
	}
	else {
		
		return( B_ERROR );
	}
	
	return( B_OK );
}


/*
 * afp_FindHostNameFromNetSettings()
 *
 * Description:
 *		Returns the hostname as stored in the Haiku/Be network config.
 *
 * Returns:
 *		Nothing
 */

void afp_FindHostNameFromNetSettings(char* hostname, int32 cbHostname)
{
	char*		ptr;
	char		buff[MAX_HOSTNAME_LEN];
	
	if ((hostname == NULL) || (cbHostname <= 0)) {
		
		return;
	}
			
	ptr = find_net_setting(NULL, "GLOBAL", "HOSTNAME", buff, sizeof(buff));
	
	memset(hostname, 0, cbHostname);
	
	//
	//Copy the name we found into the supplied buffer.
	//
	
	if (ptr != NULL)
	{
		if ((int32)strlen(ptr) < cbHostname) {
			
			strcpy(hostname, ptr);
		}
		else {
			
			strncpy(hostname, ptr, cbHostname);
		}
		
		DPRINT(("[afphostname]Found hostname in settings: %s\n", ptr));
	}
	else
	{
		//
		//Set some name in case the everything above fails.
		//
		
		strcpy(hostname, "HaikuServer");
	}
}


/*
 * afp_GetHostnameFromSettingsFile()
 *
 * Description:
 *		Returns the hostname from the user settings file.
 *
 * Returns:
 *		Nothing.
 */

void afp_GetHostnameFromSettingsFile(char* hostname, int32 cbHostname)
{
	BPath			path;
	BFile			file;
	char			fpath[256];
	
	if ((hostname == NULL) || (cbHostname <= 0)) {
		
		return;
	}

	//
	//Read in the afp-specific hostname that the server will report
	//to the user. This name will show up in the Mac's Finder windows
	//and logon UAMs as the name of this server.
	//
	
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{	
		sprintf(fpath, "%s/%s", path.Path(), AFP_HOSTNAME_FILE_NAME);
		file.SetTo(fpath, B_READ_WRITE);
				
		if (file.InitCheck() == B_OK)
		{
			memset(hostname, 0, cbHostname);
			
			file.Read(hostname, cbHostname);
		}
		
		//
		//If the file isn't there, or it is empty, create the default
		//hostname for this server.
		//
		
		if ((file.InitCheck() != B_OK) || (strlen(hostname) == 0))
		{
			//
			//Set the default hostname since a pref file wasn't found.
			//
			
			DPRINT(("[afphostname]Finding default computer name\n"));
			
			afp_FindHostNameFromNetSettings(hostname, cbHostname);
		}
	}
}


/*
 * afp_HandleGethostnameAppMsg()
 *
 * Description:
 *		Handles application messages requesting the server's hostname.
 *
 * Returns:
 *		Nothing
 */

void afp_HandleGethostnameAppMsg(BMessage* message)
{
	BMessage 	reply(be_afp_success);
	char		hostname[MAX_HOSTNAME_LEN];
	
	memset(hostname, 0, sizeof(hostname));
	
	if (afp_GetHostname(hostname, sizeof(hostname)) == B_OK)
	{
		reply.AddString(AFP_PARAM_STRING, hostname);
		message->SendReply(&reply);
	}
	else
	{
		message->SendReply(be_afp_failure);
	}
}


/*
 * afp_HandleSethostnameAppMsg()
 *
 * Description:
 *		Handles application messages setting the server's hostname.
 *
 * Returns:
 *		Nothing
 */

void afp_HandleSethostnameAppMsg(BMessage* message)
{
	BString		string;
	
	if (message->FindString(AFP_PARAM_STRING, &string) == B_OK)
	{	
		if ((string.Length() < MAX_HOSTNAME_LEN) && (string.Length() > 0))
		{
			if (afp_SetHostname(string.String()) == B_OK) {
				
				message->SendReply(be_afp_success);
				return;
			}
		}
		else
		{
			message->SendReply(be_afp_invlidehostnamelen);
			return;
		}
			
	}
	
	message->SendReply(be_afp_failure);
}



