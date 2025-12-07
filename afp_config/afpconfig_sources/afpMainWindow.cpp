#include <MenuBar.h>
#include <MenuItem.h>
#include <Box.h>
#include <String.h>
#include <Alert.h>
#include <Message.h>
#include <ListView.h>
#include <StringView.h>
#include <Screen.h>
#include <View.h>
#include <TextView.h>
#include <Be.h>

#include <stdio.h>

#include "afpMainWindow.h"
#include "afpUserConfig.h"
#include "afpConfigUtils.h"
#include "afpMsgWindow.h"
#include "afpAboutWindow.h"
#include "afpSetHostWin.h"
#include "afpLaunch.h"
#include "commands.h"

#define AFP_WINDOW_TITLE		"MacFile Server"

#define WINRECT_MAIN_LEFT		100
#define WINRECT_MAIN_TOP		100
#define WINRECT_MAIN_RIGHT		700
#define WINRECT_MAIN_BOTTOM		555

#define TEXT_SERVER_RUNNING		"Running"
#define TEXT_SERVER_DOWN		"Not started"

const rgb_color kWhite = {255,255,255,255}; 
const rgb_color kGray = {219,219,219,255}; 
const rgb_color kMedGray = {180,180,180,255}; 
const rgb_color kDarkGray = {100,100,100,255}; 
const rgb_color kBlackColor = {0,0,0,255};

const float general_font_size = 12.0f;
const float button_font_size = 14.0f;

/*
 * afpMainWindow()
 *
 * Description:
 *
 * Returns:
 */

afpMainWindow::afpMainWindow(const char *uWindowTitle) :
	BWindow(
		BRect(WINRECT_MAIN_LEFT, WINRECT_MAIN_TOP, WINRECT_MAIN_RIGHT, WINRECT_MAIN_BOTTOM),
		AFP_WINDOW_TITLE,
		B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_ASYNCHRONOUS_CONTROLS
		)
{
	mHighestBPS = 0;
	mLastBPS	= 0;
	
	BuildWindow();
	
	mFilePanel = new BFilePanel(
							B_OPEN_PANEL,
							NULL,
							NULL,
							B_DIRECTORY_NODE,
							false,
							NULL,
							NULL,
							false,
							true
							);
}


/*
 * ~afpMainWindow()
 *
 * Description:
 *
 * Returns:
 */

afpMainWindow::~afpMainWindow()
{
	if (mFilePanel != NULL)
	{
		//
		//Release the memory used by our BFilePanel
		//
		delete mFilePanel;
	}
}


/*
 * SetupMenus()
 *
 * Description:
 *
 * Returns:
 */

void afpMainWindow::SetupMenus()
{
	BMenu*		menu;
	BMenuItem*	item;

	mMenuBar = new BMenuBar(BRect(0,0,0,0), "menubar");
	
	//
	//Setup the file/about menu, just for about and quit.
	//
	menu = new BMenu("MacFile");
	menu->AddItem(
			new BMenuItem("About MacFile...",
			new BMessage(CMD_MENU_ABOUT))
			);
	menu->AddSeparatorItem();
	menu->AddItem(
			new BMenuItem("Close",
			new BMessage(B_QUIT_REQUESTED))
			);
	mMenuBar->AddItem(menu);
	
	//
	//Setup the utility menu
	//
	menu = new BMenu("Utilities");
	item = new BMenuItem("Send Message...", new BMessage(CMD_MENU_SENDMSG));
	item->SetTarget(this);
	menu->AddItem(item);
	item = new BMenuItem("Set Server Name...",  new BMessage(CMD_MENU_SETHOSTNAME));
	item->SetTarget(this);
	menu->AddItem(item);
	menu->AddSeparatorItem();
	item = new BMenuItem("Start MacFile Server", new BMessage(CMD_MENU_STARTSERVER));
	item->SetTarget(this);
	menu->AddItem(item);
	
	mMenuBar->AddItem(menu);
	
	//
	//Add the menu bar to the main window.
	//
	AddChild(mMenuBar);
}


/*
 * BuildWindow()
 *
 * Description:
 *
 * Returns:
 */

void afpMainWindow::BuildWindow(void)
{
	//BStringView*	bstrview;
	BView*			mainView;
	BBox*			bbox;
	BRect			rect;
	float			left, top;
	
	SetupMenus();
						
	rect = BScreen(B_MAIN_SCREEN_ID).Frame();
	left = (rect.Width()/2) - (BWindow::Bounds().Width()/2);
	top  = (rect.Height()/2) - (BWindow::Bounds().Height());
	top  = (top < 5) ? 5 : top;
	
	MoveTo(left, top);

	mainView = new BView(BRect(0,mMenuBar->Bounds().Height(),BWindow::Bounds().right, BWindow::Bounds().bottom), "MainView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS);
	mainView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mainView->SetFontSize(general_font_size);
	AddChild(mainView);

	//===========Volumes
	rect.Set(10,10,305,220);
	bbox = new BBox(rect);
	bbox->SetLabel("Shared Folders");
	bbox->SetFontSize(general_font_size);
	mainView->AddChild(bbox);
	
	rect.Set(10,20,270,125);
	mVolumeListView = new afpListView(
								this,
								CMD_APP_VOLLISTCHANGED,
								rect,
								"vols",
								B_SINGLE_SELECTION_LIST
								);
	mVolumeListView->SetFontSize(general_font_size);
																
	bbox->AddChild(new BScrollView(
					"scroll_vols",
					mVolumeListView,
					B_FOLLOW_LEFT | B_FOLLOW_TOP,
					0,
					false,
					true)
					);
		
	rect.Set(8, 135, 78, 155);
	BButton* new_button = new BButton(rect, "b1", "New...", new BMessage(CMD_APP_ADDVOLUME));
	new_button->SetFontSize(button_font_size);
	bbox->AddChild(new_button);
	
	rect.Set(87, 135, 177, 155);
	mRemoveButton = new BButton(rect, "b2", "Remove", new BMessage(CMD_APP_REMOVEVOL));
	mRemoveButton->SetFontSize(button_font_size);
	bbox->AddChild(mRemoveButton);
	
	rect.Set(8, 180, 105, 180+15);
	mReadOnlyBox = new BCheckBox(rect, "ro", "Read Only", new BMessage(CMD_APP_ROCHECKBOX));
	mReadOnlyBox->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mReadOnlyBox->SetFontSize(general_font_size);
	bbox->AddChild(mReadOnlyBox);
	
	//
	//Populate the volume list so the user sees what's currently shared.
	//
	PopulateVolumeList();
	
	//===========Logon Message
	rect.Set(10,225,305,225+178);
	bbox = new BBox(rect);
	bbox->SetLabel("Logon Message");
	bbox->SetFontSize(general_font_size);
	mainView->AddChild(bbox);
	
	rect.Set(10,20,270,125);
	mLogonMsgView = new BTextView(
							rect,
							"logonmsg",
							BRect(2,2,260,125),
							0,
							B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE
							);
	
	mLogonMsgView->SetFontSize(general_font_size);
	SetTextViewFontSize(mLogonMsgView, general_font_size);

	BScrollView* scroll_view = new BScrollView(
								"scroll_logonmsg",
								mLogonMsgView,
								B_FOLLOW_LEFT | B_FOLLOW_TOP,
								0,
								false,
								true);

	scroll_view->SetFontSize(general_font_size);
	bbox->AddChild(scroll_view);

	mLogonMsgView->MakeEditable(true);
	mLogonMsgView->MakeSelectable(true);
	mLogonMsgView->SetMaxBytes(199);
	
	BStringView* str_view = new BStringView(BRect(10,125,229,145), "", "Up to 199 characters allowed");
	str_view->SetFontSize(general_font_size);
	bbox->AddChild(str_view);
	
	BButton* saveButton = new BButton(BRect(220,132,280,130), "b5", "Save", new BMessage(CMD_APP_SAVELOGONMSG));
	saveButton->SetFontSize(button_font_size);
	bbox->AddChild(saveButton);
	
	GetComment();
	
	//
	//Add the server status string views
	//
	BStringView* server_status_view = new BStringView(BRect(11,406,103,406+25), "", "Server status: ");
	server_status_view->SetFontSize(14.0f);
	mainView->AddChild(server_status_view);
	mServerStatus = new BStringView(BRect(104, 406, 204, 406+25), "", TEXT_SERVER_DOWN);
	mServerStatus->SetFontSize(14.0f);
	mainView->AddChild(mServerStatus);
	
	//===========Users
	rect.Set(315,10,590,220);
	bbox = new BBox(rect);
	bbox->SetFontSize(general_font_size);
	bbox->SetLabel("Users");
	mainView->AddChild(bbox);
	
	rect.Set(10, 20, 250, 105);
	mUserListView = new afpListView(this, CMD_AFP_USERLISTCHANGED, rect, "users", B_SINGLE_SELECTION_LIST);
	mUserListView->SetFontSize(general_font_size);
	bbox->AddChild(new BScrollView(
					"scroll_users",
					mUserListView,
					B_FOLLOW_LEFT | B_FOLLOW_TOP,
					0,
					false,
					true)
					);
	mUserListView->SetInvocationMessage(new BMessage(CMD_APP_MNGUSERS));

	rect.Set(8, 115, 78, 135);
	BButton* add_user = new BButton(rect, "b3", "New...", new BMessage(CMD_APP_ADDUSER));
	add_user->SetFontSize(button_font_size);
	bbox->AddChild(add_user);
	
	rect.Set(97, 115, 177, 135);
	mRemoveUserButton = new BButton(rect, "b4", "Remove", new BMessage(CMD_APP_REMOVEUSER));
	mRemoveUserButton->SetFontSize(button_font_size);
	bbox->AddChild(mRemoveUserButton);
	
	rect.Set(196, 115, 266, 135);
	mEditUserButton = new BButton(rect, "b5", "Edit...", new BMessage(CMD_APP_MNGUSERS));
	mEditUserButton->SetFontSize(button_font_size);
	bbox->AddChild(mEditUserButton);
	
	PopulateUserList();
	
	//===========Logons allowed
	rect.Set(8, 180, 140, 180+15);
	mGuestCB = new BCheckBox(rect, "g", "Allow Guests", new BMessage(CMD_APP_GUEST_CHECKBOX_HIT));
	mGuestCB->SetViewColor(kGray);
	mGuestCB->SetFontSize(general_font_size);
	bbox->AddChild(mGuestCB);
		
	if (AFPGetGuestsAllowed()) {
		mGuestCB->SetValue(1);
	}
	
	//===========Stats
	rect.Set(315,225,590,225+178);
	bbox = new BBox(rect);
	bbox->SetLabel("Statistics");
	bbox->SetFontSize(general_font_size);
	mainView->AddChild(bbox);

	rect.Set(10,15,265,40);
	mThroughputBar = new BStatusBar(rect, "Throughput (BPS)", "Throughput (BPS): ");
	mThroughputBar->SetMaxValue(100);
	mThroughputBar->SetFontSize(general_font_size);
	bbox->AddChild(mThroughputBar);
	
	rect.Set(10,60,265,80);
	mBytesSent = new BTextControl(rect,"sent","Bytes Sent:", "0", NULL);
	mBytesSent->SetEnabled(false);
	mBytesSent->SetFontSize(general_font_size);
	SetTextViewFontSize(mBytesSent->TextView(), general_font_size);
	bbox->AddChild(mBytesSent);

	rect.Set(10,85,265,105);
	mBytesRecv = new BTextControl(rect,"recv","Bytes Recv:", "0", NULL);
	mBytesRecv->SetEnabled(false);
	mBytesRecv->SetFontSize(general_font_size);
	SetTextViewFontSize(mBytesRecv->TextView(), general_font_size);
	bbox->AddChild(mBytesRecv);

	rect.Set(10,110,265,130);
	mDSIPackets = new BTextControl(rect,"dsi","AFP Packets:", "0", NULL);
	mDSIPackets->SetEnabled(false);
	mDSIPackets->SetFontSize(general_font_size);
	SetTextViewFontSize(mDSIPackets->TextView(), general_font_size);
	bbox->AddChild(mDSIPackets);

	rect.Set(10,135,265,155);
	mLoggedIn = new BTextControl(rect,"logged","Logged In:", "0", NULL);
	mLoggedIn->SetEnabled(false);
	mLoggedIn->SetFontSize(general_font_size);
	SetTextViewFontSize(mLoggedIn->TextView(), general_font_size);
	bbox->AddChild(mLoggedIn);
	
	SetControlsState();
}


/*
 * MessageReceived()
 *
 * Description:
 *
 * Returns:
 */

void afpMainWindow::MessageReceived(BMessage * Message)
{
	switch(Message->what)
	{
		case CMD_APP_ADDUSER:
		{
			afpUserConfig*	userConfig = new afpUserConfig(this, true);
			
			if (userConfig != NULL)
			{
				userConfig->Show();
			}
			break;
		}
		
		case CMD_APP_REMOVEUSER:
		{
			char			textmsg[128];
			int16			result	= 0;
			BAlert*			alert	= NULL;
			BStringItem*	item	= NULL;
			int16			index	= 0;
			
			index = mUserListView->CurrentSelection();
				
			if (index < 0) {
			
				beep();
				return;
			}
			
			item = (BStringItem*)mUserListView->ItemAt(index);
			
			if (item == NULL) {
			
				beep();
				return;
			}
			
			sprintf(textmsg, "Are you sure you want to delete the user '%s'?\n\n", item->Text());

			//
			//Initialize our alert object and setup it's parameters.
			//
			alert = new BAlert(
							"",
							textmsg,
							"Cancel",
							"Yes"
							);
			
			alert->SetShortcut(0, B_ESCAPE);
			result = alert->Go();
			
			if (result == 1)
			{
				if (AFPDeleteUser(item->Text()) != B_OK)
				{
					(new BAlert("", "Removing user failed!", "OK"))->Go();
					break;
				}
				
				PopulateUserList();
			}
			
			break;
		}
		
		case CMD_APP_MNGUSERS:
		{
			BStringItem*	item	= NULL;
			int16			index	= 0;
			
			index = mUserListView->CurrentSelection();
				
			if (index < 0) {
			
				beep();
				return;
			}
			
			item = (BStringItem*)mUserListView->ItemAt(index);
			
			if (item == NULL) {
			
				beep();
				return;
			}
			
			afpUserConfig*	userConfig = new afpUserConfig(this, false, item->Text());
			
			if (userConfig != NULL)
			{
				userConfig->Show();
			}
			
			break;
		}
		
		case CMD_APP_SENDMSG:
			break;
		
		case CMD_APP_SAVELOGONMSG:
		{
			SaveComment();
			(new BAlert("", "Logon message has been saved.", "OK"))->Go();
			break;
		}
		
		case CMD_APP_GUEST_CHECKBOX_HIT:
		{
			status_t	status;
			
			status = AFPSetGuestsAllowed((mGuestCB->Value() > 0));
			
			if (status != B_OK)
			{
				char text[256];
				
				sprintf(text, "Saving guest failed (%s).", AFPGetErrorString(status));
				(new BAlert("", text, "OK"))->Go();
			}
			break;
		}
		
		case CMD_APP_ADDVOLUME:
			if (mFilePanel != NULL)
			{
				//
				//The user wants to add a volume to the shared list. Show
				//them the file add UI that will allow them to add a new
				//folder to the volumes list.
				//
				mFilePanel->Show();
			}
			break;
		
		case CMD_APP_REMOVEVOL:
			RemoveShare();
			break;
		
		case CMD_APP_VOLLISTCHANGED:
			SetControlsState();
			break;
			
		case CMD_APP_ROCHECKBOX:
			SaveReadOnlyState();
			break;
			
		case CMD_AFP_USERLISTCHANGED:
			SetControlsState();
			break;
			
		case CMD_MENU_SENDMSG:
			new afpMsgWindow(this);
			break;
			
		case CMD_MENU_SETHOSTNAME:
			new afpSetHostWin(this);
			break;
			
		case CMD_MENU_STARTSERVER:
			AFPLaunchServer();
			break;
			
		case CMD_MENU_ABOUT:
			new afpAboutWindow(this);
			break;
		
		default:
		  BWindow::MessageReceived(Message);
		  break;
	}
}


/*
 * PopulateUserList()
 *
 * Description:
 *	Populate the user list by asking the server for the users.
 *
 * Returns:
 */

void afpMainWindow::PopulateUserList()
{
	BString		userName;
	BString		password;
	uint32		flags	= 0;
	uint16 		i 		= 0;
	
	mUserListView->LockLooper();
	mUserListView->MakeEmpty();
	
	while (AFPGetIndUser(i++, &userName, &password, &flags) == B_OK)
	{
		if ((flags & kDontDisplay) == 0) {
			mUserListView->AddItem(new BStringItem(userName.String()));
		}
	}
	
	mUserListView->UnlockLooper();
}


/*
 * PopulateVolumeList()
 *
 * Description:
 *	Populate the volume list by asking the server for the shared volumes.
 *
 * Returns:
 */

void afpMainWindow::PopulateVolumeList()
{
	BString		path;
	int16 		i 		= 0;
	
	mVolumeListView->LockLooper();
	mVolumeListView->MakeEmpty();
	
	while(AFPGetVolumePathFromIndex(i++, &path) == B_OK) {
		mVolumeListView->AddItem(new BStringItem(path.String()));
	}
	
	mVolumeListView->UnlockLooper();
}


/*
 * SaveReadOnlyState()
 *
 * Description:
 *	Save the read only state for the selected volume to the pref file.
 *
 * Returns:
 */

void afpMainWindow::SaveReadOnlyState()
{
	uint32		volFlags	= 0;
	int32		index		= 0;
	
	index 	= mVolumeListView->CurrentSelection();
	
	if (index < 0) {
	
		return;
	}
		
	if (AFPGetVolumeFlagsFromIndex(index, &volFlags) == B_OK)
	{
		if (mReadOnlyBox->Value() == 0)
		{
			volFlags &= ~0x02;
		}
		else
		{
			volFlags |= 0x02;
		}
		
		if (AFPSaveVolumeFlags(index, volFlags) != B_OK)
		{
			(new BAlert("", "Error saving volume state!", "OK"))->Go();
		}
	}
}


/*
 * SetControlsState()
 *
 * Description:
 *	Set the proper read only state by checking (or unchecking) the
 *	checkbox when a new volume is selected in the list box.
 *
 * Returns:
 */

void afpMainWindow::SetControlsState()
{
	uint32		volFlags	= 0;
	int32		index		= 0;
	
	//
	//Setup the shared folders controls for proper display
	//
	index 	= mVolumeListView->CurrentSelection();
	
	if (index < 0) 
	{
		mReadOnlyBox->SetValue(0);
		mReadOnlyBox->SetEnabled(false);
		mRemoveButton->SetEnabled(false);
	}
	else
	{
		mReadOnlyBox->SetEnabled(true);
		mRemoveButton->SetEnabled(true);
		
		if (AFPGetVolumeFlagsFromIndex(index, &volFlags) == B_OK)
		{
			if (volFlags & 0x02)
			{
				mReadOnlyBox->SetValue(1);
			}
			else
			{
				mReadOnlyBox->SetValue(0);
			}
		}
	}
	
	//
	//Now the user controls
	//
	index = mUserListView->CurrentSelection();
	
	if (index < 0)
	{
		mEditUserButton->SetEnabled(false);
		mRemoveUserButton->SetEnabled(false);
	}
	else
	{
		mEditUserButton->SetEnabled(true);
		mRemoveUserButton->SetEnabled(true);
	}
}


/*
 * RemoveShare()
 *
 * Description:
 *	Stop sharing a directory from the list.
 *
 * Returns:
 */

void afpMainWindow::RemoveShare()
{
	BString		path;
	BAlert*		alert	= NULL;
	int			result	= 0;
	int32		index	= 0;
	char		textmsg[256];

	index 	= mVolumeListView->CurrentSelection();
	
	if (index < 0) {
	
		return;
	}
	
	if (AFPGetVolumePathFromIndex(index, &path) == B_OK)
	{
		sprintf(textmsg, "Are you sure you want to stop sharing this directory?\n\n %s", path.String());

		//
		//Initialize our alert object and setup it's parameters.
		//
		alert = new BAlert(
						"",
						textmsg,
						"Cancel",
						"Yes"
						);
		
		alert->SetShortcut(0, B_ESCAPE);
		result = alert->Go();
		
		if (result == 1)
		{
			//
			//Call the afp api that will do the dirty message work for us.
			//
			result = AFPRemoveShare(&path);
			
			switch(result)
			{
				case B_OK:
					mVolumeListView->LockLooper();
					mVolumeListView->RemoveItem(index);
					mVolumeListView->UnlockLooper();
					break;
				
				case be_afp_invalidvolume:
					sprintf(textmsg, "ERROR! The directory:\n\n %s\n\n is currently not shared by AFP!", path.String());
					(new BAlert("", textmsg, "OK"))->Go();
					break;
					
				default:
					sprintf(textmsg, "ERROR: Attempt to stop sharing %s failed!", path.String());
					(new BAlert("", textmsg, "OK"))->Go();
					break;
			}
		}
	}
}


/*
 * AddNewShare()
 *
 * Description:
 *
 * Returns:
 */

void afpMainWindow::AddNewShare(entry_ref newRef)
{
	BPath		path;
	char		textmsg[256];
	int			result;
	
	result = AFPAddShare(newRef, &path);
	
	switch(result)
	{
		case B_OK:
			//
			//Update the volume list with the added share. We have to lock the
			//window before adding to the list for the view.
			//
			(mVolumeListView->Window())->Lock();
			mVolumeListView->AddItem(new BStringItem(path.Path()));
			(mVolumeListView->Window())->Unlock();
			break;
		
		case be_afp_sharealreadyexits:
			sprintf(textmsg, "The directory:\n\n%s\n\nis already shared out by AFP", newRef.name);
			(new BAlert("", textmsg, "OK"))->Go();
			break;
			
		default:
			(new BAlert("", "Error, directory not shared!", "OK"))->Go();
			break;
	}
}


/*
 * SaveComment()
 *
 * Description:
 *
 * Returns:
 */

void afpMainWindow::SaveComment()
{
	if (AFPSaveLogonMessage(mLogonMsgView->Text(), mLogonMsgView->TextLength()) != B_OK)
	{
		(new BAlert("", "An error occured saving the logon message!", "OK"))->Go();
	}	
}


/*
 * GetComment()
 *
 * Description:
 *
 * Returns:
 */

void afpMainWindow::GetComment()
{
	char		buffer[256];
	
	if (AFPGetLogonMessage(buffer, sizeof(buffer)) == B_OK)
	{
		mLogonMsgView->Insert(buffer);
	}
}


/*
 * UpdateThroughput()
 *
 * Description:
 *
 * Returns:
 */

void afpMainWindow::UpdateThroughput()
{
	int			status 	= 0;
	int64		bytes64	= 0;
	int32		bytes32	= 0;
	char		text[64];
	
	if (AFPGetBytesPerSecond(&bytes32) == B_OK)
	{
		if (bytes32 < 0) {
			bytes32 = 0;
		}
			
		mThroughputBar->LockLooper();
		
		if (bytes32 > mHighestBPS)
		{
			mHighestBPS = bytes32;
			mThroughputBar->SetMaxValue(mHighestBPS);
		}
		
		if (mLastBPS != bytes32)
		{
			sprintf(text, "%d", (int)bytes32);
			mThroughputBar->Update(bytes32 - mLastBPS, text);
				
			mLastBPS = bytes32;
		}
		
		mThroughputBar->UnlockLooper();
		
		status++;
	}
	
	if (AFPGetBytesSent(&bytes64) == B_OK)
	{
		sprintf(text, "%u", (int)LO_LONG(bytes64));
		
		mBytesSent->LockLooper();
		
		if (strcmp(text, mBytesSent->Text())) {
			mBytesSent->SetText(text);
		}
		
		mBytesSent->UnlockLooper();
		
		status++;
	}

	if (AFPGetBytesRecieved(&bytes64) == B_OK)
	{
		sprintf(text, "%u", (int)LO_LONG(bytes64));
		
		mBytesRecv->LockLooper();
		
		if (strcmp(text, mBytesSent->Text())) {
			mBytesRecv->SetText(text);
		}
		
		mBytesRecv->UnlockLooper();
		
		status++;
	}
	
	if (AFPGetPacketsProcessed(&bytes32) == B_OK)
	{
		sprintf(text, "%d", (int)bytes32);
		
		mDSIPackets->LockLooper();
		
		if (strcmp(text, mDSIPackets->Text())) {
			mDSIPackets->SetText(text);
		}
		
		mDSIPackets->UnlockLooper();
		
		status++;
	}
	
	if (AFPGetUsersLoggedOn(&bytes32) == B_OK)
	{
		sprintf(text, "%d", (int)bytes32);
		
		mLoggedIn->LockLooper();
		
		if (strcmp(text, mLoggedIn->Text())) {
			mLoggedIn->SetText(text);
		}
		
		mLoggedIn->UnlockLooper();
		
		status++;
	}
	
	//
	//Use this opportunity to update the server status (up or down)
	//
	if (mServerStatus != NULL)
	{
		mServerStatus->LockLooper();
		
		if (status >= 1)
		{
			//
			//If status >= 1, then we're able to talk to the server.
			//
			if (strcmp(TEXT_SERVER_RUNNING, mServerStatus->Text())) {
				mServerStatus->SetText(TEXT_SERVER_RUNNING);
			}
		}
		else
		{
			if (strcmp(TEXT_SERVER_DOWN, mServerStatus->Text())) {
				mServerStatus->SetText(TEXT_SERVER_DOWN);
			}
		}
		
		mServerStatus->UnlockLooper();
	}
}


/*
 * QuitRequested()
 *
 * Description:
 *
 * Returns:
 */

bool afpMainWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);	
	return BWindow::QuitRequested();
}
