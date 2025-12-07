#ifndef __afpUserConfig__
#define __afpUserConfig__

#include <Application.h>
#include <View.h>
#include <Window.h>
#include <TextView.h>
#include <ScrollView.h>
#include <CheckBox.h>
#include <Beep.h>
#include <Button.h>
#include <TextControl.h>

#define CMD_APP_REFRESHUSERLIST	'rusr'

class afpUserConfig : public BWindow
{
public:
		afpUserConfig(BWindow* parent, bool inNewUser, const char* userName = NULL);
		~afpUserConfig();
		
	virtual void 	MessageReceived(BMessage *Message);
	virtual void 	BuildWindow(void);
		
	BTextControl*	mUserNameField;
	BTextControl*	mPasswordField;
	BCheckBox*		mAdminBox;
	BCheckBox*		mForceChngPswdBox;
	BCheckBox*		mAcctEnabledBox;
	BCheckBox*		mCanChngPswdBox;
	bool			mIsNewUser;
	char			mUserName[128];
};


#endif //__afpUserConfig__
