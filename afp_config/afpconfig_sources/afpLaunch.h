#ifndef __afpLaunch__
#define __afpLaunch__
 
#include <image.h>
#include <OS.h>
#include <stdlib.h>

//
//This is the path to where we expect the afp_server image file to reside
//
#define PATH_AFPSERVER_IMAGE_FILE	"/boot/home/config/bin/afp_server"

int AFPLaunchServer(void);

#endif //__afpLaunch__