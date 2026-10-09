#include <Alert.h>
#include <Entry.h>

#include "afpLaunch.h"
#include "afpConfigUtils.h"

extern char **environ;

/*
 * AFPLaunchServer()
 *
 * Description:
 *		Launches the afp server if it is not already running. The server
 *		image is looked for where the installer puts it
 *		(/boot/home/config/non-packaged/apps), falling back to the legacy
 *		BeOS-era location (/boot/home/config/bin).
 *
 * Returns:
 *		Error on failure, B_OK otherwise.
 */

int AFPLaunchServer(void)
{
	thread_id	exec_thread;

	if (AFPServerIsRunning() == true)
	{
		// If the server is already running, then there's nothing to do. Tell
		// the user we're not going to do anything.
		BAlert*	alert	= NULL;

		alert = new BAlert(
						"",
						"The MacFile server is already running.",
						"OK"
						);

		alert->SetShortcut(0, B_ESCAPE);
		alert->Go();

		return( B_OK );
	}

	// Find the server image. Try the location the installer uses first,
	// then fall back to the legacy path for older installations.
	static const char* const sServerPaths[] =
	{
		PATH_AFPSERVER_IMAGE_FILE,
		PATH_AFPSERVER_IMAGE_FILE_LEGACY,
	};

	const char*	serverPath = NULL;

	for (uint32 i = 0; i < sizeof(sServerPaths) / sizeof(sServerPaths[0]); i++)
	{
		BEntry entry(sServerPaths[i]);

		if (entry.Init() == B_OK && entry.Exists())
		{
			serverPath = sServerPaths[i];
			break;
		}
	}

	if (serverPath == NULL)
		return( B_ENTRY_NOT_FOUND );

	const char*	arg_v[]	= { serverPath, NULL };

	exec_thread = load_image(1, arg_v, (const char**)environ);

	// load_image() returns a thread id on success, or a negative error
	// code (not necessarily B_ERROR) on failure. Only resume and report
	// success when we actually got a thread id back.
	if (exec_thread >= B_OK)
	{
		resume_thread(exec_thread);
		return( B_OK );
	}

	return( (int)exec_thread );
}
