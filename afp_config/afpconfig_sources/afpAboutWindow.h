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

#define CMD_MSG_EXIT	'exit'

class afpAboutWindow : public BWindow
{
public:
	afpAboutWindow(BWindow* parent);
	~afpAboutWindow();

	virtual void 	MessageReceived(BMessage *Message);

private:
};