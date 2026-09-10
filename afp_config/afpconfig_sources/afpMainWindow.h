#ifndef __afpMainWindow__
#define __afpMainWindow__

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
#include <Be.h>

#define CMD_APP_GUEST_CHECKBOX_HIT	'gsth'
#define CMD_APP_ADDVOLUME			'addv'
#define CMD_APP_REMOVEVOL			'rmvv'
#define CMD_APP_ADDUSER				'addu'
#define CMD_APP_REMOVEUSER			'rmvu'
#define CMD_APP_SAVELOGONMSG		'slog'
#define CMD_APP_MNGUSERS			'musr'
#define CMD_APP_SENDMSG				'sndm'
#define CMD_APP_ROCHECKBOX			'mkro'
#define CMD_APP_VOLLISTCHANGED		'vchg'
#define CMD_APP_REFRESHVOLS			'refv'
#define CMD_AFP_USERLISTCHANGED		'uchg'

#define CMD_MENU_ABOUT				'abot'
#define CMD_MENU_SENDMSG			'send'
#define CMD_MENU_STARTSERVER		'srts'
#define CMD_MENU_SETHOSTNAME		'shst'

class afpListView : public BListView
{
public:
    afpListView(	BWindow*		win,
                    int32			message,
                    BRect 			frame,
                    const char 		*name,
                    list_view_type 	type = B_SINGLE_SELECTION_LIST,
                    uint32 			resizeMask = B_FOLLOW_LEFT | B_FOLLOW_TOP,
                    uint32 			flags = B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE):
                    
                    BListView(frame,name,type,resizeMask,flags)
                    {
                        mWindow 			= win;
                        mSelChangedMessage	= message;
                        mLastIndex			= -1;
                    }
	
	//
	//This hook is so we can change the controls according
	//to what item is selected in the list.
	//
	void SelectionChanged(void)
	{
		BMessenger	messenger(mWindow);
		BMessage	msg(mSelChangedMessage);
		
		messenger.SendMessage(&msg);
		
		BListView::SelectionChanged();
	}
	
	//
	//I have no idea why, but doublclicks don't seem to work when
	//going through the parent class' MouseDown() function.
	//
	void MouseDown(BPoint point)
	{
		BMessage 	*message 	= Looper()->CurrentMessage();
		int32 		index 		= IndexOf(point);
		bigtime_t	curTime;
		bigtime_t	dblTime;
		
		Window()->CurrentMessage()->FindInt64("when", &curTime);
		get_click_speed(&dblTime);
		
		if (((curTime - mLastClick) < dblTime) && (index == mLastIndex))
		{			
			Invoke();
		}
		
		mLastClick = curTime;
		mLastIndex = index;
		
		BListView::MouseDown(point);
	}
	
private:
	BWindow*		mWindow;
	int32			mSelChangedMessage;
	bigtime_t		mLastClick;
	int32			mLastIndex;
};


/*
 * afpIconButton
 *
 * A small icon-only BButton (no label). It draws the 1-bit refresh glyph
 * from afpRefreshIcon.h in the button's label color, centered in the
 * button, so the button background shows through the transparent pixels —
 * the same look as a browser's reload button. Drawing the icon ourselves
 * (rather than handing a BBitmap to SetIcon) keeps the result independent
 * of how the button renders 1-bit bitmaps.
 */
class afpIconButton : public BButton
{
public:
	afpIconButton(BRect frame, const char* name, BMessage* message,
		const char* tooltip)
		: BButton(frame, name, "", message, B_FOLLOW_LEFT | B_FOLLOW_TOP)
	{
		if (tooltip != NULL)
			SetToolTip(tooltip);
	}

protected:
	virtual void Draw(BRect updateRect);
};


class afpMainWindow : public BWindow
{
public:
	afpMainWindow(const char *uWindowTitle);
	~afpMainWindow();
	
	virtual void 	MessageReceived(BMessage *Message);
	virtual void	SetupMenus(void);
	virtual void 	BuildWindow(void);

	virtual void	PopulateUserList();
	virtual void	PopulateVolumeList();
	virtual void	SaveReadOnlyState();
	virtual void	SetControlsState();
	virtual void	RemoveShare();
	virtual void	AddNewShare(entry_ref newRef);
	virtual void	SaveComment();
	virtual void	GetComment();
	virtual void	UpdateThroughput();
	
	virtual bool 	QuitRequested();

private:

		BMenuBar*		mMenuBar;
		BBox*			mBox;
		afpListView*	mVolumeListView;
		BCheckBox*		mReadOnlyBox;
		BButton*		mRemoveButton;
		afpIconButton*	mRefreshButton;
		afpListView*	mUserListView;
		BStringView*	mServerStatus;
		BButton*		mRemoveUserButton;
		BButton*		mEditUserButton;
		BCheckBox*		mGuestCB;
		BTextView*		mLogonMsgView;
		BStatusBar*		mThroughputBar;
		int32			mHighestBPS;
		int32			mLastBPS;
		BTextControl*	mBytesSent;
		BTextControl*	mBytesRecv;
		BTextControl*	mDSIPackets;
		BTextControl*	mLoggedIn;
		BFilePanel*		mFilePanel;
};


#endif //__afpMainWindow__
