#include <sys/select.h>

#include "afpdhxlogin.h"
#include "afp_session.h"
#include "afp_buffer.h"


/*
 * DHXLogin()
 *
 * Description:
 *		Begin the login sequence for the DHCAST128 UAM.
 *
 * Returns: AFPERROR
 */

AFPERROR DHXLogin(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize,
	char*			afpUserName,
	DHXInfo*		dhxInfo
)
{
	afp_buffer			afpBuffer(afpReqBuffer);
	afp_buffer			afpReply(afpReplyBuffer);
	unsigned char		iv[] 		= "CJalbert";
	uint8				p[]			= {	0xBA, 0x28, 0x73, 0xDF, 0xB0, 0x60, 0x57, 0xD4,
										0x3F, 0x20, 0x24, 0x74, 0x4C, 0xEE, 0xE7, 0x5B	};
	uint8				g			= 0x07;
	BIGNUM				*gbn		= NULL;
	BIGNUM				*pbn		= NULL;
	DH					*dh			= NULL;
	AFPERROR			afpError	= afpParmErr;
	uint16				sessid;
	uint8				randbuf[DHX_RANDBUFSIZE];
	unsigned char encrypt_buffer[DHX_CRYPTBUFLEN] = {};
	int					i;
	
	if (dhxInfo == NULL)
	{
		DPRINT(("[afp:DHXLogin]dhxInfo invalid\n"));
		return( afpParmErr );
	}
	
	// Obtain the client's public key from the request.
	BIGNUM* client_pub_key = BN_bin2bn((unsigned char*)afpBuffer.GetCurrentPosPtr(), DHX_KEYSIZE, NULL);
	if (client_pub_key == NULL)
	{
		DPRINT(("[afp:DHXLogin]BN_bin2bn() failed obtaining the client's key\n"));
		goto exit;
	}
	
	// Obtain our primes
	gbn = BN_bin2bn(&g, sizeof(g), NULL);
	if (gbn == NULL)
	{
		DPRINT(("BN_bin2bn() failed obtaining our primes1\n"));
		goto exit;
	}

    pbn = BN_bin2bn(p, sizeof(p), NULL);
	if (pbn == NULL)
	{
		DPRINT(("BN_bin2bn() failed obtaining our primes2\n"));
		goto exit;
	}

    dh = DH_new();
	if (dh == NULL)
	{
		DPRINT(("DH_new() failed creating new DH context\n"));
		goto exit;
	}

	if (!DH_set0_pqg(dh, pbn, NULL, gbn))
	{
		DPRINT(("DH_set0_pqg() failed.\n"));
		goto exit;
	}

	// Generate the key
	if (!DH_generate_key(dh))
	{
		DPRINT(("DH_generate_key() failed generating our public keys\n"));
		goto exit;
	}
	
	const BIGNUM* pub_key;
	DH_get0_key(dh, &pub_key, NULL);
	if (BN_num_bytes(pub_key) > DHX_KEYSIZE)
	{
		ASSERT(0);
	}

	CAST_KEY cast_key;
    i = DH_compute_key((unsigned char*)afpReply.GetCurrentPosPtr(), client_pub_key, dh);
 	CAST_set_key(&cast_key, i, (unsigned char*)afpReply.GetCurrentPosPtr());

	afpReply.AddInt16(sessid);

	BN_bn2bin(pub_key, (unsigned char*)afpReply.GetCurrentPosPtr());
	afpReply.Advance(DHX_KEYSIZE);

 	afpGetRandomNumber(afpSession, (char*)randbuf, sizeof(randbuf));

	memcpy(encrypt_buffer, &randbuf, sizeof(randbuf));
	memset(encrypt_buffer + DHX_RANDBUFSIZE, 0, DHX_RANDBUFSIZE);

	CAST_cbc_encrypt(
		encrypt_buffer,
		(unsigned char*)afpReply.GetCurrentPosPtr(),
		DHX_CRYPTBUFLEN,
		&cast_key,
		iv,
		CAST_ENCRYPT
	);

	afpReply.Advance(DHX_CRYPTBUFLEN);

	memcpy(dhxInfo->randomBuffer, randbuf, sizeof(randbuf));
	memcpy(&dhxInfo->castkey, &cast_key, sizeof(cast_key));
	strcpy(dhxInfo->userName, afpUserName);
	dhxInfo->uamLoginType = afpUAMDHCAST128;
	dhxInfo->sessionID = sessid;
	
	*afpDataSize = afpReply.GetDataLength();

	afpError = afpAuthContinue;
						
exit:
	if (client_pub_key)
    {
        BN_free(client_pub_key);
    }

	if (dh)
    {
        DH_free(dh);
    }
	
	return( afpError );
}


/*
 * DHXLoginContinue()
 *
 * Description:
 *		Continue a login sequence for the DHCAST128 UAM.
 *
 *		Note that this funciton expects the afpReqBuffer to point at
 *		the positino just past the afp command and flags fields.
 *
 * Returns: AFPERROR
 */

AFPERROR DHXLoginContinue(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	char*			afpPassword,
	int16			afpPasswordLen,
	DHXInfo*		dhxInfo
)
{
	afp_buffer		afpBuffer(afpReqBuffer);
	unsigned char	iv[] 		= "LWallace";
	int16			sessionID	= 0;
	AFPERROR		afpError	= afpUserNotAuth;
	
	if (dhxInfo == NULL)
	{
		DPRINT(("[afpuser:DHXLoginContinue]dhxInfo invalid\n"));
		return( afpMiscErr );
	}
		
	sessionID = afpBuffer.GetInt16();
	
	if (sessionID != dhxInfo->sessionID)
	{
		DPRINT(("[afpuser:DHXLoginContinue]Session ID's don't match! (%d vs. %lu)\n", sessionID, dhxhash(afpSession)));
		return( afpParmErr );
	}
	
	CAST_cbc_encrypt(
			(unsigned char*)afpBuffer.GetCurrentPosPtr(),
			(unsigned char*)afpReplyBuffer,
			DHX_CRYPT2BUFLEN,
			&dhxInfo->castkey,
			iv,
			CAST_DECRYPT
			);
	
	afpError = DHXCheckRandnum((unsigned char*)afpReplyBuffer, dhxInfo->randomBuffer);
	
	if (!AFP_SUCCESS(afpError))
	{
		DPRINT(("[afpuser:DHXLoginContinue]Randnum check failed, exiting\n"));
		goto exit;
	}

	memset(afpReplyBuffer, 0, DHX_RANDBUFSIZE);
		
	afpReplyBuffer += DHX_RANDBUFSIZE;
	afpReplyBuffer[DHX_PASSWDLEN] = '\0';
	
	if (DHX_PASSWDLEN > afpPasswordLen)
	{
		DPRINT(("[afpuser:DHXLoginContinue]Password buffer too small\n"));
				
		afpError = afpParmErr;
		goto exit;
	}
	
	memcpy(afpPassword, afpReplyBuffer, DHX_PASSWDLEN);
		
	DPRINT(("[afpuser:DHXLoginContinue]Successful continued login for DHCAST128\n"));
	afpError = AFP_OK;
	
exit:
	memset(afpReplyBuffer, 0, DHX_CRYPT2BUFLEN);
		
	return( afpError );
}


/*
 * DHXChangePassword()
 *
 * Description:
 *		Grab the encrypted old and new passwords.
 *
 * Returns: AFPERROR
 */

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
)
{
	afp_buffer		afpBuffer(afpReqBuffer);
	unsigned char	iv[] 		= "LWallace";
	int16			sessionID	= 0;
	AFPERROR		afpError 	= afpParmErr;
	
	if (dhxInfo == NULL)
	{
		DPRINT(("[afpuser:DHXChangePassword]dhxInfo invalid\n"));
		return( afpMiscErr );
	}
	
	sessionID = afpBuffer.GetInt16();
	
	if (sessionID == 0)
	{
		DPRINT(("[afpuser:DHXChangePassword]First time through, calling DHXLogin()\n"));
		
		return( DHXLogin(
					afpSession,
					afpBuffer.GetCurrentPosPtr(),
					afpReplyBuffer,
					afpDataSize,
					userName,
					dhxInfo)		);
	}
	
	DPRINT(("[afpuser:DHXChangePassword]Second time through, completing decryption\n"));
	
	if (sessionID != dhxInfo->sessionID)
	{
		DPRINT(("[afpuser:DHXChangePassword]Session ID's don't match! (%d vs. %lu)\n", sessionID, dhxhash(afpSession)));
		return( afpParmErr );
	}
	
	CAST_cbc_encrypt(
			(unsigned char*)afpBuffer.GetCurrentPosPtr(),
			(unsigned char*)afpReplyBuffer,
			DHX_CHNGPSWDBUFLEN,
			&dhxInfo->castkey,
			iv,
			CAST_DECRYPT
			);
	
	afpError = DHXCheckRandnum((unsigned char*)afpReplyBuffer, dhxInfo->randomBuffer);
	
	if (!AFP_SUCCESS(afpError))
	{
		DPRINT(("[afpuser:DHXChangePassword]Randnum check failed, exiting with parmErr\n"));
		goto exit;
	}
	
	memset(afpReplyBuffer, 0, DHX_RANDBUFSIZE);
	afpReplyBuffer += DHX_RANDBUFSIZE;
	
	afpReplyBuffer[DHX_PASSWDLEN*2] = '\0';
	
	strncpy(oldPassword, (char*)&afpReplyBuffer[DHX_PASSWDLEN], cbOldPassword);
	
	afpReplyBuffer[DHX_PASSWDLEN] = '\0';
	strncpy(newPassword, (char*)afpReplyBuffer, cbNewPassword);
	
	DPRINT(("[afpuser:DHXChangePassword]Old password: %s\n", oldPassword));
	DPRINT(("[afpuser:DHXChangePassword]New password: %s\n", newPassword));
	
	afpError = AFP_OK;
	
exit:
	memset(afpReplyBuffer, 0, DHX_PASSWDLEN*2);
		
	DPRINT(("[afpuser:DHXChangePassword]Returning: %lu\n", afpError));
	
	return( afpError );
}


/*
 * DHXCheckRandnum()
 *
 * Description:
 *		For DHX, the client takes a 16 byte random number we pass in DHXLogin()
 *		and increments it by 1. Check to make sure it is incremented.
 *
 * Returns: AFPERROR if not incremented
 */

AFPERROR DHXCheckRandnum(
	unsigned char*	clientBuf,
	unsigned char*	serverBuf
)
{
	BIGNUM		*bn1		= NULL;
	BIGNUM		*bn2		= NULL;
	BIGNUM		*bnResult	= NULL;
	AFPERROR	afpError	= afpParmErr;
	
	bn1 = BN_bin2bn(clientBuf, DHX_RANDBUFSIZE, NULL);
	
	if (bn1 == NULL)
	{
		DPRINT(("[afpuser:DHXCheckRandnum]Failed to extract random number key\n"));
		goto exit;
	}
	
	bn2 = BN_bin2bn(serverBuf, DHX_RANDBUFSIZE, NULL);
	
	if (bn2 == NULL)
	{
		DPRINT(("[afpuser:DHXCheckRandnum]Failed to get random number\n"));
		goto exit;
	}
	
	bnResult = BN_new();
	
	if (bnResult == NULL)
	{
		DPRINT(("[afpuser:DHXCheckRandnum]Failed to create BIGNUM 3\n"));
		goto exit;
	}
	
	BN_sub(bnResult, bn1, bn2);
	
	if (!BN_is_one(bnResult))
	{
		DPRINT(("[afpuser:DHXCheckRandnum]Random was not incremented by 1 by client!\n"));
		goto exit;
	}
	
	afpError = AFP_OK;
	
exit:

	if (bn1)		BN_free(bn1);
	if (bn2)		BN_free(bn2);
	if (bnResult)	BN_free(bnResult);
	
	DPRINT(("[afpuser:DHXCheckRandnum]Returning: %lu\n", afpError));
	
	return( afpError );
	
}


/*
 * afpGetRandomNumber()
 *
 * Description:
 *		Returns a random number for use in encrypted UAMs
 *
 * Returns: AFPERROR
 */

void afpGetRandomNumber(
	afp_session* 	afpSession,
	char* 			buf,
	int16 			cbBuf
)
{
	struct 	timeval		tv;
	struct 	timezone	tz;
	uint32	result;
	int		i;
	
	if (gettimeofday(&tv, &tz) >= 0)
	{
		srandom(tv.tv_sec + (unsigned long)afpSession);
		
		for (i = 0; i < cbBuf; i += sizeof(result))
		{
			result = random();
			
			memcpy(buf + i, &result, sizeof(result));
		}
	}
}





