#include <mutex>
#include <memory>

#include <String.h>
#include <List.h>
#include <StorageKit.h>
#include <Alert.h>
#include <Beep.h>

#include "commands.h"
#include "afpvolume.h"

std::mutex volume_blist_mutex;
std::unique_ptr<BList> volume_blist = std::make_unique<BList>();

/*
 * SaveVolumeData()
 *
 * Description:
 *		Writes out the volume specific data to a preferences file
 *		so we know about the data when we restart.
 *
 * Returns:
 */

void SaveVolumeData(VolumeStorageData* volData)
{
	BPath		path;
	BNode		file;
	BDirectory	dir;
	char		fpath[256];
	char		attrName[B_ATTR_NAME_LENGTH];
	
	//
	//Save the current volume flag settings to the file.
	//
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK)
	{
		dir.SetTo(path.Path());
		
		if (!dir.Contains(AFP_PREFS_FILE_NAME, B_FILE_NODE))
		{
			DPRINT(("[SaveVolumeData]Creating volume file!!\n"));
			dir.CreateFile(AFP_PREFS_FILE_NAME, NULL, true);
		}
		
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
		
		if (file.InitCheck() == B_OK)
		{			
			sprintf(attrName, "%s%s", AFP_VOLUMES_TYPE, volData->path);
			file.WriteAttr(attrName, 0, 0, volData, sizeof(VolumeStorageData));
			
			DPRINT(("[SaveVolumeData]Saved to attribute name '%s'\n", attrName));
		}
	}

	//
	//Now update the volume object live.
	//
	fp_volume*	volume 	= NULL;
	int			i		= 0;
	
	std::lock_guard lock(volume_blist_mutex);
	
	while((volume = (fp_volume*)volume_blist->ItemAt(i++)) != NULL)
	{
		if (!strcmp(volume->GetPath()->Path(), volData->path))
		{
			volume->SetVolumeFlags(volData->flags);
			break;
		}
	}
}


/*
 * GetVolumeData()
 *
 * Description:
 *		Retrieves information about a specific volume from the
 *		preferences file.
 *
 * Returns:
 */

status_t GetVolumeData(const char* volPath, VolumeStorageData* outData)
{
	BPath				path;
	BNode				file;
	char				fpath[256];
	VolumeStorageData	volData;
	char				attrName[B_ATTR_NAME_LENGTH];
	status_t			status	= B_OK;
	ssize_t				size 	= 0;
	
	status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	
	if (status == B_OK)
	{	
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
		
		status = B_ERROR;
		
		DPRINT(("[GetVolumeData]Looking for path '%s' in attributes\n", volPath));
				
		if (file.InitCheck() == B_OK)
		{			
			while(file.GetNextAttrName(attrName) == B_OK)
			{	
				if (strstr(attrName, AFP_VOLUMES_TYPE))
				{
					DPRINT(("[GetVolumeData]Found '%s' in attributes\n", attrName));
					
					memset(&volData, 0, sizeof(VolumeStorageData));
					
					size = file.ReadAttr(attrName, 0, 0, &volData, sizeof(VolumeStorageData));
					
					if (size > B_OK)
					{
						if (!strcmp(volData.path, volPath))
						{
							memcpy(outData, &volData, sizeof(VolumeStorageData));
							status = B_OK;
							
							break;
						}
					}
				}
			}
		}
	}
	
	return( status );
}


/*
 * StartSharingVolume()
 *
 * Description:
 *		Tells the afp server to begin sharing a specific volume
 *		share point.
 *
 * Returns:
 */
 
 status_t StartSharingVolume(VolumeStorageData* volData)
 {
 	BPath*			bpath;
	fp_volume*		newVol;
	
	bpath = new BPath(volData->path);
	
	if (FindVolume(bpath->Leaf()) != NULL)
	{
		delete bpath;
		
		return( B_ERROR );
	}
	
	DPRINT(("[afpVolume::StartSharingVolume]Now sharing: %s\n", bpath->Leaf()));
	
	newVol = new fp_volume(bpath, volData->flags);
	
	std::lock_guard lock(volume_blist_mutex);
	volume_blist->AddItem(newVol);
	
	WatchVolume(volData->path);
	
	return( B_OK );
 }
 
 
/*
 * StopSharingVolume()
 *
 * Description:
 *		Tells the afp server to stop sharing a specific volume
 *		share point.
 *
 * Returns:
 */

status_t StopSharingVolume(const char* path)
{
	fp_volume*	volume 	= NULL;
	int			i		= 0;
	status_t	status	= B_ERROR;
	
	std::lock_guard lock(volume_blist_mutex);
	
	while((volume = (fp_volume*)volume_blist->ItemAt(i++)) != NULL)
	{
		if (!strcmp(volume->GetPath()->Path(), path))
		{
			volume_blist->RemoveItem(volume);
			delete volume;
						
			status = B_OK;
			break;
		}
	}
		
	StopWatchingVolume(path);
	
	return( status );
}


/*
 * ShareAllVolumes()
 *
 * Description:
 *		Tells the afp_server to share all the volumes listed in
 *		the preferences file.
 *
 * Returns:
 */

status_t ShareAllVolumes()
{
	BPath				path;
	BNode				file;
	char				fpath[256];
	VolumeStorageData	volData;
	char				attrName[B_ATTR_NAME_LENGTH];
	status_t			status	= B_OK;
	ssize_t				size 	= 0;
	
	status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	
	if (status == B_OK)
	{	
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
		
		status = B_ERROR;
				
		if (file.InitCheck() == B_OK)
		{
			while(file.GetNextAttrName(attrName) == B_OK)
			{	
				if (strstr(attrName, AFP_VOLUMES_TYPE))
				{
					memset(&volData, 0, sizeof(VolumeStorageData));
					
					size = file.ReadAttr(attrName, 0, 0, &volData, sizeof(VolumeStorageData));
					
					if (size > B_OK) {
					
						StartSharingVolume(&volData);
					}
				}
			}
		}
	}
	
	return( B_OK );
}


/*
 * FindVolume()
 *
 * Description:
 *		Find a volume by its volume id.
 *
 * Returns:
 */

fp_volume* FindVolume(uint16 volID)
{
	fp_volume*	afpVolume 	= NULL;
	uint16		i			= 0;
	
	std::lock_guard lock(volume_blist_mutex);
		
	while((afpVolume = (fp_volume*)volume_blist->ItemAt(i)) != NULL)
	{
		if (afpVolume->GetVolumeID() == volID)
		{
			break;
		}
		
		i++;
	}
			
	return( afpVolume );
}


/*
 * FindVolume()
 *
 * Description:
 *		Find a volume by its volume name.
 *
 * Returns:
 */

fp_volume* FindVolume(const char* volName)
{
	fp_volume*	afpVolume 	= NULL;
	int16		i			= 0;
	
	std::lock_guard lock(volume_blist_mutex);
	
	while((afpVolume = (fp_volume*)volume_blist->ItemAt(i)) != NULL)
	{
		if (!strcmp(afpVolume->GetVolumeName(), volName))
			break;
		
		i++;
	}
			
	return( afpVolume );
}


/*
 * FindVolume()
 *
 * Description:
 *		Find a volume by its volume node_ref.
 *
 * Returns:
 */

fp_volume* FindVolume(node_ref nref)
{
	fp_volume*	afpVolume 	= NULL;
	BDirectory*	dir			= NULL;
	int16		i			= 0;
	node_ref	ourRef;
	
	std::lock_guard lock(volume_blist_mutex);
	
	while((afpVolume = (fp_volume*)volume_blist->ItemAt(i)) != NULL)
	{
		dir = afpVolume->GetDirectory();
		
		if (dir != NULL)
		{
			dir->GetNodeRef(&ourRef);
			
			if (ourRef == nref) {
				break;
			}
		}
		
		i++;
	}
			
	return( afpVolume );
}


/*
 * MarkAllVolumesClean()
 *
 * Description:
 *		Marks all volumes as clean. This is usually called by the
 *		scavenger thread.
 *
 * Returns:
 */

void MarkAllVolumesClean()
{
	std::lock_guard lock(volume_blist_mutex);
	
	fp_volume* volume;
	int j = 0;
	
	while((volume = (fp_volume*)volume_blist->ItemAt(j++)) != NULL)
	{
		if (volume->IsDirty()) {
		
			volume->MakeClean();
		}
	}
}


/*
 * RemoveVolumeData()
 *
 * Description:
 *		Remove a volume from the preferences file.
 *
 * Returns:
 */

status_t RemoveVolumeData(const char* volName)
{
	BPath		path;
	BNode		file;
	BDirectory	dir;
	char		fpath[256];
	char		attrName[B_ATTR_NAME_LENGTH];
	size_t		size	= 0;
	status_t	status 	= B_OK;
	
	status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	
	if (status == B_OK)
	{		
		sprintf(fpath, "%s/%s", path.Path(), AFP_PREFS_FILE_NAME);
		file.SetTo(fpath);
				
		if (file.InitCheck() == B_OK)
		{
			sprintf(attrName, "%s%s", AFP_VOLUMES_TYPE, volName);	
			size = file.RemoveAttr(attrName);
		}
		else {
		
			status = B_ERROR;
		}
	}
	
	return( status );
}


/*
 * WatchVolume()
 *
 * Description:
 *
 * Returns:
 */

void WatchVolume(const char* path)
{
	BEntry		entry(path);
	node_ref	nref;
	status_t	status;
	
	if (entry.InitCheck() == B_OK)
	{
		entry.GetNodeRef(&nref);
		
		status = watch_node(
					&nref,
					B_ENTRY_REMOVED | B_ENTRY_MOVED | B_WATCH_NAME,
					be_app_messenger
					);
					
		if (status != B_OK)
		{
			DPRINT(("[afpServerApplication:WatchVolume]watch_node failed for %s!\n", path));
		}
	}
}


/*
 * StopWatchingVolume()
 *
 * Description:
 *
 * Returns:
 */

void StopWatchingVolume(const char* path)
{
	BEntry		entry(path);
	node_ref	nref;
	
	if (entry.InitCheck() == B_OK)
	{
		entry.GetNodeRef(&nref);
		watch_node(&nref, B_STOP_WATCHING, be_app_messenger);
	}
}


/*
 * StopWatchingVolume()
 *
 * Description:
 *
 * Returns:
 */

void StopWatchingVolume(node_ref nref)
{
	watch_node(&nref, B_STOP_WATCHING, be_app_messenger);
}
