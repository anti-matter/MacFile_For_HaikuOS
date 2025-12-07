#ifndef __afplogon__
#define __afplogon__

#include "afp.h"

typedef struct
{
	char	username[UAM_MAXPASSWORDLEN+1];
	char	password[UAM_MAXPASSWORDLEN+1];
	
	uint32	group;
	uint32	flags;
	uint32	id;
	
	uint32	reserved2;
}AFP_USER_DATA;

AFPERROR afpImpChangePswd(
	char*			userName,
	char*			oldPassword,
	char*			newPassword
	);
	
AFPERROR afpImpLogonUser(
	const char*		userName,
	char*			password,
	AFP_USER_DATA*	userInfo
	);

AFPERROR afpSaveNewUser(
	const char*		userName,
	const char*		password,
	uint32			flags
	);

AFPERROR afpGetUserDataByID(
	AFP_USER_DATA*	userData,
	uint32			uID
	);

AFPERROR afpGetIndUser(
	AFP_USER_DATA*	userData,
	int16			index
	);
	
AFPERROR afpGetUserDataByName(
	const char*		userName,
	AFP_USER_DATA*	userData
	);
	
AFPERROR afpDeleteUser(
	const char*		userName
	);
	
AFPERROR afpUpdateUserInfo(
	AFP_USER_DATA	userData
	);

AFPERROR afpGetUserDBVersion(
	uint32* version
	);

AFPERROR afpVerifyUserDatabase();
	
bool afpAccountEnabled(
	const char*		userName
	);
	
#endif //__afplogon__
