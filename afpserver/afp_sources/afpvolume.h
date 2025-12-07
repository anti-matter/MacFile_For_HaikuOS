#ifndef __afpvolume__
#define __afpvolume__

#include <Path.h>
#include <Directory.h>
#include <BlockCache.h>

#include "afpGlobals.h"
#include "fp_volume.h"

#define VOLUME_DATA_V1	0x0001

//
//This is how the volume meta data is stored on disk.
//
typedef struct
{
	uint16		version;		//See defines above
	char		path[256];
	uint32		flags;
}VolumeStorageData, *PVolumeStorageData;


void 		SaveVolumeData(VolumeStorageData* volData);
status_t 	GetVolumeData(const char* volName, VolumeStorageData* volData);
status_t 	StartSharingVolume(VolumeStorageData* volData);
status_t 	StopSharingVolume(const char* path);
status_t 	ShareAllVolumes();
fp_volume* 	FindVolume(uint16 volID);
fp_volume* 	FindVolume(const char* volName);
fp_volume* 	FindVolume(node_ref nref);
void 		MarkAllVolumesClean();
status_t 	RemoveVolumeData(const char* volName);

void 		WatchVolume(const char* path);
void 		StopWatchingVolume(const char* path);
void 		StopWatchingVolume(node_ref nref);

#endif //__afpvolume__
