#include <stdio.h>
#include <string.h>

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Entry.h>
#include <Messenger.h>
#include <Screen.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>

#include "InstallerWindow.h"
#include "InstallWorker.h"

#define TITLE_STRING	"MacFile for Haiku"
#define DESC_STRING		"Install or uninstall the MacFile AFP file server."
#define SERVER_PATH		"/boot/home/config/non-packaged/apps/afp_server"

const float font_size = 14.0f;

enum
{
	CMD_INSTALL		= 'inst',
	CMD_UNINSTALL	= 'unin',
	CMD_QUIT		= 'quit'
};

/*
 * InstallerWindow()
 *
 * Description:
 *
 * Returns:
 */

InstallerWindow::InstallerWindow(const BString& releaseDir) :
	BWindow(
		BRect(0, 0, 480, 540),
		"MacFile Installer",
		B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE
		),
	fReleaseDir(releaseDir),
	fInstallButton(NULL),
	fUninstallButton(NULL),
	fProgressView(NULL),
	fStatusView(NULL),
	fLogView(NULL),
	fBusy(false),
	fInstalled(false)
{
	BView*		mainView;
	BButton*	button;
	BRect		rect;
	BStringView*	bstrview;
	BScrollView*	logScroll;

	//
	//Center on the screen.
	//
	BRect screen = BScreen().Frame();
	MoveTo(
		screen.left + (screen.Width() / 2) - (Bounds().Width() / 2),
		screen.top + (screen.Height() / 3) - (Bounds().Height() / 2)
		);

	mainView = new BView(Bounds(), "MainView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
	mainView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mainView->SetFontSize(font_size);
	AddChild(mainView);

	//
	//*****************Title and description
	//
	rect.Set(10, 10, 470, 34);
	bstrview = new BStringView(rect, "", TITLE_STRING);
	bstrview->SetFontSize(18);
	bstrview->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	bstrview->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(bstrview);

	rect.Set(10, 38, 470, 56);
	bstrview = new BStringView(rect, "", DESC_STRING);
	bstrview->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	bstrview->SetFontSize(font_size);
	bstrview->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(bstrview);

	//
	//*****************Status and progress
	//
	rect.Set(10, 70, 470, 88);
	fStatusView = new BStringView(rect, "status", "");
	fStatusView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	fStatusView->SetFontSize(font_size);
	fStatusView->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(fStatusView);

	//
	//This Haiku build has no BProgressBar, so progress is shown as a
	//percentage in a centered string view.
	//
	fProgressView = new BStringView(BRect(20, 96, 460, 114), "progress", "");
	fProgressView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	fProgressView->SetFontSize(font_size);
	fProgressView->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(fProgressView);

	//
	//*****************Buttons
	//
	rect.Set(20, 126, 140, 148);
	fInstallButton = new BButton(rect, "install", "Install",
		new BMessage(CMD_INSTALL));
	fInstallButton->SetFontSize(font_size);
	mainView->AddChild(fInstallButton);

	rect.Set(150, 126, 270, 148);
	fUninstallButton = new BButton(rect, "uninstall", "Uninstall",
		new BMessage(CMD_UNINSTALL));
	fUninstallButton->SetFontSize(font_size);
	mainView->AddChild(fUninstallButton);

	rect.Set(320, 126, 460, 148);
	button = new BButton(rect, "quit", "Quit", new BMessage(CMD_QUIT));
	button->SetFontSize(font_size);
	mainView->AddChild(button);
	SetDefaultButton(fInstallButton);

	//
	//*****************Log
	//
	//The BTextView is the scrollview's target. Per the proven pattern in
	//afpMsgWindow.cpp, the target's frame is in the PARENT (window)
	//coordinate system and represents the scrollview's INTERIOR. The
	//BScrollView then sizes its outer frame to the target's frame PLUS the
	//14px vertical scrollbar. So set the target's right edge to 470 - 14 =
	//456, and the scrollview's outer right edge lands at 470 -- the same
	//10px buffer as every other element. Do NOT call MoveTo/ResizeTo; the
	//BScrollView adopts the target's frame automatically.
	//
	BRect logRect(10, 160, 470 - 14, 530);
	//
	//BTextView wraps text to the width of its content rect (the 3rd
	//constructor argument), NOT its frame. _UpdateInsets() derives the
	//left/right insets from the difference between the frame and this
	//content rect, so the content rect must span nearly the full frame
	//width or the text wraps early. A fixed narrow rect (e.g. 240 wide)
	//left a ~216px right inset in our 446px frame, wrapping at ~half.
	//Span the frame with a small symmetric 2px inset on every side so the
	//log text uses the full interior width of the scroll view.
	//
	BRect textRect(logRect.left + 2, logRect.top + 2,
		logRect.right - 2, logRect.bottom - 2);
	fLogView = new BTextView(logRect, "log",
		textRect, 0, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
	fLogView->MakeEditable(false);
	fLogView->MakeSelectable(true);
	fLogView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	fLogView->SetFontSize(font_size);

	logScroll = new BScrollView("logScroll", fLogView,
		B_FOLLOW_LEFT | B_FOLLOW_TOP, 0, false, true);
	mainView->AddChild(logScroll);

	//
	//Detect whether the server is installed.
	//
	BEntry serverEntry(SERVER_PATH);
	fInstalled = serverEntry.Exists() && serverEntry.IsFile();

	RefreshState();
}

/*
 * ~InstallerWindow()
 *
 * Description:
 *
 * Returns:
 */

InstallerWindow::~InstallerWindow()
{
}

/*
 * QuitRequested()
 *
 * Description:
 *		Quit the whole application (not just this window) so the
 *		MacFileInstaller process exits. A BApplication does not quit
 *		when its last window closes on its own. This fires both when
 *		the user clicks the Quit button and when the window's close
 *		box is clicked, since both call BWindow::Quit().
 *
 * Returns:
 *		true, so the window closes as normal.
 */

bool InstallerWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return BWindow::QuitRequested();
}

/*
 * RefreshState()
 *
 * Description:
 *		Enable/disable buttons and set the status text per the
 *		current install state.
 *
 * Returns:
 */

void InstallerWindow::RefreshState()
{
	fInstallButton->SetEnabled(!fBusy && !fInstalled);
	fUninstallButton->SetEnabled(!fBusy && fInstalled);

	if (!fBusy)
		fStatusView->SetText(fInstalled ? "MacFile is installed." : "MacFile is not installed.");
}

/*
 * AppendLog()
 *
 * Description:
 *
 * Returns:
 */

void InstallerWindow::AppendLog(const char* line)
{
	fLogView->Insert(line);
	fLogView->Insert("\n");
	fLogView->ScrollToSelection();
}

/*
 * StartOperation()
 *
 * Description:
 *		Spawn the backend worker thread for install or uninstall.
 *
 * Returns:
 */

void InstallerWindow::StartOperation(const char* subcommand)
{
	fBusy = true;
	RefreshState();
	fProgressView->SetText("");
	fStatusView->SetText(strcmp(subcommand, "install") == 0
		? "Starting install..." : "Starting uninstall...");

	BMessenger messenger(this);
	InstallWorker::Spawn(fReleaseDir, subcommand, &messenger);
}

/*
 * MessageReceived()
 *
 * Description:
 *
 * Returns:
 */

void InstallerWindow::MessageReceived(BMessage* message)
{
	switch(message->what)
	{
		case CMD_INSTALL:
			if (!fBusy)
				StartOperation("install");
			break;

		case CMD_UNINSTALL:
			if (!fBusy)
				StartOperation("uninstall");
			break;

		case CMD_QUIT:
			//
			//Quit the whole application, not just this window. BWindow::Quit()
			//only closes the window and leaves the BApplication (and process)
			//running; posting B_QUIT_REQUESTED to be_app exits the process.
			//This is the same path the window close box takes via
			//QuitRequested(), and it is the proven idiom from afp_config.
			//
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;

		case INSTALL_M_PROGRESS:
		{
			int32 percent = 0;
			message->FindInt32("percent", &percent);

			char progressText[16];
			snprintf(progressText, sizeof(progressText), "%d%%", (int)percent);
			fProgressView->SetText(progressText);

			const char* label = message->FindString("label");
			if (label != NULL)
				fStatusView->SetText(label);
			break;
		}

		case INSTALL_M_STATUS:
		{
			const char* state = message->FindString("state");
			fInstalled = (state != NULL) && (strcmp(state, "installed") == 0);
			RefreshState();
			break;
		}

		case INSTALL_M_LOG:
		{
			const char* line = message->FindString("line");
			if (line != NULL)
				AppendLog(line);
			break;
		}

		case INSTALL_M_DONE:
		{
			//
			//The script and the worker each emit DONE; only act on
			//the first one.
			//
			if (fBusy)
			{
				const char* result = message->FindString("result");
				bool success = (result != NULL) && (strcmp(result, "success") == 0);

				fBusy = false;

				//
				//The install/uninstall paths do not emit a STATUS line, so
				//re-detect the real install state from disk before refreshing
				//the buttons. This is what flips Uninstall off / Install on
				//after an uninstall (and the reverse after an install).
				//
				BEntry serverEntry(SERVER_PATH);
				fInstalled = serverEntry.Exists() && serverEntry.IsFile();

				RefreshState();

				BAlert* alert = new BAlert("",
					success ? "The operation completed successfully."
					        : "The operation failed. See the log for details.",
					"OK");
				alert->Go();
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
