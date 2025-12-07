#include <Alert.h>
#include <Entry.h>
#include <Message.h>
#include <Debug.h>
#include <StorageKit.h>
#include <TextView.h>
#include <Window.h>
#include <Box.h>
#include <Roster.h>
#include <TrackerAddOn.h>

#include "commands.h"
#include "afpConfigApplication.h"

/*
 * main()
 *
 * Description:
 *
 * Returns:
 */

int main(int, char **)
{
	afpConfigApplication* app = NULL;
	
	app = new afpConfigApplication();
	app->Run();
	
	delete be_app;

	return( 0 );
}


/*
 * process_refs()
 *
 * Description:
 *		This function is called by the tracker when a user selects
 *		this add-on.
 *
 * Returns:
 */

void process_refs(entry_ref dir_ref, BMessage* msg, void*)
{
	switch(be_roster->Launch(ApplicationSignature))
	{
		case B_OK:
			break;
		
		default:
			break;
	}
}















