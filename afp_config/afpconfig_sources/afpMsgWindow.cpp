#include <stdio.h>
#include <Screen.h>
#include <TextView.h>

#include "afpMsgWindow.h"
#include "afpConfigUtils.h"
#include "commands.h"

#define MSGWIN_TITLE	"Send Message"

const float font_size = 14.0f;

/*
 * afpMsgWindow()
 *
 * Description:
 *
 * Returns:
 */

afpMsgWindow::afpMsgWindow(BWindow* parent) :
	BWindow(
		BRect(100, 100, 400, 315),
		MSGWIN_TITLE,
		B_MODAL_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE
		)
{
	BView*			mainView;
	BButton*		button;
	BRect			rect;
	BStringView*	bstrview;
	
	rect = parent->Frame();
	MoveTo(
		(rect.left + (rect.Width()/2))  - (BWindow::Bounds().Width()/2),
		(rect.top  + (rect.Height()/2)) - (BWindow::Bounds().Height()/2)
		);
	
	//
	//Setup the background color for dialogs.
	//
	mainView = new BView(BRect(0,0,BWindow::Bounds().right, BWindow::Bounds().bottom), "MainView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS);
	mainView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(mainView);
	
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
					"b1", "Send", new BMessage(CMD_MSG_SEND)
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

	//
	//Build the editable text box and title for message entry.
	//
	rect.Set(10, 40, BWindow::Bounds().right-25, BWindow::Bounds().bottom-50);
	mMsgText = new BTextView(
							rect,
							"txtmsg",
							BRect(2,2,240,150),
							0,
							B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE
							);
	mMsgText->SetFontSize(font_size);
	SetTextViewFontSize(mMsgText, font_size);
	
	mainView->AddChild(new BScrollView(
							"scroll_txtnmsg",
							mMsgText,
							B_FOLLOW_LEFT | B_FOLLOW_TOP,
							0,
							false,
							true)
							);

	mMsgText->MakeEditable(true);
	mMsgText->MakeSelectable(true);
	mMsgText->SetMaxBytes(199);
	mMsgText->MakeFocus(true);

	rect.Set(rect.left-3, rect.top-22, rect.right, rect.top-2);
	bstrview = new BStringView(rect, "", HELP_MSG);
	bstrview->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	bstrview->SetFontSize(font_size);
	mainView->AddChild(bstrview);

	Show();
}


/*
 * ~afpMsgWindow()
 *
 * Description:
 *
 * Returns:
 */

afpMsgWindow::~afpMsgWindow()
{
}


/*
 * MessageReceived()
 *
 * Description:
 *
 * Returns:
 */

void afpMsgWindow::MessageReceived(BMessage *Message)
{
	int 	error = 0;
	char	msg[256];
	
	switch(Message->what)
	{
		case CMD_MSG_SEND:
			if (mMsgText->TextLength() > 0)
			{
				error = AFPSendMessage(mMsgText->Text(), mMsgText->TextLength());
				
				if (error != B_OK)
				{
					sprintf(msg, "An error occured sending message!\n\nError Code: %d", error);
					(new BAlert("", msg, "OK"))->Go();
				}
				else {
				
					(new BAlert("", "The message was sent successfully!", "OK"))->Go();
				}
			}
			else
			{
				(new BAlert("", "The message must contain at least 1 character.", "OK"))->Go();
				break;
			}
			
			//
			//Let it fall through so the window closes
			//	
		case CMD_MSG_CANCEL:
			Quit();
			break;
		
		default:
			break;
	}
}



