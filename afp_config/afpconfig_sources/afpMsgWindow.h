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

#define HELP_MSG		"Send a message to all connected users:"
#define CMD_MSG_CANCEL	'cncl'
#define CMD_MSG_SEND	'send'

class afpMsgWindow : public BWindow
{
public:
	afpMsgWindow(BWindow* parent);
	~afpMsgWindow();

	virtual void 	MessageReceived(BMessage *Message);

private:
	BButton*		mSend;
	BTextView*		mMsgText;
};
