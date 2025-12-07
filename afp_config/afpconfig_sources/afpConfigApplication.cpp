#include <List.h>
#include <StorageKit.h>
#include <Message.h>
#include <FilePanel.h>

#include "afpConfigApplication.h"
#include "afpUserConfig.h"
#include "afpConfigUtils.h"
#include "afpLaunch.h"

extern int afpAppReturnValue;

/*
 * afpConfigApplication()
 *
 * Description:
 *
 * Returns:
 */

afpConfigApplication::afpConfigApplication()
:
	BApplication(ApplicationSignature),
	iMainWindow(NULL)
{
	int result;
	
	SetPulseRate(1000000);
	
	//
	//Check to see if the server is running. If not, ask the user if
	//they wish to start it.
	//
	if (!AFPServerIsRunning())
	{
		//
		//The server is not running, ask the user if they wish to
		//start it.
		//
		
		char textmsg[512];

		sprintf(textmsg, "The afp_server is not running, would you like to start it? Many functions will not work unless the server is running.");
		
		BAlert* alert = new BAlert(
						"",
						textmsg,
						"No",
						"Yes"
						);
		
		alert->SetShortcut(0, B_ESCAPE);
		result = alert->Go();
		
		if (result == 1)
		{
			//
			//Call the utility function that will launch the server.
			//
			
			result = AFPLaunchServer();
			
			snooze(100000);
			
			if ((result != B_OK) || (!AFPServerIsRunning()))
			{
				//
				//If we fail to launch the server then it must not be installed
				//properly. Bail out since there's nothing we can ever do.
				//
				
				BAlert* alert2 = new BAlert(
								"",
								"Failed to start afp_server, ensure it is installed properly.",
								"OK"
								);
				
				alert2->SetShortcut(0, B_ESCAPE);
				result = alert2->Go();
				
				be_app->PostMessage(B_QUIT_REQUESTED);
				
				return;
			}
		}
		else
		{
			be_app->PostMessage(B_QUIT_REQUESTED);
			return;
		}
	}
	
  	iMainWindow = new afpMainWindow("AFP Configuration");
  
  	if (iMainWindow != NULL)
	{	
		iMainWindow->Show();
	}
	else
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
	}
}


/*
 * ~afpConfigApplication()
 *
 * Description:
 *
 * Returns:
 */

afpConfigApplication::~afpConfigApplication()
{
	if (iMainWindow != NULL)
	{
		//
		//Must Lock() before calling Quit()
		//
		if (iMainWindow->LockWithTimeout(30000000) == B_OK)
		{
			//
			//Never delete a BWindow, call Quit() instead.
			//
			iMainWindow->Quit();
		}
	}
}

/*
 * ReadyToRun()
 *
 * Description:
 *
 * Returns:
 */

void afpConfigApplication::ReadyToRun()
{
}

/*
 * Pulse()
 *
 * Description:
 *
 * Returns:
 */

void afpConfigApplication::Pulse()
{
	iMainWindow->UpdateThroughput();
}


/*
 * MessageReceived()
 *
 * Description:
 *
 * Returns:
 */

void afpConfigApplication::MessageReceived(BMessage * Message)
{
	switch(Message->what)
	{
		case CMD_APP_REFRESHUSERLIST:
			iMainWindow->PopulateUserList();
			break;
			
		default:
			BApplication::MessageReceived(Message);
			break;
	}
}

void afpConfigApplication::RefsReceived(BMessage* message)
{
	entry_ref	dirRef;
	int			count;
	
	if (message->what == B_REFS_RECEIVED)
	{
		//
		//Cycle through each directory we've been handed and tell the AFP
		//server to serve it up as a volume.
		//
		for (	count = 0;
				message->FindRef("refs", count, &dirRef) == B_NO_ERROR;
				count++ )
		{
				iMainWindow->AddNewShare(dirRef);
		}
	}
}





