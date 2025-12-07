#ifndef __AFPHOSTNAME__
#define __AFPHOSTNAME__

#include "Message.h"
#include "afpGlobals.h"

#ifndef MAX_HOSTNAME_LEN
#define MAX_HOSTNAME_LEN	128
#endif


status_t afp_SetHostname(const char* hostname);
status_t afp_GetHostname(char* hostname, int32 cbHostname);
void afp_FindHostNameFromNetSettings(char* hostname, int32 cbHostname);
void afp_GetHostnameFromSettingsFile(char* hostname, int32 cbHostname);

void afp_HandleGethostnameAppMsg(BMessage* message);
void afp_HandleSethostnameAppMsg(BMessage* message);

#endif //__AFPHOSTNAME__
