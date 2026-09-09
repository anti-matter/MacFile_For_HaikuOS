#include <Path.h>
#include <private/app/AppMisc.h>

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

	//
	//Resolve the directory containing this application's binary. The
	//backend script (install-macfile.sh) and the payload archive
	//(install.zip) live alongside it, so this is the release directory
	//the worker needs.
	//
	char appPath[B_PATH_NAME_LENGTH];
	if (BPrivate::get_app_path(appPath) == B_OK)
	{
		appDir.SetTo(appPath);
		appDir.GetParent(&appDir);
	}

	//
	//Fall back to the default install location if the release directory
	//could not be resolved.
	//
	if (appDir.Path()[0] == '\0')
		appDir.SetTo("/boot/home/config/non-packaged/apps");

	fWindow = new InstallerWindow(appDir.Path());
	fWindow->Show();
}
