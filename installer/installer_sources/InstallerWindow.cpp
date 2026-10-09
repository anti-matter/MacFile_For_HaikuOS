#include <stdio.h>
#include <string.h>

#include <Alert.h>
#include <AppFileInfo.h>
#include <Application.h>
#include <Button.h>
#include <Entry.h>
#include <File.h>
#include <Messenger.h>
#include <Path.h>
#include <Screen.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>

#include <private/app/AppMisc.h>

#include "InstallerWindow.h"
#include "InstallWorker.h"

#define TITLE_STRING	"MacFile for Haiku"
#define DESC_STRING		"Install, upgrade, or uninstall the MacFile AFP file server."
#define SERVER_PATH		"/boot/home/config/non-packaged/apps/afp_server"

const float font_size = 14.0f;

enum
{
	CMD_INSTALL		= 'inst',
	CMD_UPGRADE		= 'upgr',
	CMD_UNINSTALL	= 'unin',
	CMD_QUIT		= 'quit'
};

/*
 * read_app_version()
 *
 * Description:
 *		Reads the app_version resource of the application image at
 *		path and formats it as "major.middle.minor".
 *
 * Returns:
 *		true on success, false if the file or its version info could
 *		not be read.
 */

static bool
read_app_version(const char* path, BString& version)
{
	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;

	BAppFileInfo info(&file);
	if (info.InitCheck() != B_OK)
		return false;

	version_info versionData;
	if (info.GetVersionInfo(&versionData, B_APP_VERSION_KIND) != B_OK)
		return false;

	char text[48];
	snprintf(text, sizeof(text), "%lu.%lu.%lu",
		(unsigned long)versionData.major,
		(unsigned long)versionData.middle,
		(unsigned long)versionData.minor);
	version = text;
	return true;
}

/*
 * InstallerWindow()
 *
 * Description:
 *		Builds the installer window: title, description, a version line
 *		(installed version vs. version this installer carries), status
 *		and progress lines, the action buttons, and the log.
 *
 * Returns:
 */

InstallerWindow::InstallerWindow(const BString& releaseDir) :
	BWindow(
		BRect(0, 0, 480, 387),
		"MacFile Installer",
		B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE
		),
	fReleaseDir(releaseDir),
	fInstallButton(NULL),
	fUpgradeButton(NULL),
	fUninstallButton(NULL),
	fVersionView(NULL),
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
	//*****************Versions
	//
	//Persistent line showing the installed server version (if any) and
	//the version this installer will install. Unlike the status line it is
	//not overwritten while an operation is in flight.
	//
	rect.Set(10, 70, 470, 88);
	fVersionView = new BStringView(rect, "versions", "");
	fVersionView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	fVersionView->SetFontSize(font_size);
	fVersionView->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(fVersionView);

	//
	//*****************Status and progress
	//
	rect.Set(10, 102, 470, 120);
	fStatusView = new BStringView(rect, "status", "");
	fStatusView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	fStatusView->SetFontSize(font_size);
	fStatusView->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(fStatusView);

	//
	//This Haiku build has no BProgressBar, so progress is shown as a
	//percentage in a centered string view.
	//
	fProgressView = new BStringView(BRect(20, 128, 460, 146), "progress", "");
	fProgressView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	fProgressView->SetFontSize(font_size);
	fProgressView->SetAlignment(B_ALIGN_CENTER);
	mainView->AddChild(fProgressView);

	//
	//*****************Buttons
	//
	rect.Set(20, 158, 120, 180);
	fInstallButton = new BButton(rect, "install", "Install",
		new BMessage(CMD_INSTALL));
	fInstallButton->SetFontSize(font_size);
	mainView->AddChild(fInstallButton);

	rect.Set(130, 158, 230, 180);
	fUpgradeButton = new BButton(rect, "upgrade", "Upgrade",
		new BMessage(CMD_UPGRADE));
	fUpgradeButton->SetFontSize(font_size);
	mainView->AddChild(fUpgradeButton);

	rect.Set(240, 158, 340, 180);
	fUninstallButton = new BButton(rect, "uninstall", "Uninstall",
		new BMessage(CMD_UNINSTALL));
	fUninstallButton->SetFontSize(font_size);
	mainView->AddChild(fUninstallButton);

	rect.Set(350, 158, 460, 180);
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
	//
	//The log area is deliberately SHORT (half of the previous height):
	//Height = 377 - 192 = 185px = half of the old 370px. The width (446px)
	//is unchanged. The window was grown from 355 to 387 px to make room
	//for the version line (everything below the description shifted down
	//32px), so the log's bottom at y=377 plus the standard 10px buffer
	//leaves no empty space below the log.
	//
	BRect logRect(10, 192, 470 - 14, 377);
	//
	//BTextView wraps text to the width of its content rect (the 3rd
	//constructor argument), NOT its frame. _UpdateInsets() computes the
	//top/bottom/left/right insets as the difference between the frame and
	//this content rect, and BOTH are measured in the BTextView's OWN
	//coordinate system -- Bounds() is (0, 0, width, height), origin at the
	//view's top-left. So the content rect must be expressed relative to the
	//view origin, NOT in window coordinates: a small symmetric 2px inset on
	//every side. (Passing the frame's absolute coordinates -- e.g. top = 162
	//-- would make the top inset 162px and push the first line of text
	//halfway down the area, while clamping the right inset to 0 only by
	//accident of the frame being wider than the rect.)
	//
	BRect textRect(2, 2, logRect.Width() - 2, logRect.Height() - 2);
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

	UpdateVersionView();
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
	//
	//Install is for a fresh install (only when nothing is installed). Upgrade
	//and Uninstall are only available once an installation is present.
	//
	fInstallButton->SetEnabled(!fBusy && !fInstalled);
	fUpgradeButton->SetEnabled(!fBusy && fInstalled);
	fUninstallButton->SetEnabled(!fBusy && fInstalled);

	if (!fBusy)
		fStatusView->SetText(fInstalled ? "MacFile is installed." : "MacFile is not installed.");
}

/*
 * UpdateVersionView()
 *
 * Description:
 *		Refresh the version line: the version of the installed
 *		afp_server (if any) and the version this installer will
 *		install.
 *
 *		The "version to install" is this installer's own app_version.
 *		bump-version.sh keeps the installer's rdef, the server's rdef,
 *		and the AFP_SERVER_VERSION macro in lockstep at release time,
 *		and install.zip is packaged from the same build, so the
 *		installer's version is the payload's version.
 *
 * Returns:
 */

void InstallerWindow::UpdateVersionView()
{
	//
	//The version this installer will install: read the app_version
	//resource out of our own binary.
	//
	BString installerVersion("unknown");

	char appPath[B_PATH_NAME_LENGTH];
	if (BPrivate::get_app_path(appPath) == B_OK)
		read_app_version(appPath, installerVersion);

	//
	//The version currently on the system. "none" when nothing is
	//installed; "unknown" when a server binary is present but carries no
	//readable version (e.g. a pre-2.x build).
	//
	BString installedVersion;
	if (!fInstalled)
		installedVersion = "none";
	else if (!read_app_version(SERVER_PATH, installedVersion))
		installedVersion = "unknown";

	char text[128];
	snprintf(text, sizeof(text), "Installed version: %s    Version to install: %s",
		installedVersion.String(), installerVersion.String());
	fVersionView->SetText(text);
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
 *		Spawn the backend worker thread for install, upgrade, or uninstall.
 *
 * Returns:
 */

void InstallerWindow::StartOperation(const char* subcommand)
{
	fBusy = true;
	fOperation = subcommand;
	RefreshState();
	fProgressView->SetText("");

	const char* statusText;
	if (strcmp(subcommand, "install") == 0)
		statusText = "Starting install...";
	else if (strcmp(subcommand, "upgrade") == 0)
		statusText = "Starting upgrade...";
	else
		statusText = "Starting uninstall...";
	fStatusView->SetText(statusText);

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

		case CMD_UPGRADE:
			if (!fBusy)
				StartOperation("upgrade");
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
			UpdateVersionView();
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
				//The install/upgrade/uninstall paths do not emit a STATUS line,
				//so re-detect the real install state from disk before refreshing
				//the buttons. This is what flips Uninstall off / Install on
				//after an uninstall (and the reverse after an install/upgrade).
				//
				BEntry serverEntry(SERVER_PATH);
				fInstalled = serverEntry.Exists() && serverEntry.IsFile();

				//
				//The version line must follow the new state (install and
				//upgrade change the installed version, uninstall removes it).
				//
				UpdateVersionView();

				//
				//Refresh the buttons for the new install state. This also sets
				//the status line, which we override below with the specific
				//completion message.
				//
				RefreshState();

				//
				//Operation-specific completion message, shown in the status
				//line and the completion alert.
				//
				const char* successText;
				const char* failureText;
				if (fOperation == "upgrade")
				{
					successText = "The AFP Server was upgraded successfully.";
					failureText = "The AFP Server could not be upgraded. See the log for details.";
				}
				else if (fOperation == "uninstall")
				{
					successText = "The AFP Server was uninstalled successfully.";
					failureText = "The AFP Server could not be uninstalled. See the log for details.";
				}
				else
				{
					successText = "The AFP Server was installed successfully.";
					failureText = "The AFP Server could not be installed. See the log for details.";
				}

				const char* finalText = success ? successText : failureText;
				fStatusView->SetText(finalText);

				BAlert* alert = new BAlert("", finalText, "OK");
				alert->Go();
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
