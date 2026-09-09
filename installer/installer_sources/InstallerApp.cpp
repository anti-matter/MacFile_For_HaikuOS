#include <FindDirectory.h>
#include <Path.h>

#include "InstallerApp.h"
#include "InstallerWindow.h"

#define APP_SIGNATURE	"application/x-vnd.MacFileInstaller"

/*
 * InstallerApp()
 *
 * Description:
 *
 * Returns:
 */

InstallerApp::InstallerApp()
:
	BApplication(APP_SIGNATURE),
	fWindow(NULL)
{
}

/*
 * ~InstallerApp()
 *
 * Description:
 *
 * Returns:
 */

InstallerApp::~InstallerApp()
{
}

/*
 * ReadyToRun()
 *
 * Description:
 *		Build the installer window. The release directory is the
 *		directory containing this application's binary, so the
 *		backend script and install.zip are found next to it.
 *
 * Returns:
 */

void InstallerApp::ReadyToRun()
{
	BPath appDir;

	if (find_directory(B_APP_DIRECTORY, &appDir) != B_OK)
		appDir.SetTo("/boot/home/config/non-packaged/apps");

	fWindow = new InstallerWindow(appDir.Path());
	fWindow->Show();
}
