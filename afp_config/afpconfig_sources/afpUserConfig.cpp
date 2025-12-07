#include <StringView.h>
#include <Alert.h>
#include <stdio.h>
#include <String.h>

#include "afpUserConfig.h"
#include "afpConfigUtils.h"

#define CMD_USRCONFIG_ADMIN		'usra'
#define CMD_USRCONFIG_SAVE		'usrs'
#define CMD_USRCONFIG_CANCEL	'usrc'

#define WINRECT_USERCONFIG_TOP		150
#define WINRECT_USERCONFIG_LEFT		125
#define WINRECT_USERCONFIG_RIGHT	450
#define WINRECT_USERCONFIG_BOTTOM	355

const float font_size = 12.0f;

/*
 * afpUserConfig()
 *
 * Description:
 *
 * Returns:
 */

afpUserConfig::afpUserConfig(BWindow* parent, bool inNewUser, const char* userName) :
	BWindow(
		BRect(WINRECT_USERCONFIG_TOP, WINRECT_USERCONFIG_LEFT, WINRECT_USERCONFIG_RIGHT, WINRECT_USERCONFIG_BOTTOM),
		"User Properties",
		B_MODAL_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE
		)
{
	BRect rect;
	
	//
	//Center window over the parent window.
	//
	rect = parent->Frame();
	MoveTo(
		(rect.left + (rect.Width()/2))  - (BWindow::Bounds().Width()/2),
		(rect.top  + (rect.Height()/2)) - (BWindow::Bounds().Height()/2)
		);

	if (userName != NULL) {
		strcpy(mUserName, userName);
	}
	
	mIsNewUser = inNewUser;
	
	BuildWindow();
}


/*
 * ~afpUserConfig()
 *
 * Description:
 *
 * Returns:
 */

afpUserConfig::~afpUserConfig()
{
}


/*
 * BuildWindow()
 *
 * Description:
 *
 * Returns:
 */
 
void afpUserConfig::BuildWindow(void)
{
	BView*			mainView;
	BRect			rect;
		
	mainView = new BView(BWindow::Bounds(), "MainView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS);
	mainView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(mainView);
	
	rect.Set(10,10,BWindow::Bounds().right-10,25);
	BStringView* bstrview = new BStringView(rect, "winlabel", "User Properties:");
	bstrview->SetFontSize(font_size);
	mainView->AddChild(bstrview);
	
	rect.Set(10,30,BWindow::Bounds().right-10,45);
	mUserNameField = new BTextControl(rect, "name","Name:", "", NULL);
	mUserNameField->SetDivider(90);
	mUserNameField->SetFontSize(font_size);
	SetTextViewFontSize(mUserNameField->TextView(), font_size);
	mainView->AddChild(mUserNameField);
		
	rect.Set(10,55,BWindow::Bounds().right-10,70);
	mPasswordField = new BTextControl(rect, "pswd","Password:", "", NULL);
	mPasswordField->SetDivider(90);
	mPasswordField->SetFontSize(font_size);
	SetTextViewFontSize(mPasswordField->TextView(), font_size);
	(mPasswordField->TextView())->HideTyping(true);
	mainView->AddChild(mPasswordField);
	
	//Checkboxes
	
	rect.Set(10,100,BWindow::Bounds().right-10,115);
	mAdminBox = new BCheckBox(rect, "admin", "User has administrative privileges", new BMessage(CMD_USRCONFIG_ADMIN));
	mAdminBox->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mAdminBox->SetFontSize(font_size);
	mainView->AddChild(mAdminBox);

	rect.Set(10,120,BWindow::Bounds().right-10,135);
	mForceChngPswdBox = new BCheckBox(rect, "chngpswd", "User must change password next login", new BMessage(CMD_USRCONFIG_ADMIN));
	mForceChngPswdBox->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mForceChngPswdBox->SetFontSize(font_size);
	mainView->AddChild(mForceChngPswdBox);
	
	rect.Set(10,140,BWindow::Bounds().right-10,155);
	mAcctEnabledBox = new BCheckBox(rect, "acctenble", "Account enabled", new BMessage(CMD_USRCONFIG_ADMIN));
	mAcctEnabledBox->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mAcctEnabledBox->SetFontSize(font_size);
	mainView->AddChild(mAcctEnabledBox);

	rect.Set(10,160,BWindow::Bounds().right-10,175);
	mCanChngPswdBox = new BCheckBox(rect, "canchng", "User can change password", new BMessage(CMD_USRCONFIG_ADMIN));
	mCanChngPswdBox->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mCanChngPswdBox->SetFontSize(font_size);
	mainView->AddChild(mCanChngPswdBox);

	BButton* button = new BButton(BRect(
						BWindow::Bounds().right-80,
						BWindow::Bounds().bottom-40,
						BWindow::Bounds().right-10,
						BWindow::Bounds().bottom-20),"","Save",
						new BMessage(CMD_USRCONFIG_SAVE));
	button->SetFontSize(font_size);
	mainView->AddChild(button);
	
	button = new BButton(BRect(
				BWindow::Bounds().right-160,
				BWindow::Bounds().bottom-40,
				BWindow::Bounds().right-90,
				BWindow::Bounds().bottom-20),"","Cancel",
				new BMessage(CMD_USRCONFIG_CANCEL));
	button->SetFontSize(font_size);
	mainView->AddChild(button);

	//
	//We set up differently based on whether or not we're adding a new user
	//or editing an existing one.
	//
	if (!mIsNewUser)
	{
		BString		password;
		uint32		flags;
		
		if (AFPGetUserInfo(mUserName, &password, &flags) == B_OK)
		{
			mUserNameField->SetText(mUserName);
			mPasswordField->SetText(password.String());
			
			if ((flags & kIsAdmin) != 0) {
				mAdminBox->SetValue(1);
			}
			if ((flags & kUserEnabled) != 0) {
				mAcctEnabledBox->SetValue(1);
			}
			if ((flags & kMustChngPswd) != 0) {
				mForceChngPswdBox->SetValue(1);
			}
			if ((flags & kCanChngPswd) != 0) {
				mCanChngPswdBox->SetValue(1);
			}
		}
		else
		{
			(new BAlert("", "There was an error getting the user info.", "OK"))->Go();
			Quit();
		}

		mUserNameField->SetEnabled(false);
		mPasswordField->MakeFocus(true);
	}
	else
	{
		mUserNameField->MakeFocus(true);
		
		mAcctEnabledBox->SetValue(1);
		mCanChngPswdBox->SetValue(1);
	}
}


/*
 * MessageReceived()
 *
 * Description:
 *
 * Returns:
 */

void afpUserConfig::MessageReceived(BMessage * Message)
{
	status_t	status = B_OK;
	
	switch(Message->what)
	{
		case CMD_USRCONFIG_SAVE:
		{
			BString		pswd;
			char		text[256];
			uint32		flags = 0;
			
			//
			//Make sure we have data to save.
			//
			if (strlen(mUserNameField->Text()) == 0)
			{
				(new BAlert("", "User name must be supplied.", "OK"))->Go();
				break;
			}
			
			if (mIsNewUser)
			{
				if (mAdminBox->Value() > 0) {
				
					flags |= kIsAdmin;
				}
				if (mForceChngPswdBox->Value() > 0) {
					
					flags |= kMustChngPswd;
				}
				if (mAcctEnabledBox->Value() > 0) {
					
					flags |= kUserEnabled;
				}
				if (mCanChngPswdBox->Value() > 0) {
					
					flags |= kCanChngPswd;
				}
				
				status = AFPAddUser(
							mUserNameField->Text(),
							mPasswordField->Text(),
							flags
							);
			}
			else
			{
				status = AFPGetUserInfo(mUserNameField->Text(), &pswd, &flags);
								
				if (BE_SUCCESS(status))
				{
					if (mAdminBox->Value() > 0) {
					
						flags |= kIsAdmin;
					}
					else if ((flags & kIsAdmin) != 0) {
						
						flags &= ~kIsAdmin;
					}
					
					if (mForceChngPswdBox->Value() > 0) {
						
						flags |= kMustChngPswd;
					}
					else if ((flags & kMustChngPswd) != 0) {
						
						flags &= ~kMustChngPswd;
					}
					
					if (mAcctEnabledBox->Value() > 0) {
						
						flags |= kUserEnabled;
					}
					else if ((flags & kUserEnabled) != 0) {
						
						flags &= ~kUserEnabled;
					}
					
					if (mCanChngPswdBox->Value() > 0){
						
						flags |= kCanChngPswd;
					}
					else if ((flags & kCanChngPswd) != 0) {
					
						flags &= ~kCanChngPswd;
					}
			
					status = AFPUpdateUserProperties(
									mUserNameField->Text(),
									mPasswordField->Text(),
									flags
									);
				}
			}
			
			if (!BE_SUCCESS(status))
			{
				sprintf(text, "Adding/Updating user failed! Please try again. (%s)", GET_BERR_STR(status));
				(new BAlert("", text, "OK"))->Go();
			}
			
			be_app->PostMessage(CMD_APP_REFRESHUSERLIST);
			Quit();
			break;
		}
		
		case CMD_USRCONFIG_CANCEL:
			Quit();
			break;
		
		default:
			BWindow::MessageReceived(Message);
			break;
	}
}









