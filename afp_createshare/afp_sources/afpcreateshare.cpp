/*
 *	afpcreateshare.cpp
 *
 *	AFP Server add-on to share directories.
 */

#include <Alert.h>
#include <Entry.h>
#include <Message.h>
#include <Debug.h>
#include <StorageKit.h>
#include <TrackerAddOn.h>

#include "commands.h"
#include "afpConfigUtils.h"
#include "afpcreateshare.h"


/*
 * process_refs()
 *
 * Description:
 *		This function is called by the tracker when a user selects
 *		this add-on.
 *
 * Returns:
 */
extern "C" void
process_refs(entry_ref dir_ref, BMessage* msg, void*)
{
	uint32		type	= 0;
	int32		count	= 0;
	int			result	= 0;
	BAlert*		alert	= NULL;
	entry_ref	ref;
	BEntry		entry;
	BPath		path;
	char		textmsg[256];
	
	msg->GetInfo("refs", &type, &count);
	
	if (type != B_REF_TYPE)
	{
		//
		//We were not given a type of entry_ref, there's nothing
		//for us to do here.
		//
		return;
	}
	
	//
	//Cycle through each directory we've been handed and tell the AFP
	//server to serve it up as a volume.
	//
	for (	count = 0;
			msg->FindRef("refs", count, &ref) == B_NO_ERROR;
			count++ )
	{
		entry.SetTo(&ref);
		
		if (entry.InitCheck() == B_OK)
		{
			BDirectory	dir(&entry);
			entry.GetPath(&path);

			//
			//AFP can only share directories, files and links will not be
			//and cannot be shared.
			//
			if (!entry.IsDirectory())
			{
				sprintf(textmsg, "%s is not a directory and cannot be shared", path.Path());
				
				(new BAlert("", textmsg, "OK"))->Go();
				continue;
			}
			
			//
			//We do not allow sharing of the root directory, this causes
			//many, many problems.
			//
			if (dir.IsRootDirectory())
			{
				sprintf(textmsg, "Sorry, sharing of an entire volume is not permitted.");
				
				(new BAlert("", textmsg, "OK"))->Go();
				continue;
			}
			
			sprintf(textmsg, "Are you sure you want to share this directory?\n\n %s", path.Path());

			//
			//Initialize our alert object and setup it's parameters.
			//
			alert = new BAlert(
							"",
							textmsg,
							"Cancel",
							"Yes"
							);
			
			alert->SetShortcut(0, B_ESCAPE);
			result = alert->Go();
			
			if (result == 1)
			{
				result = AFPAddShare(ref, &path);
				
				switch(result)
				{
					case be_afp_success:
						sprintf(textmsg, "AFP share\n\n%s\n\ncreated successfully!", path.Path());
						(new BAlert("", textmsg, "OK"))->Go();
						break;
					
					case be_afp_sharealreadyexits:
						sprintf(textmsg, "The directory:\n\n%s\n\nis already shared out by AFP", path.Path());
						(new BAlert("", textmsg, "OK"))->Go();
						break;
						
					default:
						sprintf(textmsg, "Error sharing the directory %s!\n\nError code: %d", path.Path(), result);
						(new BAlert("", textmsg, "OK"))->Go();
						break;
				}
			}
		}
	}
}


/*
 * AFPCreateShare()
 *
 * Description:
 *
 * Returns:
 */

AFPCreateShare::AFPCreateShare() : BApplication("application/x-vnd.afpcreate")
{
	return;
}

AFPCreateShare::~AFPCreateShare()
{
	return;
}


/*
 * main()
 *
 * Description:
 *
 * Returns:
 */

int main()
{
	new AFPCreateShare();
	
	(new BAlert("", "AFP Create Share Add-on.\n\nSelect a directory, then select this add-on.", "OK"))->Go();
	
	delete be_app;

	return( 0 );
}













