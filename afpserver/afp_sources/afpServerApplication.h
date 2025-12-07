#ifndef __afpServerApplication__
#define __afpServerApplication__
	
#include "afpGlobals.h"

#define ApplicationSignature "application/x-vnd.afp_server"

#include <Application.h>

class afpServerApplication : public BApplication
{
public:
	afpServerApplication();
	~afpServerApplication();
	
	virtual void MessageReceived(BMessage* message);
	
	virtual void ReadyToRun();
	virtual void Pulse();
};
	
#endif //__afpServerApplication__
