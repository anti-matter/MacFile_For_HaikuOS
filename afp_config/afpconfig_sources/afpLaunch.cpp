#include <Alert.h>

#include "afpLaunch.h"
#include "afpConfigUtils.h"

extern char **environ;

/*
 * AFPLaunchServer()
 *
 * Description:
 *		Launches the afp server if it is not already running. Note
 *		that the afp_server image file must reside in /boot/home/config/bin.
 *
 * Returns:
 *		Error on failure, B_OK otherwise.
 */

int AFPLaunchServer(void)
{
	char**		arg_v;
	int32		arg_c;
	thread_id	exec_thread;
	
	if (AFPServerIsRunning() == true)
	{
		//
		//If the server is already, then there's nothing to do. Tell the
		//user we're not going to do anything.
		//
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
	
	arg_c	= 1;
	arg_v	= (char **)malloc(sizeof(char *) * (arg_c + 1));
	
	if (arg_v != NULL)
	{
		arg_v[0]	= strdup(PATH_AFPSERVER_IMAGE_FILE);
		arg_v[1]	= NULL;
		
		exec_thread = load_image(arg_c, (const char**)arg_v, (const char**)environ);
		
		while(--arg_c >= 0)
			free(arg_v[arg_c]);
		
		free(arg_v);
		
		if (exec_thread != B_ERROR)
		{
			resume_thread(exec_thread);
			return( B_OK );
		}
	}
	
	return( B_ERROR );
}