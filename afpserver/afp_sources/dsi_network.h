#ifndef __dsi_network__
#define __dsi_network__

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <net_settings.h>

#include "afpGlobals.h"
#include "afp.h"


void afpInitializeServerNetworking();
status_t afpSrvrConnectThread(void* data);


#endif //__dsi_network__
