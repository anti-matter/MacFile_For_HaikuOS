#ifndef __InstallWorker__
#define __InstallWorker__

#include <String.h>
#include <Messenger.h>
#include <SupportDefs.h>

//
//Message codes posted by the worker thread to the installer window.
//
enum
{
	INSTALL_M_PROGRESS	= 'prog',	// int32 "percent", string "label"
	INSTALL_M_STATUS	= 'stat',	// string "state" (installed|not_installed)
	INSTALL_M_LOG		= 'lgnl',	// string "line"
	INSTALL_M_DONE		= 'done'	// string "result" (success|failure)
};

class InstallWorker
{
public:
	//
	//Spawn a thread that runs <releaseDir>/install-macfile.sh <subcommand>
	//and posts protocol lines to <target>.
	//
	static void Spawn(
		const BString&	releaseDir,
		const char*		subcommand,
		BMessenger*		target
		);

private:
	static int32 worker_loop(void* arg);
};

#endif //__InstallWorker__
