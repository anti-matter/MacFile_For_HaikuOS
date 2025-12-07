#include "afp_buffer.h"
#include "afpmsg.h"


char	g_afpLoginMessage[AFP_MSG_MAXLEN+sizeof(char)];
char	g_afpServerMessage[AFP_MSG_MAXLEN+sizeof(char)];

/*
 * FPGetServerMessage()
 *
 * Description:
 *		Sends the server messages back to the client (if any).
 *
 *		Types:
 *			Login message 	= 0
 *			Server message	= 1 (in response to a message attn code)
 *
 * Returns: AFPERROR
 */

AFPERROR FPGetServerMessage(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	)
{
	#pragma unused(afpSession)
	
	afp_buffer		afpRequest(afpReqBuffer);
	afp_buffer		afpReply(afpReplyBuffer);
	int16			afpMessageType		= 0;
	int16			afpMessageBitmap	= 0;
	
	//
	//First word contains command and padding
	//
	afpRequest.Advance(sizeof(int16));
	
	//
	//Get the message type and parameters.
	//
	afpMessageType		= afpRequest.GetInt16();
	afpMessageBitmap	= afpRequest.GetInt16();
	
	//
	//We put the first 2 parameters in the block that basically just
	//echo the AFP parameters sent to us.
	//
	afpReply.AddInt16(afpMessageType);
	afpReply.AddInt16(afpMessageBitmap);
	
	switch(afpMessageType)
	{
		//
		//12.29.04: Starting with AFP3.0, bit 1 of the message bitmap indicates that
		//the client wants the message string in unicode format. Thanks to
		//Apple for updating the docs several years later so I can fix the bug.
		//
		case AFP_MSG_LOGINTYPE:
			if (afpMessageBitmap & kSendMessageAsUnicode)
				afpReply.AddUniString(g_afpLoginMessage, false);
			else
				afpReply.AddCStringAsPascal(g_afpLoginMessage);
				
				DPRINT(("[afp:FPGetServerMessage]Sending message (%s)\n", g_afpLoginMessage));
			break;
		
		case AFP_MSG_SERVERTYPE:
			if (afpMessageBitmap & kSendMessageAsUnicode)
				afpReply.AddUniString(g_afpServerMessage, false);
			else
				afpReply.AddCStringAsPascal(g_afpServerMessage);
				
				DPRINT(("[afp:FPGetServerMessage]Sending message (%s)\n", g_afpServerMessage));
			break;
			
		default:
			DPRINT(("[afp:FPGetServerMessage]Bad message type! (%d)\n", afpMessageType));
			return( afpParmErr );
	}
			
	*afpDataSize = afpReply.GetDataLength();
	
	return( AFP_OK );
}


/*
 * afp_Initialize()
 *
 * Description:
 *		Users of the AFP API defined here must call this function before
 *		using the API.
 *
 * Returns: None
 */

void afp_Initialize(void)
{
	memset(g_afpLoginMessage, 0, sizeof(g_afpLoginMessage));
	memset(g_afpServerMessage, 0, sizeof(g_afpServerMessage));
}


/*
 * afp_SetLogonMessage()
 *
 * Description:
 *		Use this routine to set the logon message users will see
 *		when they log onto the afp server.
 *
 * Returns: B_OK if successful, B_ERROR otherwise
 */

status_t afp_SetLogonMessage(const char* logonMsg)
{
	int16	len = strlen(logonMsg);
	
	if (len > AFP_MSG_MAXLEN)
	{
		return( B_ERROR );
	}
	else
	{
		memset(g_afpLoginMessage, 0, sizeof(g_afpLoginMessage));
		strncpy(g_afpLoginMessage, logonMsg, len);
	}
	
	return( B_OK );
}


/*
 * afp_SetServerMessage()
 *
 * Description:
 *		Use this routine to set the server message users will see
 *		when they recieve an attn message from the server.
 *
 * Returns: B_OK if successful, B_ERROR otherwise
 */

status_t afp_SetServerMessage(const char* serverMsg)
{
	int16	len = strlen(serverMsg);
	
	if (len > AFP_MSG_MAXLEN)
	{
		return( B_ERROR );
	}
	else
	{
		memset(g_afpServerMessage, 0, sizeof(g_afpServerMessage));
		strncpy(g_afpServerMessage, serverMsg, len);
	}
	
	return( B_OK );
}

