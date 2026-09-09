#ifndef __InstallerApp__
#define __InstallerApp__

#include <Application.h>

class InstallerWindow;

class InstallerApp : public BApplication
{
public:
	InstallerApp();
	virtual ~InstallerApp();

	virtual void ReadyToRun();

private:
	InstallerWindow*	fWindow;
};

#endif //__InstallerApp__
