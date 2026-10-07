#ifndef __fp_objects__
#define __fp_objects__

#include <Entry.h>
#include <Directory.h>

#include "afp.h"
#include "fp_volume.h"
#include "afp_buffer.h"
#include "afp_session.h"

#define AFP_FINFO_ATTRIBUTE	"Afp_FinderInfo"
#define AFP_RSRC_ATTRIBUTE	"Afp_Resource"
#define AFP_ATTR_ATTRIBUTE	"Afp_Attributes"
#define AFP_ATTR_LONGNAME	"Afp_Longname"
// Stores the true Mac name of an entry whose Mac name is "." or ".." (which
// no filesystem can store literally, being self/parent references). The
// on-disk name holds each '.' mapped to byte+128; this attribute carries the
// real Mac name so it can be returned to the client verbatim. See
// GetAFPName/SetAFPName and afp_buffer::GetString.
#define AFP_ATTR_CHARMAP	"Afp_CharMap"

// These macros make working with posix perms easier. For our purposes, we
// treat the posix perms as follows:
// 
// 				Owner		User		Guest
// 		Read	S_IRUSR		S_IRGRP		S_IROTH
// 		Write	S_IWUSR		S_IWGRP		S_IWOTH
// 		Search	S_IXUSR		S_IXGRP		S_IXOTH
// 

// Owner permissions
#define	POSIX_AFP_IROWNER	S_IRUSR
#define POSIX_AFP_IWOWNER	S_IWUSR
#define POSIX_AFP_ISOWNER	S_IXUSR

// User permissions
#define POSIX_AFP_IRUSER	S_IRGRP
#define POSIX_AFP_IWUSER	S_IWGRP
#define POSIX_AFP_ISUSER	S_IXGRP

// Guest permissions
#define POSIX_AFP_IRGUEST	S_IROTH
#define POSIX_AFP_IWGUEST	S_IWOTH
#define POSIX_AFP_ISGUEST	S_IXOTH


class fp_objects : public BEntry
{
public:

	enum
	{
		kUserType_Guest = 1,
		kUserType_User,
		kUserType_Owner
	}AFPUserType;
	
						fp_objects(const char* volumePath, int32 afpDirID);
						~fp_objects();
	
	static AFPERROR 	SetAFPEntry(
									fp_volume*		afpVolume,
									int32 			afpDirID,
									const char*		afpPathname,
									BEntry& 		afpEntry,
									bool			traverse=false
									);
	
	static AFPERROR 	GetEntryFromFileId(
									BDirectory* 	volumeDirectory,
									uint32		 	fileId,
									BEntry& 		afpEntry
									);
									
	static AFPERROR		GetEntryByLongName(BDirectory& dir, const char* afpPathname, BEntry& afpEntry);
	static void			CreateLongName(char* afpPathname, BEntry* afpEntry, bool afpHardCreate=false);

	// Get the Mac name of an entry for return to the client (the mapped
	// on-disk name, or the true Mac name when the entry's name is "."/"..").
	static AFPERROR		GetAFPName(BEntry* afpEntry, char* name, size_t nameSize);

	// Persist the true Mac name of a created/renamed entry when its name had
	// to be mapped for the filesystem (see AFP_ATTR_CHARMAP).
	static void			SetAFPName(BEntry* afpEntry, const char* macPathname);

	static AFPERROR		fp_GetDirParms(
									afp_session*	afpSession,
									fp_volume*		afpVolume,
									BEntry* 		afpEntry,
									int16			afpDirBitmap,
									afp_buffer* 	afpReply
									);
	
	static AFPERROR		fp_GetFileParms(
									afp_session*	afpSession,
									fp_volume*		afpVolume,
									BEntry* 		afpEntry,
									int16			afpFileBitmap,
									afp_buffer* 	afpReply
									);
									
	static AFPERROR 	fp_SetFileDirParms(
									afp_session*	afpSession,
									afp_buffer*		afpRequest,
									BEntry* 		afpEntry,
									int16			afpBitmap
									);
									
	static mode_t		AFPtoBPermissions(
									int32			afpPerms
									);
	
	static uint32 		BtoAFPPermissions(
									int16			userType,
									mode_t 			bperms
									);
	
	static AFPERROR 	GetAFPFinderInfo(
									BEntry* 		afpEntry,
									FINDER_INFO*	afpFInfo
									);
									
	static AFPERROR 	SetAFPFinderInfo(
									BEntry* 		afpEntry,
									FINDER_INFO*	afpFInfo
									);

	static AFPERROR 	GetAFPAttributes(
									BEntry* 		afpEntry,
									int16*			afpAttributes
									);

	static AFPERROR 	SetAFPAttributes(
									BEntry* 		afpEntry,
									int16*			afpAttributes
									);
	
	static status_t 	CopyAttrs(BNode& inFrom, BNode& inTo);
	static status_t 	CopyFile(BFile& inFrom, BFile& inTo);
	
private:
};


#endif // __fp_objects__
