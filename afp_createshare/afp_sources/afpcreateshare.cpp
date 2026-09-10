/*
 *	afpcreateshare.cpp
 *
 *	AFP Server tracker add-on to share directories.
 *
 *	This is a tracker add-on: the tracker loads it as a shared library and
 *	calls process_refs() when the user picks it from the Add-ons menu after
 *	selecting one or more items. It is not a standalone application.
 *
 *	Note on the dialogs: process_refs() runs on the tracker's private
 *	"Add-on" thread, which is a plain thread and NOT a BLooper. Destroying a
 *	BWindow (such as a BAlert) after its Go() call has returned double-frees
 *	in the BLooper destructor (the window's own message queue is freed twice)
 *	and crashes the tracker. Go() has already closed the window, so we let
 *	each BAlert leak rather than delete it — a small, bounded leak in a
 *	rarely-invoked add-on is the safe trade-off for not crashing the tracker.
 */

#include <stdio.h>

#include <Alert.h>
#include <Entry.h>
#include <Message.h>
#include <StorageKit.h>
#include <TrackerAddOn.h>

#include "commands.h"
#include "afpConfigUtils.h"


/*
 * process_refs()
 *
 * Description:
 *		This function is called by the tracker when a user selects
 *		this add-on from the Add-ons menu. Each selected directory is
 *		offered up as a new AFP volume.
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
	char		textmsg[B_PATH_NAME_LENGTH + 128];

	msg->GetInfo("refs", &type, &count);

	if (type != B_REF_TYPE) {
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

		if (entry.InitCheck() == B_OK) {
			BDirectory	dir(&entry);
			entry.GetPath(&path);

			//
			//AFP can only share directories, files and links will not be
			//and cannot be shared.
			//
			if (!entry.IsDirectory()) {
				snprintf(textmsg, sizeof(textmsg),
					"%s is not a directory and cannot be shared",
					path.Path());

				(new BAlert("", textmsg, "OK"))->Go();
				continue;
			}

			//
			//We do not allow sharing of the root directory, this causes
			//many, many problems.
			//
			if (dir.IsRootDirectory()) {
				snprintf(textmsg, sizeof(textmsg),
					"Sorry, sharing of an entire volume is not permitted.");

				(new BAlert("", textmsg, "OK"))->Go();
				continue;
			}

			snprintf(textmsg, sizeof(textmsg),
				"Are you sure you want to share this directory?\n\n %s",
				path.Path());

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

			//
			//Deliberately NOT deleted: see the note at the top of this
			//file. Deleting the BAlert here (on the non-looper add-on
			//thread) double-frees and crashes the tracker.
			//

			if (result == 1) {
				result = AFPAddShare(ref, &path);

				switch(result)
				{
					case be_afp_success:
						snprintf(textmsg, sizeof(textmsg),
							"AFP share\n\n%s\n\ncreated successfully!",
							path.Path());
						(new BAlert("", textmsg, "OK"))->Go();
						break;

					case be_afp_sharealreadyexits:
						snprintf(textmsg, sizeof(textmsg),
							"The directory:\n\n%s\n\nis already shared out by AFP",
							path.Path());
						(new BAlert("", textmsg, "OK"))->Go();
						break;

					default:
						snprintf(textmsg, sizeof(textmsg),
							"Error sharing the directory %s!\n\nError code: %d",
							path.Path(), result);
						(new BAlert("", textmsg, "OK"))->Go();
						break;
				}
			}
		}
	}
}
