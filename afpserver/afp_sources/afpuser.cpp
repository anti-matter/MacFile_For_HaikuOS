#include <StorageKit.h>

#include "commands.h"
#include "afpGlobals.h"
#include "afp.h"
#include "afplogon.h"
#include "afpuser.h"
#include "afpdhxlogin.h"
#include "afp_buffer.h"
#include "afp_session.h"

extern int16	gMaxAFPSessions;


/*
 * afpGetUAMType()
 *
 * Description:
 *		Returns a UAM type code based on the supplied UAM string.
 *
 * Returns: 0 if error, uam version otherwise
 */

int8 afpGetUAMType(
	char* uamString
)
{
	int8	uamResult = 0;
	
	if (!strcmp(uamString, UAM_NONE_STR))
		uamResult = afpUAMGuest;
	else if (!strcmp(uamString, UAM_CLEAR_TEXT))
		uamResult = afpUAMClearText;
	else if (!strcmp(uamString, UAM_DHCAST128))
		uamResult = afpUAMDHCAST128;
		
	return( uamResult );
}


/*
 * afpGetAFPVersion()
 *
 * Description:
 *		Returns a AFP version code based on the supplied AFP Version string.
 *
 * Returns: 0 if error, version otherwise
 */

int8 afpGetAFPVersion(
	char* afpString
)
{
	int8	version = 0;
	
	if (!strcmp(afpString, AFP_22_VERSION_STR))
		version = afpVersion22;
	else if (!strcmp(afpString, AFP_30_VERSION_STR))
		version = afpVersion30;
	else if (!strcmp(afpString, AFP_31_VERSION_STR))
		version = afpVersion31;
	else if (!strcmp(afpString, AFP_32_VERSION_STR))
		version = afpVersion32;
	else if (!strcmp(afpString, AFP_33_VERSION_STR))
		version = afpVersion33;
		
	return( version );
}


/*
 * FPLogin()
 *
 * Description:
 *		Handle a login request to this AFP server.
 *
 * Returns: AFPERROR
 */

AFPERROR FPLogin(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)
	
	afp_buffer		afpBuffer(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			szAFPVersion[MAX_AFP_VERSION_LEN+1];
	char			szUAMVersion[MAX_UAM_STRING_LEN+1];
	char			szUserName[UAM_USERNAMELEN+1];
	char			afpPathname[MAX_AFP_PATH];
	AFP_USER_DATA	userInfo;
	int16			afpFlags		= 0;
	int8			afpCommand		= 0;
	int8			afpUserNameType	= 0;
	int8			afpUAMType		= 0;
	int8			afpAFPVersion	= 0;
	int8			afpPathType 	= 0;
	AFPERROR		afpError 		= AFP_OK;
	
	DPRINT(("[afp:FPLogin]ENTER\n"));
	
	//
	//Check for maximum logon sessions limit being reached. If it is, we
	//just return afpAccessDenied.
	//
	if (gMaxAFPSessions != 0)
	{
		//
		//Implement
		//
	}
	
	//
	//If the session is already authenticated, there's no reason to continue.
	//
	if (afpSession->IsAuthenticated())
	{
		DPRINT(("[afp:FPLogin]User already authenticated\n"));
		return( afpMiscErr );
	}
	
	//
	//The first byte in the buffer is the afp command byte.
	//
	afpCommand = afpBuffer.GetInt8();
	
	if (afpCommand == afpLoginExt)
	{
		afpBuffer.Advance(sizeof(int8));
		afpFlags = afpBuffer.GetInt16();
	}
		
	//
	//Get the AFP version the client is requesting to use and store it.
	//
	afpBuffer.GetString(szAFPVersion, sizeof(szAFPVersion));
	DPRINT(("[afp:FPLogin]AFPVersion = %s\n", szAFPVersion));
	
	afpAFPVersion = afpGetAFPVersion(szAFPVersion);
	
	if (afpAFPVersion == 0)
	{
		DPRINT(("[afp:FPLogin]Unsupported AFP version\n"));
		return( afpBadVersNum );
	}
	
	afpSession->SetAFPVersion(afpAFPVersion);
	
	//
	//Retrieve the UAM type string, convert it to an int, and then
	//store the uam type in the session object.
	//
	afpBuffer.GetString(szUAMVersion, sizeof(szUAMVersion));
	DPRINT(("[afp:FPLogin]UAMVersion = %s\n", szUAMVersion));

	afpUAMType = afpGetUAMType(szUAMVersion);
	
	if (afpUAMType == 0)
	{
		DPRINT(("[afp:FPLogin]Unsupported UAM version\n"));
		return( afpBadUAM );
	}
	
	afpSession->SetUAMLoginType(afpUAMType);
	
	//
	//Every UAM other then guest requires the user name, so retrieve it now.
	//
	if (afpUAMType != afpUAMGuest)
	{
		if (afpCommand == afpLoginExt)
		{
			afpUserNameType	= afpBuffer.GetInt8();			
			afpBuffer.GetString(szUserName, sizeof(szUserName), false, afpUserNameType);
			
			afpPathType = afpBuffer.GetInt8();
			afpBuffer.GetString(afpPathname, sizeof(afpPathname), false, afpPathType);
		}
		else
		{
			afpBuffer.GetString(szUserName, sizeof(szUserName));
		}
		
		afpBuffer.AdvanceIfOdd();	//Padding byte
	}
	
	switch(afpUAMType)
	{
		case afpUAMGuest:
		{
			DPRINT(("[afp:FPLogin]Guest login\n"));
			
			afpError = afpImpLogonUser(AFP_GUEST_NAME, NULL, &userInfo);

			//
			//If we get here without error, then we are authenticated.
			//
			if (AFP_SUCCESS(afpError))
			{
				afpSession->SetIsAuthenticated(true);
				afpSession->SetUAMLoginInfo(&userInfo);
			}
		}
		break;
		
		case afpUAMClearText:
		{
			char	szPassword[UAM_CLRTXTPWDLEN+1] = {};

			afpBuffer.GetRawData(szPassword, UAM_CLRTXTPWDLEN);
			
			DPRINT(("[afp:FPLogin]Username = %s\n", szUserName));
			DPRINT(("[afp:FPLogin]Password = %s\n", szPassword));
			
			afpError = afpImpLogonUser(szUserName, szPassword, &userInfo);
	
			if ((AFP_SUCCESS(afpError)) || (afpError == afpPwdExpiredErr))
			{
				if (afpError == afpPwdExpiredErr)
				{
					//
					//Flag for temporary login, allows only the changing of
					//passwords for expired accounts.
					//
					userInfo.flags |= kTempAuthenticated;
				}
				
				afpSession->SetIsAuthenticated(true);				
				afpSession->SetUAMLoginInfo(&userInfo);
			}
		}
		break;
		
		case afpUAMDHCAST128:
		{
			//
			//This login UAM uses SSL to encrypt the password buffer using
			//public and private keys.
			//
			
			DHXInfo*	dhxInfo = NULL;
			
			//
			//This is where we store all the information we'll need for the login continue.
			//
			dhxInfo = (DHXInfo*)afpSession->BeginExtendedLogin(sizeof(DHXInfo));
			
			if (dhxInfo == NULL)
			{
				//
				//We failed to allocated the necessary block of data to store information
				//between login api calls. No choice but to bail out.
				//
				
				DPRINT(("[afp:FPLogin]Failed to allocate DHXInfo structure, exiting.\n"));
				return( afpMiscErr );
			}
					
			DPRINT(("[afp:FPLogin]Username = %s\n", szUserName));
						
			afpError = DHXLogin(
							afpSession,
							afpBuffer.GetCurrentPosPtr(),
							afpReplyBuffer,
							afpDataSize,
							szUserName,
							dhxInfo
							);
		}
		break;
		
		default:
			return( afpBadUAM );
	}
		
	DPRINT(("[afp:FPLogin]Returning %lu\n", afpError));
	
	return( afpError );
}


/*
 * FPContLogin()
 *
 * Description:
 *		Continue a login sequence.
 *
 * Returns: AFPERROR
 */

AFPERROR FPContLogin(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpBuffer(afpReqBuffer);
	AFP_USER_DATA	userInfo;
	int8			uamLogonType 	= afpSession->GetUAMLoginType();
	AFPERROR		afpError 		= afpUserNotAuth;
	
	afpBuffer.Advance(sizeof(int16));
	
	DPRINT(("[afp:FPContLogin]Enter\n"));
	DPRINT(("[afp:FPContLogin]LogonUAM = %d\n", uamLogonType));

	switch(uamLogonType)
	{
		case afpUAMDHCAST128:
		{
			char 		szPassword[DHX_PASSWDLEN] = {};
			DHXInfo*	dhxInfo = (DHXInfo*)afpSession->GetExtendedLoginInfo();
			
			if (dhxInfo == NULL)
			{
				DPRINT(("[afp:FPContLogin]DHXInfo is invalid\n"));
				return( afpMiscErr );
			}
						
			afpError = DHXLoginContinue(
							afpSession,
							afpBuffer.GetCurrentPosPtr(),
							afpReplyBuffer,
							szPassword,
							sizeof(szPassword),
							dhxInfo
							);
			
			if (AFP_SUCCESS(afpError))
			{
				afpError = afpImpLogonUser(
								dhxInfo->userName,
								szPassword,
								&userInfo
								);
				
				DPRINT(("[afp:FPContLogin]afpImpLogonUser() returned: %lu\n", afpError));
			}
			
			memset(szPassword, 0, sizeof(szPassword));
			afpSession->EndExtendedLogin();
			break;
		}
		
		default:
			return( afpParmErr );
	}
	
	if ((AFP_SUCCESS(afpError)) || (afpError == afpPwdExpiredErr))
	{
		if (AFP_SUCCESS(afpError)) {
		
			afpSession->SetIsAuthenticated(true);
		}
		else
		{
			//
			//Flag for temporary login, allows only the changing of
			//passwords for expired accounts.
			//
			userInfo.flags |= kTempAuthenticated;
		}
		
		afpSession->SetUAMLoginInfo(&userInfo);
	}
	
	DPRINT(("[afp:FPContLogin]Returning: %lu\n", afpError));
	
	return( afpError );
}


/*
 * FPLogout()
 *
 * Description:
 *		Handle a logout request to this AFP server.
 *
 * Returns: AFPERROR
 */

AFPERROR FPLogout(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpReqBuffer)
	#pragma unused(afpReplyBuffer)
	#pragma unused(afpDataSize)
	
	AFP_USER_DATA	userInfo;
	
	DPRINT(("[afp:FPLogout]Enter...\n"));
	
	afpSession->SetIsAuthenticated(false);
	
	//
	//Take away temporary access if it was granted.
	//
	afpSession->GetUserInfo(&userInfo);
	if (userInfo.flags & kTempAuthenticated)
	{
		userInfo.flags &= ~kTempAuthenticated;
		afpSession->SetUAMLoginInfo(&userInfo);
	}
	
	return( AFP_OK );
}


/*
 * FPChangePswd()
 *
 * Description:
 *		Change a user's password via the client.
 *
 * Returns: AFPERROR
 */

AFPERROR FPChangePswd(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	char			szUAMVersion[MAX_UAM_STRING_LEN+1];
	char			szOldPassword[UAM_MAXPASSWORDLEN+1];
	char			szNewPassword[UAM_MAXPASSWORDLEN+1];
	char			szUserName[UAM_USERNAMELEN+1];
	int8			uamType		= 0;
	AFPERROR		afpError 	= AFP_OK;
	
	DPRINT(("[afpuser:FPChangePswd]Enter...\n"));
	
	afpRequest.Advance(sizeof(int16));
	
	afpRequest.GetString(szUAMVersion, sizeof(szUAMVersion));
	uamType = afpGetUAMType(szUAMVersion);
	
	afpRequest.AdvanceIfOdd();
		
	//
	//Starting with AFP3.0, the username is just 2 bytes of zeros.
	//
	if (afpSession->GetAFPVersion() > afpVersion22)
	{
		afpRequest.Advance(sizeof(int16));
		strcpy(szUserName, afpSession->GetUserName());
	}
	else
	{		
		afpRequest.GetString(szUserName, sizeof(szUserName));
		afpRequest.AdvanceIfOdd();
	}

	DPRINT(("[afpuser:FPChangePswd]UAM Str:  %s\n", szUAMVersion));
	DPRINT(("[afpuser:FPChangePswd]Username: %s\n", szUserName));
	
	switch(uamType)
	{
		case afpUAMGuest:
			return( afpBadUAM );
		
		case afpUAMClearText:
			//
			//Grab the old and new cleartext passwords from the buffer.
			//
			afpRequest.GetRawData(szOldPassword, UAM_CLRTXTPWDLEN);
			szOldPassword[UAM_CLRTXTPWDLEN+1] = 0;
			afpRequest.GetRawData(szNewPassword, UAM_CLRTXTPWDLEN);
			szNewPassword[UAM_CLRTXTPWDLEN+1] = 0;
			break;
		
		case afpUAMRandnumExchange:
			return( afpBadUAM );
			
		case afpUAMDHCAST128:
		{
			DHXInfo* dhxInfo = NULL;
			
			//
			//This may be our second time through in which case dxhInfo is already
			//created and filled with information.
			//
			dhxInfo = (DHXInfo*)afpSession->GetExtendedLoginInfo();
			
			if (dhxInfo == NULL)
			{
				//
				//Must be our first time through, so no blob info saved yet.
				//
				dhxInfo = (DHXInfo*)afpSession->BeginExtendedLogin(sizeof(DHXInfo));
				
				if (dhxInfo == NULL)
				{
					//
					//We failed to allocated the necessary block of data to store information
					//between login api calls. No choice but to bail out.
					//
					
					DPRINT(("[afp:FPChangePswd]Failed to allocate DHXInfo structure, exiting.\n"));
					return( afpMiscErr );
				}
			}
			
			memset(szOldPassword, 0, sizeof(szOldPassword));
			memset(szNewPassword, 0, sizeof(szNewPassword));
			
			afpError = DHXChangePassword(
							afpSession,
							afpRequest.GetCurrentPosPtr(),
							afpReplyBuffer,
							afpDataSize,
							szUserName,
							szOldPassword,
							sizeof(szOldPassword),
							szNewPassword,
							sizeof(szNewPassword),
							dhxInfo
							);
			
			if (afpError == afpAuthContinue)
			{
				//
				//We've only complete the first step in the key exchange/encryption
				//process. Returning afpAuthContinue and we'll be called again with
				//the new/old password encrypted payload.
				//
				return( afpError );
			}
			
			afpSession->EndExtendedLogin();
			
			if (!AFP_SUCCESS(afpError))
			{
				//
				//We had a failure in the DHX change password routine.
				//
				
				goto exit;
			}
			break;
		}
		
		default:
			break;
	}
		
	afpError = afpImpChangePswd(
					szUserName,
					szOldPassword,
					szNewPassword
					);

exit:
	memset(szOldPassword, 0, sizeof(szOldPassword));
	memset(szNewPassword, 0, sizeof(szNewPassword));
	
	DPRINT(("[afpuser:FPChangePswd]Returning: %lu\n", afpError));
	
	return( afpError );
}





