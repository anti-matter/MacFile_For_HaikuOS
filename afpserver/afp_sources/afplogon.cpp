#include <StorageKit.h>

#include "commands.h"
#include "afp.h"
#include "afpGlobals.h"
#include "afplogon.h"

/*
 * afpImpChangePswd()
 *
 * Description:
 *		Change the password for the given user as requested by the
 *		client.
 *
 * Returns: AFPERROR
 */

AFPERROR afpImpChangePswd(
	char*			userName,
	char*			oldPassword,
	char*			newPassword
	)
{
	AFP_USER_DATA	userData;
	AFPERROR		afpError = AFP_OK;
	
	//
	//First, we validate that the correct username and password
	//were provided for the account. We use the logon user function
	//for convenience, however this may need to change in the future
	//if Haiku gets a real user/account system.
	//
	afpError = afpImpLogonUser(userName, oldPassword, &userData);
	
	if (!AFP_SUCCESS(afpError))
	{
		return( afpError );
	}
	
	//
	//afpImpLogonUser() zeros out the password in the userData field,
	//so we have to get it again. This sucks, but better for security.
	//
	afpError = afpGetUserDataByName(userName, &userData);
	
	if (AFP_SUCCESS(afpError))
	{
		//
		//Check to make sure that the user is allowed to change their password
		//
		if ((userData.flags & kCanChngPswd) == 0)
		{
			DPRINT(("[afplogon:afpImpChangePswd]User does not change password privileges\n"));
			return( afpAccessDenied );
		}
		
		DPRINT(("[afplogon:afpImpChangePswd]Old password: %s\n", oldPassword));
		DPRINT(("[afplogon:afpImpChangePswd]New password: %s\n", newPassword));
		
		//
		//We don't allow zero length passwords since this is bad security.
		//
		if (strlen(newPassword) == 0) {
			
			return( afpPwdTooShortErr );
		}
		
		//
		//Don't allow the user to reset the password to the same one again.
		//
		if (!strcmp(userData.password, newPassword)) {
			
			return( afpPwdSameErr );
		}
		
		//
		//Now, store the password in our userInfo buffer
		//
		memset(userData.password, 0, sizeof(userData.password));
		strcpy(userData.password, newPassword);
		
		//
		//If the must change password flag is set, we can clear it now.
		//
		if ((userData.flags & kMustChngPswd) != 0)
		{
			userData.flags &= ~kMustChngPswd;
		}
		
		afpError = afpUpdateUserInfo(userData);		
	}
	
	memset(&userData.password, 0, sizeof(userData.password));
	
	DPRINT(("[afplogon:afpImpChangePswd]Returning: %lu\n", afpError));
	return( afpError );
}


/*
 * afpImpLogonUser()
 *
 * Description:
 *		Implements a user logon into the server. This function expects
 *		a decrypted (clear text) username and password.
 *
 * Returns: AFPERROR
 */

AFPERROR afpImpLogonUser(
	const char*		userName,
	char*			password,
	AFP_USER_DATA*	userInfo	//return user info (copied into)
)
{
	AFPERROR		afpError = afpUserNotAuth;
	AFP_USER_DATA	userData;
	
	//
	//In all cases, must absolutely have a user name
	//
	if ((userName == NULL) || (userInfo == NULL))
	{
		return( afpParmErr );
	}
	
	afpError = afpGetUserDataByName(userName, &userData);
	
	if (AFP_SUCCESS(afpError))
	{
		//
		//First make sure the user account is enabled.
		//
		if ((userData.flags & kUserEnabled) == 0)
		{
			DPRINT(("[afplogon:logon]User account disabled\n"));
			
			memset(&userData, 0, sizeof(userData));
			return( afpUserNotAuth );
		}
		
		//
		//Now compare the cleartext passwords.
		//
		if (password != NULL)
		{
			if (strcmp(password, userData.password))
			{
				DPRINT(("[afplogon:logon]Password match failure - afpUserNotAuth\n"));
				afpError = afpUserNotAuth;
			}
		}
		
		if (AFP_SUCCESS(afpError))
		{
			if ((password == NULL) && (strlen(userData.password) > 0))
			{
				//
				//There's some kind of trickery going on here!
				//
				DPRINT(("[afplogon:logon]Password NULL, but account has password\n"));
				
				afpError = afpUserNotAuth;
			}
			else
			{
				//
				//Copy the user info into the supplied buffer.
				//
				memcpy(userInfo, &userData, sizeof(userData));
				memset(userInfo->password, 0, sizeof(userInfo->password));

				//
				//Check for password expiry
				//
				if ((AFP_SUCCESS(afpError)) && ((userData.flags & kMustChngPswd) != 0))
				{
					afpError = afpPwdExpiredErr;
				}
			}
		}
		
		//
		//Zero out all uneeded buffers for safety
		//
		memset(&userData, 0, sizeof(userData));
	}
	else
	{
		//
		//We couldn't find the user name in our database, return
		//the right afp error code.
		//
		afpError = afpUserNotAuth;
	}
	
	DPRINT(("[afplogon:logon]Returning: %lu\n", afpError));
		
	return( afpError );
}


/*
 * afpSaveNewUser()
 *
 * Description:
 *		Saves a new user to the user data file.
 *
 * Returns: AFPERROR
 */

AFPERROR afpSaveNewUser(
	const char*		userName,
	const char*		password,
	uint32			flags
	)
{
	BPath			path;
	BNode			file;
	BDirectory		dir;
	char			fpath[256];
	char			attrName[B_ATTR_NAME_LENGTH];
	AFP_USER_DATA	userData;
	ssize_t			size 	= 0;
	
	//
	//Make sure the user name doesn't already exist.
	//
	if (afpGetUserDataByName(userName, NULL) == AFP_OK)
	{
		DPRINT(("[afpSaveNewUser]User already exists!\n"));
		return( be_afp_useralreadyexists );
	}
	
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{
		dir.SetTo(path.Path());
		dir.CreateFile(AFP_PREFS_FILE_NAME, NULL, true);
		
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
				
		if (file.InitCheck() == B_OK)
		{
			AFP_USER_DATA	temp;
			uint32			index = 0;
			
			sprintf(attrName, "%s%s", AFP_USERS_TYPE, userName);
			
			//
			//Now store the data in the structure.
			//
			memset(&userData, 0, sizeof(AFP_USER_DATA));
			
			strcpy(userData.username, userName);
			strcpy(userData.password, password);
			
			userData.flags = flags;
			
			//
			//Find the next user id to assign to this one
			//
			while(afpGetIndUser(&temp, index) == B_OK)
				index++;
			
			userData.id = index;
			
			//
			//Now, set the group id based on the type of user this is
			//
			if (userData.flags & kIsAdmin)
				userData.group = AFP_HAIKU_GROUP_ADMINS_ID;
			else if (strcmp(userName, AFP_GUEST_NAME) == 0)
				userData.group = AFP_HAIKU_GROUP_GUESTS_ID;
			else
				userData.group = AFP_HAIKU_GROUP_USERS_ID;
			
			size = file.WriteAttr(attrName, 0, 0, &userData, sizeof(AFP_USER_DATA));
			
			//
			//Now update the user schema version information.
			//
			if (strcmp(userName, AFP_GUEST_NAME) == 0)
			{
				//
				//The guest account is only created when the perfs file does not
				//exist (clean install). We use this opportunity to set record the
				//schema version in the attribute.
				//
				uint32 vers = AFP_USERDB_VERSION;
				
				size = file.WriteAttr(AFP_USERS_SCHEMA_VERSION, 0, 0, &vers, sizeof(vers));
				
				if (size != sizeof(vers))
				{
					DPRINT(("[afpSaveNewUser]Failed to write version attribute! (%lu)\n", size));
				}
			}
		}
		else
		{
			DPRINT(("[afpSaveNewUser]Failed to open user data file for write\n"));
			return( be_afp_fileoperationfailed );
		}
	}
	
	return( AFP_OK );
}


/*
 * afpGetUserDataByID()
 *
 * Description:
 *		Returns the user data that corresponds to the passed ID. ID's start at 0.
 *
 * Returns: AFPERROR
 */

AFPERROR afpGetUserDataByID(
	AFP_USER_DATA*	userData,
	uint32			uID
	)
{
	AFP_USER_DATA	temp;
	uint32			index = 0;
	
	while(afpGetIndUser(&temp, index) == B_OK)
	{		
		if (temp.id == uID)
		{
			if (userData != NULL) {
			
				memcpy(userData, &temp, sizeof(temp));
			}
			
			return( B_OK );
		}
		
		index++;
	}
	
	return( be_afp_usernotfound );
}


/*
 * afpGetIndUser()
 *
 * Description:
 *		Returns the user at index n. Index starts at 0.
 *
 * Returns: AFPERROR
 */

AFPERROR afpGetIndUser(
	AFP_USER_DATA*	userData,
	int16			index
	)
{
	BPath		path;
	BNode		file;
	char		fpath[256];
	char		attrName[B_ATTR_NAME_LENGTH];
	int16		count	= 0;
	ssize_t		size 	= 0;
	
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{	
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
		
		if (file.InitCheck() == B_OK)
		{
			while(file.GetNextAttrName(attrName) == B_OK)
			{
				if (strstr(attrName, AFP_USERS_TYPE))
				{
					if (count != index) {
						
						count++;
						continue;
					}
					
					memset(userData, 0, sizeof(AFP_USER_DATA));
					
					size = file.ReadAttr(attrName, 0, 0, userData, sizeof(AFP_USER_DATA));
					
					if (size > B_OK)
					{
						//DPRINT(("[afpGetIndUser]Returning %s user\n", userData->username));
						return( AFP_OK );
					}
					else
					{
						DPRINT(("[afpGetIndUser]Zero data read from attribute!\n"));
						continue;
					}
				}
			}
			
			return( be_afp_nomorerecords );
		}
		else
		{
			DPRINT(("[afpGetIndUser]InitCheck() failed on data file (%s)\n",
					GET_BERR_STR(file.InitCheck())));
		}
	}
	
	return( be_afp_fileoperationfailed );
}


/*
 * afpGetUserDataByName()
 *
 * Description:
 *		Returns the user data associated with a username.
 *
 * Returns: AFPERROR
 */

AFPERROR afpGetUserDataByName(
	const char*		userName,
	AFP_USER_DATA*	userData
	)
{
	AFP_USER_DATA	tempData;
	int16			i = 0;
	
	memset(&tempData, 0, sizeof(AFP_USER_DATA));
	
	while(afpGetIndUser(&tempData, i) == AFP_OK)
	{
		i++;
		
		if (!strcmp(tempData.username, userName))
		{
			if (userData != NULL) {
			
				memcpy(userData, &tempData, sizeof(AFP_USER_DATA));
			}
			
			return( AFP_OK );
		}
	}
	
	return( be_afp_usernotfound );
}


/*
 * afpDeleteUser()
 *
 * Description:
 *		Deletes a user from the user file.
 *
 * Returns: AFPERROR
 */

AFPERROR afpDeleteUser(
	const char*		userName
	)
{
	BPath		path;
	BNode		file;
	BDirectory	dir;
	char		fpath[256];
	char		attrName[B_ATTR_NAME_LENGTH];
	ssize_t		size 	= 0;
	
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{		
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
				
		if (file.InitCheck() == B_OK)
		{
			sprintf(attrName, "%s%s", AFP_USERS_TYPE, userName);	
			size = file.RemoveAttr(attrName);
		}
	}
	
	return( AFP_OK );
}


/*
 * afpUpdateUserInfo()
 *
 * Description:
 *		Updates a users entry in the user database.
 *
 * Returns: AFPERROR
 */

AFPERROR afpUpdateUserInfo(
	AFP_USER_DATA	userData
	)
{
	BPath			path;
	BNode			file;
	char			fpath[256];
	char			attrName[B_ATTR_NAME_LENGTH];
	AFP_USER_DATA	oldData;
	ssize_t			size 	= 0;
	
	//
	//Make sure the user name already exists in our settings file.
	//
	if (afpGetUserDataByName(userData.username, &oldData) != AFP_OK)
	{
		DPRINT(("[afpUpdateUserInfo]User does not exist! [%s]\n", userData.username));
		return( be_afp_usernotfound );
	}

	//
	//The configuration API does not maintain user or group ID's, these
	//are hidden from the API. So, we have to ensure they are set correctly
	//when updating the user information.
	//
	
	userData.id		= oldData.id;
	userData.group	= oldData.group;
	
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
				
		if (file.InitCheck() == B_OK)
		{
			sprintf(attrName, "%s%s", AFP_USERS_TYPE, userData.username);
			
			size = file.WriteAttr(attrName, 0, 0, &userData, sizeof(AFP_USER_DATA));
			
			if (size <= 0)
				return( size );
			else
				return( B_OK );
		}
	}
	
	return( -1 );
}


/*
 * afpGetUserDBVersion()
 *
 * Description:
 *		Returns the version of the user database if it exists.
 *
 * Returns: AFPERROR
 */

AFPERROR afpGetUserDBVersion(uint32* version)
{
	BPath			path;
	BNode			file;
	char			fpath[256];
	ssize_t			size = 0;
	uint32			vers = 0;
	
	*version = 0;

	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
				
		if (file.InitCheck() == B_OK)
		{			
			size = file.ReadAttr(AFP_USERS_SCHEMA_VERSION, 0, 0, &vers, sizeof(vers));
			
			if (size <= 0)
			{
				//
				//In the event of an error, size contains the error code.
				//
				return( size );
			}
			else
			{
				//
				//Success, set the return parameter with the version
				//
				*version = vers;
				
				return( B_OK );
			}
		}
	}
	
	return( be_afp_userdbnotfound );
}


/*
 * afpVerifyUserDatabase()
 *
 * Description:
 *		This function should be called when the server is starting up.
 *		It verifies the user database is at the correct version and
 *		should take appropriate action if not (i.e. upgrade the db).
 *
 * Returns: AFPERROR
 */

AFPERROR afpVerifyUserDatabase()
{
	uint32		vers 	= 0;
	status_t	status	= 0;
	
	status = afpGetUserDBVersion(&vers);
	
	if ((status == B_OK) || (status == B_ENTRY_NOT_FOUND))
	{
		if (vers < AFP_USERDB_VERSION)
		{
			//
			//The version in the existing prefs file is old, or the version
			//attribute does not exist (again, the version is old).
			//
			
			BPath			path;
			BNode			file;
			BDirectory		dir;
			char			fpath[256];
			AFP_USER_DATA	temp;
			ssize_t			size  = 0;
			uint32			index = 0;
			
			DPRINT(("[afpVerifyUserDatabase]User db schema too old, upgrading...\n"));
			
			switch(vers)
			{
				//
				//A version of 0 means there was no version attribute at all, this
				//means we're upgrading from v1 of the user schema. The only difference
				//is that user & group ID's are now used.
				//
				case 0:
					DPRINT(("[afpVerifyUserDatabase]Upgrading user schema from v1...\n"));
					
					while(afpGetIndUser(&temp, index) == B_OK)
					{
						//
						//Set the new user ID to the next available ID.
						//
						temp.id = index;
						
						//
						//Now, set the group id based on the type of user this is
						//
						if (temp.flags & kIsAdmin)
							temp.group = AFP_HAIKU_GROUP_ADMINS_ID;
						else if (strcmp(temp.username, AFP_GUEST_NAME) == 0)
							temp.group = AFP_HAIKU_GROUP_GUESTS_ID;
						else
							temp.group = AFP_HAIKU_GROUP_USERS_ID;
							
						afpUpdateUserInfo(temp);
							
						index++;
					}
					
					status = be_afp_userdbupgraded;
					break;
				
				default:
					break;
			}
			
			//
			//Update the version attribute with the latest version number
			//so we won't come here again for no reason.
			//
			
			vers = AFP_USERDB_VERSION;
			
			if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
			{		
				sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
				file.SetTo(fpath);
						
				if (file.InitCheck() == B_OK)
				{
					size = file.WriteAttr(AFP_USERS_SCHEMA_VERSION, 0, 0, &vers, sizeof(vers));
					
					if (size != sizeof(vers))
					{
						DPRINT(("[afpVerifyUserDatabase]Failed to write out new version attribute!\n"));
					}
				}
			}
		}
	}
	
	return( status );
}


/*
 * afpAccountEnabled()
 *
 * Description:
 *		Quick and dirty call to see if an account is enabled.
 *
 * Returns: bool
 */

bool afpAccountEnabled(
	const char*		userName
	)
{
	AFP_USER_DATA	userData;
	AFPERROR		afpError = AFP_OK;
	
	afpError = afpGetUserDataByName(userName, &userData);
	
	if (AFP_SUCCESS(afpError))
	{
		if (userData.flags & kUserEnabled) {
		
			return( true );
		}
	}
	
	return( false );
}
	










	





