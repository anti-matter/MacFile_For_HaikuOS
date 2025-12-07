#ifndef __AFPSETHOSTWIN__
#define __AFPSETHOSTWIN__

#include <Application.h>
#include <View.h>
#include <Window.h>
#include <TextView.h>
#include <ScrollView.h>
#include <CheckBox.h>
#include <Beep.h>
#include <Button.h>
#include <TextControl.h>
#include <StatusBar.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Box.h>
#include <String.h>
#include <Alert.h>
#include <StorageKit.h>
#include <Message.h>
#include <ListView.h>
#include <FilePanel.h>
#include <StringView.h>

#define HOST_HELP_MSG	"Enter the name of this server:"
#define CMD_MSG_CANCEL	'cncl'
#define CMD_MSG_SET		'sset'
#define CMD_MSG_DEFAULT	'dflt'

class afpSetHostWin : public BWindow
{
public:
	afpSetHostWin(BWindow* parent);
	~afpSetHostWin();

	virtual void 	MessageReceived(BMessage *Message);
	virtual void	FindHostNameFromNetSettings(char* hostname, int32 cbHostname);

private:
	BTextControl*	mNameTxt;
};


#endif //__AFPSETHOSTWIN__
