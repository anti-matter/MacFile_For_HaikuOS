#ifndef __afpLaunch__
#define __afpLaunch__
 
#include <image.h>
#include <OS.h>
#include <stdlib.h>

// This is the path to where we expect the afp_server image file to reside.
// It must match BIN_DIRECTORY in distribution/install-macfile.sh (and
// SERVER_PATH in the installer app), which is where the installer extracts
// the server binary to.
#define PATH_AFPSERVER_IMAGE_FILE	"/boot/home/config/non-packaged/apps/afp_server"

// Legacy location used by old installers. Checked as a fallback so users
// with an older installation can still launch the server.
#define PATH_AFPSERVER_IMAGE_FILE_LEGACY	"/boot/home/config/bin/afp_server"

int AFPLaunchServer(void);

#endif // __afpLaunch__