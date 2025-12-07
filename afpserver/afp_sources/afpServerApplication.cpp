#include <String.h>
#include <List.h>
#include <StorageKit.h>
#include <Alert.h>
#include <Beep.h>

#include "debug.h"
#include "commands.h"
#include "afpServerApplication.h"
#include "dsi_network.h"
#include "fp_volume.h"
#include "dsi_scavenger.h"
#include "dsi_stats.h"
#include "afpmsg.h"
#include "afplogon.h"
#include "afpvolume.h"
#include "afphostname.h"

extern dsi_scavenger* gAFPSessionMgr;
extern std::unique_ptr<BList> volume_blist;

dsi_stats gAFPStats;

/*
 * afpServerApplication()
 *
 * Description:
 *
 * Returns:
 */

afpServerApplication::afpServerApplication()
:
	BApplication(ApplicationSignature)
{
	afpInitializeServerNetworking();
}


/*
 * ~afpServerApplication()
 *
 * Description:
 *
 * Returns:
 */

afpServerApplication::~afpServerApplication()
{
}


/*
 * MessageReceived()
 *
 * Description:
 *
 * Returns:
 */

void afpServerApplication::MessageReceived(BMessage* message)
{
	BString				string;
	BPath				path;
	uint16				volIndex;
	AFPERROR			afpError;
	VolumeStorageData	volData;
	uint32				volFlags;
	int					result = B_OK;
	
	switch(message->what)
	{
		case CMD_AFP_SERVER_VERSION:
		{
			BMessage reply(be_afp_success);
			
			reply.AddInt32(AFP_PARAM_INT32, AFP_SERVER_VERSION);
			message->SendReply(&reply);
			break;
		}
		
		case CMD_AFP_COMMAND_VERSION:
		{
			BMessage reply(be_afp_success);
			
			reply.AddInt32(AFP_PARAM_INT32, AFP_CMD_VERSION);
			message->SendReply(&reply);
			break;
		}
		
		case CMD_AFP_GETHOSTNAME:
			afp_HandleGethostnameAppMsg(message);
			break;
			
		case CMD_AFP_SETHOSTNAME:
			afp_HandleSethostnameAppMsg(message);
			break;
		
		case CMD_AFP_GETRECVBYTES:
		{
			BMessage reply(be_afp_success);
			
			reply.AddInt64(AFP_PARAM_INT64, gAFPStats.Net_BytesRecv());
			message->SendReply(&reply);
			break;
		}
		
		case CMD_AFP_GETSENTBYTES:
		{
			BMessage reply(be_afp_success);
			
			reply.AddInt64(AFP_PARAM_INT64, gAFPStats.Net_BytesSent());
			message->SendReply(&reply);
			break;
		}
		
		case CMD_AFP_GETBYTESPERSECOND:
		{
			BMessage reply(be_afp_success);
			
			reply.AddInt32(AFP_PARAM_INT32, gAFPStats.Net_BytesPerSecond());
			message->SendReply(&reply);
			break;
		}
		
		case CMD_AFP_GETDSIPACKETS:
		{
			BMessage reply(be_afp_success);
						
			reply.AddInt32(AFP_PARAM_INT32, gAFPStats.DSI_NumPacketsProcessed());
			message->SendReply(&reply);
			break;
		}
		
		case CMD_AFP_GETUSERSLOGGEDIN:
		{
			BMessage reply(be_afp_success);
			
			reply.AddInt32(AFP_PARAM_INT32, gAFPSessionMgr->NumOpenSessions());
			message->SendReply(&reply);
			break;
		}			
		
		case CMD_AFP_GETUSERINFO:
			if (message->FindString(AFP_PARAM_STRING_USERNAME, &string) == B_OK)
			{
				AFP_USER_DATA	userData;
				
				afpError = afpGetUserDataByName(string.String(), &userData);
				
				if (AFP_SUCCESS(afpError))
				{
					BMessage reply(be_afp_success);
					
					reply.AddString(AFP_PARAM_STRING_USERNAME, userData.username);
					reply.AddString(AFP_PARAM_STRING_PASSWORD, userData.password);
					reply.AddInt32(AFP_PARAM_INT32, userData.flags);
					
					DPRINT(("[CMD_AFP_GETUSERINFO]User flags (%s) = 0x%x\n", userData.username, (int)userData.flags));
					
					message->SendReply(&reply);
				}
				else
				{
					message->SendReply(be_afp_failure);
				}
			}
			break;
			
		case CMD_AFP_UPDATEUSERINFO:
			if (message->FindString(AFP_PARAM_STRING_USERNAME, &string) == B_OK)
			{
				AFP_USER_DATA	userData;
				BString			password;
				uint32			flags = 0;
				
				afpError = afpGetUserDataByName(string.String(), &userData);
				
				DPRINT(("[CMD_AFP_UPDATEUSERINFO]afpGetUserDataByName() returned: %lu\n", afpError));
				
				if (AFP_SUCCESS(afpError))
				{
					if (message->FindString(AFP_PARAM_STRING_PASSWORD, &password) != B_OK)
					{
						message->SendReply(be_afp_failure);
						break;
					}
					
					if (message->FindInt32(AFP_PARAM_INT32, (int32*)&flags) != B_OK)
					{
						message->SendReply(be_afp_failure);
						break;
					}
										
					memset(userData.password, 0, sizeof(userData.password));
					strcpy(userData.password, password.String());
					
					userData.flags	 	= flags;
					afpError 			= afpUpdateUserInfo(userData);
					
					DPRINT(("[CMD_AFP_UPDATEUSERINFO]afpUpdateUserInfo() returned: %lu\n", afpError));
				}
				
				if (AFP_SUCCESS(afpError))
				{
					DPRINT(("[CMD_AFP_UPDATEUSERINFO]User flags (%s) = 0x%x\n", string.String(), (int)flags));
					message->SendReply(be_afp_success);
				}
				else
				{
					DPRINT(("[CMD_AFP_UPDATEUSERINFO]afpGetUserDataByName() failed (%lu)\n", afpError));
					message->SendReply(be_afp_failure);
				}
			}
			else {
				DPRINT(("[CMD_AFP_UPDATEUSERINFO]No user name supplied!\n"));
				message->SendReply(be_afp_failure);
			}
			break;
			
		case CMD_AFP_ADDUSER:
			if (message->FindString(AFP_PARAM_STRING_USERNAME, &string) == B_OK)
			{
				BString	password;
				uint32	flags;
				
				if (message->FindString(AFP_PARAM_STRING_PASSWORD, &password) == B_OK)
				{
					if (message->FindInt32(AFP_PARAM_INT32, (int32*)&flags) == B_OK)
					{
						afpError = afpSaveNewUser(string.String(), password.String(), flags);
						if (!AFP_SUCCESS(afpError))
						{
							message->SendReply(be_afp_failure);
						}
						else
						{
							message->SendReply(be_afp_success);
						}
					}
				}
			}
			else
			{
				message->SendReply(be_afp_failure);
			}
			break;
		
		case CMD_AFP_DELUSER:
			result = be_afp_failure;
			
			if (message->FindString(AFP_PARAM_STRING_USERNAME, &string) == B_OK)
			{
				if (afpDeleteUser(string.String()) == AFP_OK)
				{
					result = be_afp_success;
				}
			}
			
			message->SendReply(result);
			break;
		
		case CMD_AFP_GETINDUSER:
		{
			AFP_USER_DATA	userData;
			int16			index;
			
			if (message->FindInt16(AFP_PARAM_INT16, &index) == B_OK)
			{
				if (afpGetIndUser(&userData, index) == AFP_OK)
				{
					BMessage reply(be_afp_success);
					
					reply.AddString(
							AFP_PARAM_STRING_USERNAME,
							userData.username
							);
					
					reply.AddString(
							AFP_PARAM_STRING_PASSWORD,
							userData.password
							);

					reply.AddInt32(
							AFP_PARAM_INT32,
							userData.flags
							);

					message->SendReply(&reply);
				}
				else
				{
					message->SendReply(be_afp_failure);
				}
			}
			else
			{
				message->SendReply(be_afp_failure);
			}
			
			break;
		}
		
		case CMD_AFP_SETVOLFLAGS:
			if (message->FindString(AFP_PARAM_PATHSTRING, &string) == B_OK)
			{
				if (GetVolumeData(string.String(), &volData) == B_OK)
				{
					if (message->FindInt32(AFP_PARAM_INT32, (int32*)&volFlags) == B_OK)
					{
						volData.flags = volFlags;
						SaveVolumeData(&volData);
						
						message->SendReply(be_afp_success);
						
						DPRINT(("[CMD_AFP_GETFLAGS]Set volume flags succeeded (0x%lx)\n", volFlags));
						break;
					}
				}
				
				DPRINT(("[CMD_AFP_GETFLAGS]Failed to set volume data!\n"));
			}
			
			message->SendReply(be_afp_failure);
			break;
		
		case CMD_AFP_GETVOLFLAGS:
			if (message->FindString(AFP_PARAM_PATHSTRING, &string) == B_OK)
			{
				if (GetVolumeData(string.String(), &volData) == B_OK)
				{
					BMessage reply(be_afp_success);
					
					reply.AddInt32(AFP_PARAM_INT32, volData.flags);
					message->SendReply(&reply);
					
					DPRINT(("[CMD_AFP_GETFLAGS]Get volume flags succeeded (0x%lx)\n", volData.flags));
					break;
				}
				
				DPRINT(("[CMD_AFP_GETFLAGS]Failed to get volume data!\n"));
			}
			
			message->SendReply(be_afp_failure);
			break;
			
		case CMD_AFP_ADDSHARE:
			if (message->FindString(AFP_PARAM_PATHSTRING, &string) == B_OK)
			{
				//
				//Sending custom volume flags is not required since we've done
				//a version without them.
				//
				if (message->FindInt32(AFP_PARAM_INT32, (int32*)&volFlags) == B_OK) {
				
					volData.flags	= volFlags;
				}
				else {
				
					volData.flags	= 0;
				}
				
				strcpy(volData.path, string.String());
				
				SaveVolumeData(&volData);
				
				if (StartSharingVolume(&volData) == B_OK) {
				
					message->SendReply(be_afp_success);
				}
				else {
					message->SendReply(be_afp_sharealreadyexits);
				}
			}
			else
			{
				message->SendReply(be_afp_failure);
			}
			break;
			
		case CMD_AFP_RMVSHARE:
			if (message->FindString(AFP_PARAM_PATHSTRING, &string) == B_OK)
			{
				//
				//Tell the server to not share this volume anymore.
				//
				if (StopSharingVolume(string.String()) == B_OK)
				{
					//
					//Remove the volume from the volume preferences file.
					//
					RemoveVolumeData(string.String());
					
					message->SendReply(be_afp_success);
				}
				else {	
					message->SendReply(be_afp_invalidvolume);
				}
			}
			else
			{
				message->SendReply(be_afp_failure);
			}
			break;
			
		case CMD_AFP_SENDMSG:
			if (message->FindString(AFP_PARAM_STRING, &string) == B_OK)
			{
				if (afp_SetServerMessage(string.String()) == B_OK)
				{
					DPRINT(("send global attention (0x%x)\n", ATTN_SERVER_MESSAGE));
					gAFPSessionMgr->SendGlobalAttention(ATTN_SERVER_MESSAGE);
					message->SendReply(be_afp_success);
				}
				else {	
					message->SendReply(be_afp_failure);
				}
			}
			else
			{
				message->SendReply(be_afp_paramerr);
			}
			break;
			
		case CMD_AFP_UPDATELOGINMSG:
			if (message->FindString(AFP_PARAM_STRING, &string) == B_OK)
			{
				if (afp_SetLogonMessage(string.String()) == B_OK) {
					message->SendReply(be_afp_success);
				}
				else {
					message->SendReply(be_afp_failure);
				}
			}
			else
			{
				message->SendReply(be_afp_paramerr);
			}
			break;
			
		case CMD_AFP_GETVOLUMENAME:							
			if (message->FindInt16(AFP_PARAM_INT16, (int16*)&volIndex) == B_OK)
			{
				fp_volume* afpVolume = NULL;
								
				afpVolume = (fp_volume*)volume_blist->ItemAt(volIndex);
								
				if (afpVolume != NULL)
				{
					BMessage reply(be_afp_success);
					
					reply.AddString(AFP_PARAM_STRING, afpVolume->GetVolumeName());
					message->SendReply(&reply);
				}
				else {
					message->SendReply(be_afp_failure);
				}
			}
			break;
			
		case CMD_AFP_GETVOLUMEPATH:
			if (message->FindInt16(AFP_PARAM_INT16, (int16*)&volIndex) == B_OK)
			{
				fp_volume* afpVolume = NULL;
								
				afpVolume = (fp_volume*)volume_blist->ItemAt(volIndex);
								
				if (afpVolume != NULL)
				{
					BMessage reply(be_afp_success);
					
					reply.AddString(
							AFP_PARAM_PATHSTRING,
							(afpVolume->GetPath())->Path()
							);
					
					message->SendReply(&reply);
				}
				else {
					message->SendReply(be_afp_failure);
				}
			}
			break;

		//
		//We watch the nodes associated with our shared AFP volumes. If
		//one of the share points for our volumes is moved, renamed or deleted,
		//then we automatically stop sharing that directory.
		//
		case B_NODE_MONITOR:
		{
			fp_volume*	afpVolume	= NULL;
			int32		opcode		= 0;
			node_ref	nref;
			
			message->FindInt32("opcode", &opcode);
			
			switch(opcode)
			{
				case B_ENTRY_MOVED:
					DPRINT(("DIR MOVED!!!!!\n"));
					
					message->FindInt64("node", &nref.node);
					message->FindInt32("device", &nref.device);
					break;
					
				case B_ENTRY_REMOVED:
					DPRINT(("DIR REMOVED!!!!!\n"));
					
					message->FindInt64("node", &nref.node);
					message->FindInt32("device", &nref.device);
					break;
				
				default:
					break;
			}
			
			//
			//Look for the volume associated to this node.
			//
			afpVolume = FindVolume(nref);
			
			if (afpVolume != NULL)
			{	
				BPath*	vPath = afpVolume->GetPath();
				char	textmsg[512];
				
				sprintf(
					textmsg,
					"You have moved, renamed or deleted the AFP share point:\n\n%s\n\nThe volume is no longer shared by afp_server",
					vPath->Path()
					);
				
				(new BAlert("", textmsg, "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();

				StopSharingVolume(vPath->Path());
				RemoveVolumeData(vPath->Path());
			}
			break;
		}
		
		case B_QUIT_REQUESTED:
			DBGWRITE(dbg_level_info, "Quit requested\n");
			gAFPSessionMgr->SendGlobalAttention(ATTN_SERVER_SHUTDOWN);
			break;
			
		default:
			BApplication::MessageReceived(message);
			break;
	}
}


/*
 * ReadyToRun()
 *
 * Description:
 *
 * Returns:
 */

void afpServerApplication::ReadyToRun()
{
	BPath			path;
	BFile			file;
	char			fpath[256];
	char			buffer[256];
	AFP_USER_DATA	userData;
	
	//
	//Read in the logon message from the message preference file
	//
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{	
		sprintf(fpath, "%s/%s", path.Path(), AFP_LOGONMSG_FILE_NAME);
		file.SetTo(fpath, B_READ_WRITE);
				
		if (file.InitCheck() == B_OK)
		{
			memset(buffer, 0, sizeof(buffer));
			
			file.Read(buffer, sizeof(buffer));
			
			if (strlen(buffer) > 0)
			{
				afp_SetLogonMessage(buffer);
			}
		}
	}
	
	afpVerifyUserDatabase();
		
	//
	//Check for and create if necessary the Guest user account
	//
	if (afpGetUserDataByName(AFP_GUEST_NAME, &userData) == B_OK)
	{
		//
		//Nothing to do here since the account already exists.
		//
	}
	else
	{		
		afpSaveNewUser(AFP_GUEST_NAME, "", kDontDisplay);
	}

	//
	//Create the volume list and share all volumes as configured.
	//
	ShareAllVolumes();
}


/*
 * Pulse()
 *
 * Description:
 *
 * Returns:
 */

void afpServerApplication::Pulse()
{
	//Provides a "heartbeat" for your application; a good place to blink cursors, etc.
	//You set the pulse rate in BApplication::SetPulseRate().
}
