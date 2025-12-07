#include <stdio.h>
#include <Screen.h>
#include <TextView.h>
#include <net_settings.h>

#include "afpSetHostWin.h"
#include "afpConfigUtils.h"
#include "commands.h"

const float font_size = 14.0f;

/*
 * afpSetHostWin()
 *
 * Description:
 *
 * Returns:
 */

afpSetHostWin::afpSetHostWin(BWindow* parent) :
	BWindow(
		BRect(100, 100, 410, 230),
		"",
		B_MODAL_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE
		)
{
	BView*			mainView;
	BButton*		button;
	BRect			rect;
	BStringView*	bstrview;
	char			hostname[MAX_HOSTNAME_LEN];
	
	rect = parent->Frame();
	MoveTo(
		(rect.left + (rect.Width()/2))  - (BWindow::Bounds().Width()/2),
		(rect.top  + (rect.Height()/2)) - (BWindow::Bounds().Height()/2)
		);
	
	//
	//Setup the background color for dialogs.
	//
	mainView = new BView(
						BRect(0,0,BWindow::Bounds().right, BWindow::Bounds().bottom),
						"MainView",
						B_FOLLOW_ALL_SIDES,
						B_WILL_DRAW | B_FRAME_EVENTS
						);
	
	mainView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(mainView);
	
	//
	//Add the edit text box for entering the name
	//
	rect.Set(10,45,this->Bounds().right-10,70);
	mNameTxt = new BTextControl(rect,"name","Server name:", "", NULL);
	mNameTxt->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mNameTxt->SetFontSize(font_size);
	mNameTxt->SetEnabled(true);
	mNameTxt->SetDivider(100);
	SetTextViewFontSize(mNameTxt->TextView(), font_size);
	mainView->AddChild(mNameTxt);
	
	//
	//Get the current hostname and set the edit text.
	//
	if (AFPGetHostnameFromFile(hostname, sizeof(hostname)) != B_OK)
	{
	}
	else
	{
		mNameTxt->SetText(hostname);
	}
	
	//
	//Build the send and cancel buttons. We adjust per the bottom right
	//corner of the dialog, so resizing won't affect positioning.
	//
	button = new BButton(
					BRect(
						BWindow::Bounds().right-80,
						BWindow::Bounds().bottom-40,
						BWindow::Bounds().right-10,
						BWindow::Bounds().bottom-20),
					"b1", "Save", new BMessage(CMD_MSG_SET)
					);
	button->SetFontSize(font_size);
	mainView->AddChild(button);
	
	button = new BButton(
					BRect(
						BWindow::Bounds().right-160,
						BWindow::Bounds().bottom-40,
						BWindow::Bounds().right-90,
						BWindow::Bounds().bottom-20),
					"b2", "Cancel", new BMessage(CMD_MSG_CANCEL)
					);
    button->SetFontSize(font_size);
	mainView->AddChild(button);
	
	button = new BButton(
					BRect(
						BWindow::Bounds().right-240,
						BWindow::Bounds().bottom-40,
						BWindow::Bounds().right-170,
						BWindow::Bounds().bottom-20),
					"b2", "Default", new BMessage(CMD_MSG_DEFAULT)
					);
	button->SetFontSize(font_size);
	mainView->AddChild(button);
	
	//
	//Help message for what the user should enter in this dialog.
	//
	
	rect.Set(5, 5, this->Bounds().right-5, this->Bounds().top+25);
	bstrview = new BStringView(rect, "", HOST_HELP_MSG);
	bstrview->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	bstrview->SetFontSize(font_size);
	mainView->AddChild(bstrview);

	Show();
}


/*
 * ~afpSetHostWin()
 *
 * Description:
 *
 * Returns:
 */

afpSetHostWin::~afpSetHostWin()
{
}


/*
 * MessageReceived()
 *
 * Description:
 *
 * Returns:
 */

void afpSetHostWin::MessageReceived(BMessage *Message)
{
	int 	error = 0;
	char	msg[256];
	
	switch(Message->what)
	{
		case CMD_MSG_SET:
			if (strlen(mNameTxt->Text()) > 0)
			{
				error = AFPSaveHostnameToFile(mNameTxt->Text());
				
				if (error != B_OK)
				{
					sprintf(msg, "An error occured saving the server name!\n\nError Code: %d", error);
					(new BAlert("", msg, "OK"))->Go();
				}
				else
				{
					Hide();
					
					sprintf(msg, "The server name has been successfully changed to '%s'.", mNameTxt->Text());
					(new BAlert("", msg, "OK"))->Go();
				}
			}
			else
			{
				(new BAlert("", "The server name must be at least one character in length.", "OK"))->Go();
				break;
			}
			
			//
			//Let it fall through so the window closes
			//	
		case CMD_MSG_CANCEL:
			Quit();
			break;
			
		case CMD_MSG_DEFAULT:
			this->FindHostNameFromNetSettings(msg, sizeof(msg));
			mNameTxt->SetText(msg);
			break;
		
		default:
			break;
	}
}


/*
 * FindHostNameFromNetSettings()
 *
 * Description:
 *		Returns the hostname as stored in the Haiku/Be network config.
 *
 * Returns:
 *		Nothing
 */

void afpSetHostWin::FindHostNameFromNetSettings(char* hostname, int32 cbHostname)
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
	}
	else
	{
		//
		//Set some name in case the everything above fails.
		//
		
		strcpy(hostname, "HaikuServer");
	}
}



