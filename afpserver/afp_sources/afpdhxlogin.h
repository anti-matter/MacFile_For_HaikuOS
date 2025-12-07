#ifndef __afpdhxlogin__
#define __afpdhxlogin__

#include "afp.h"
#include "afpGlobals.h"

#define OPENSSL_API_COMPAT 0x00908000L

#include "openssl/bn.h"
#include "openssl/dh.h"
#include "openssl/cast.h"

#define DHX_KEYSIZE 		16
#define DHX_PASSWDLEN 		64
#define DHX_RANDBUFSIZE		16
#define DHX_CRYPTBUFLEN  	(DHX_KEYSIZE*2)
#define DHX_CRYPT2BUFLEN 	(DHX_KEYSIZE + DHX_PASSWDLEN)
#define DHX_CHNGPSWDBUFLEN	(DHX_KEYSIZE + (DHX_PASSWDLEN*2))

//hash a number to a 16-bit integer
#define dhxhash(a) ((((unsigned long) (a) >> 8) ^ (unsigned long) (a)) & 0xffff)

//
//The following is the blob we store between FPLogin() and 
//FPLoginCont() function calls.
//
typedef struct
{
	char		userName[UAM_USERNAMELEN+1];
	int16		sessionID;
	int8		uamLoginType;
	uint8		randomBuffer[16];
	CAST_KEY 	castkey;
	
}DHXInfo;

AFPERROR DHXLogin(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize,
	char*			afpUserName,
	DHXInfo*		dhxInfo
);

AFPERROR DHXLoginContinue(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	char*			afpPassword,
	int16			afpPasswordLen,
	DHXInfo*		dhxInfo
);

AFPERROR DHXChangePassword(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize,
	char*			userName,
	char*			oldPassword,
	int16			cbOldPassword,
	char*			newPassword,
	int16			cbNewPassword,
	DHXInfo*		dhxInfo
);

AFPERROR DHXCheckRandnum(
	unsigned char*	clientBuf,
	unsigned char*	serverBuf
);

void afpGetRandomNumber(
	afp_session* 	afpSession,
	char* 			buf,
	int16 			cbBuf
);

#endif //__afpdhxlogin__
