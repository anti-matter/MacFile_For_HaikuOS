#include "afpConfigUtils.h"

#include <Entry.h>
#include <stdio.h>

void StopSharingUAMVolume()
{
	BEntry		oldUAM("/boot/home/BeUAM_Volume");
	BPath		path;
	status_t	result;
	
	printf("Stop sharing old BeUAM_Volume...\n");
	
	if (oldUAM.GetPath(&path) == B_OK)
	{
		BString str(path.Path());
		
		result = AFPRemoveShare(&str);
		
		if (result != be_afp_success)
		{
			printf("Could not stop sharing %s (%d)\n", str.String(), result);
			return;
		}
	}
	else {
		
		printf("Failed to remove old uam share.\n");
		return;
	}
	
	printf("BeUAM_Volume no longer shared.\n");
}


void ShareVolume()
{
	int			result = B_OK;
	BEntry		entry("/boot/home/MacUtilityVolume");
	entry_ref	newRef;
	BPath		path;
	
	if (!entry.Exists())
	{
		printf("Sharing volume failed, folder does not exist.\n");
		return;
	}
	
	if (entry.GetRef(&newRef) != B_OK)
	{
		printf("Error setting directory ref!!\n");
		return;
	}
	
	result = AFPAddShare(newRef, &path);
	
	if (result == B_OK) {
		
		printf("%s shared successfully!\n", path.Path());
	}
	else {
	
		printf("Sharing %s failed!!\n", path.Path());
	}
}

int main(int, char **)
{
	//
	//First, stop sharing the old deprecated UAM volume.
	//
	StopSharingUAMVolume();
	
	ShareVolume();
	
	return( 0 );
}
