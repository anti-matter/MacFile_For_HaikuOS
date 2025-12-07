#include <stdio.h>
#include <Screen.h>
#include <TextView.h>

#include "afpAboutWindow.h"
#include "afpConfigUtils.h"
#include "SG_URLView.h"
#include "commands.h"

#define TITLE_STRING		"Macintosh® File Server for Haiku"
#define AFP_VERSION_STRING	"Version 1.8"
#define COPYRIGHT_STRING	"Copyright (c) 2002-2024, Michael J. Conrad."

const float font_size = 14.0f;

afpAboutWindow::afpAboutWindow(BWindow* parent) :
	BWindow(
		BRect(100, 100, 550, 290),
		"",
		B_MODAL_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE
		)
{
	BView*			mainView;
	BButton*		button;
	BRect			rect;
	BStringView*	bstrview;
	char			srvrver[16];
	char			verstring[32];
	
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
	mainView->SetFontSize(font_size);
	AddChild(mainView);
	
	//
	//*****************Add the title text
	//
	rect.Set(BWindow::Bounds().left,10,BWindow::Bounds().right,30);
	bstrview = new BStringView(rect, "", TITLE_STRING);
	bstrview->SetFontSize(18);
	bstrview->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	bstrview->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(bstrview);
	
	rect.Set(BWindow::Bounds().left, 30, BWindow::Bounds().right, 45);
	bstrview = new BStringView(rect, "", COPYRIGHT_STRING);
	bstrview->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	bstrview->SetAlignment(B_ALIGN_CENTER);
	bstrview->SetFontSize(font_size);
	mainView->AddChild(bstrview);
	
	AFPGetServerVersionString(srvrver, sizeof(srvrver));
	memset(verstring, 0, sizeof(verstring));
	sprintf(verstring, "Version %s", srvrver);
	
	rect.Set(BWindow::Bounds().left, 50, BWindow::Bounds().right, 65);
	bstrview = new BStringView(rect, "", verstring);
	bstrview->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	bstrview->SetFontSize(font_size);
	bstrview->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(bstrview);

	//
	//*****************Misc info.
	//
	SG_URLView* urlView;
	
	rect.Set(10, 85, BWindow::Bounds().right, 100);
	urlView = new SG_URLView(
						rect,
						"https://github.com/anti-matter/MacFile_For_HaikuOS/wiki",
						"https://github.com/anti-matter/MacFile_For_HaikuOS/wiki",
						WWW_LINK
						);
	urlView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	urlView->SetFontSize(font_size);
	mainView->AddChild(urlView);

	//
	//Build the OK button. We adjust per the bottom right
	//corner of the dialog, so resizing won't affect positioning.
	//
	button = new BButton(
					BRect(
						((BWindow::Bounds().right-BWindow::Bounds().left) / 2) - 40,
						BWindow::Bounds().bottom-45,
						((BWindow::Bounds().right-BWindow::Bounds().left) / 2) + 40,
						BWindow::Bounds().bottom-25),
					"b1", "OK", new BMessage(CMD_MSG_EXIT)
					);
	button->SetFontSize(font_size);
	mainView->AddChild(button);
	
	SetDefaultButton(button);
	Show();
}

afpAboutWindow::~afpAboutWindow()
{
}


void afpAboutWindow::MessageReceived(BMessage *Message)
{
	switch(Message->what)
	{
		case CMD_MSG_EXIT:
			Quit();
			break;
		
		default:
			break;
	}
}
