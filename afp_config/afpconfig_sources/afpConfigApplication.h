#ifndef __afpServerApplication__
#define __afpServerApplication__

#define ApplicationSignature "application/x-vnd.MacFile"

#include <Application.h>
#include "afpMainWindow.h"

class afpConfigApplication : public BApplication
{
public:
	afpConfigApplication();
	~afpConfigApplication();
	
	virtual void ReadyToRun();
	virtual void Pulse();
	virtual void MessageReceived(BMessage * Message);
	virtual void RefsReceived(BMessage* message);
	
private:
	afpMainWindow 	*iMainWindow;
};
	
#endif //__afpServerApplication__
