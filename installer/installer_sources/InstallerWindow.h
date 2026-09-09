#ifndef __InstallerWindow__
#define __InstallerWindow__

#include <Window.h>
#include <String.h>

class BButton;
class BStringView;
class BTextView;

class InstallerWindow : public BWindow
{
public:
	InstallerWindow(const BString& releaseDir);
	virtual ~InstallerWindow();

	virtual void MessageReceived(BMessage* message);
	virtual void WindowClosed(bool wasCanceled);

private:
	void RefreshState();
	void AppendLog(const char* line);
	void StartOperation(const char* subcommand);

	BString		fReleaseDir;
	BButton*	fInstallButton;
	BButton*	fUninstallButton;
	BStringView*	fProgressView;
	BStringView*	fStatusView;
	BTextView*	fLogView;
	bool		fBusy;
	bool		fInstalled;
};

#endif //__InstallerWindow__
