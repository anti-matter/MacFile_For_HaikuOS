#ifndef __afpdesk__
#define __afpdesk__

#include <File.h>
#include <Entry.h>
#include <SupportDefs.h>
#include <memory>
#include <string>

#include "debug.h"
#include "afp.h"

#define DESKTOP_FILE_NAME	".afpdesktop.db"
#define AFP_CMNT_ATTRIBUTE	"Afp_Comment"

typedef uint32 FILETYPE;
typedef uint32 FILECREATOR;

#define MAX_DESKTOP_DATA_SIZE	1024
#define ICON_DATA_SIZE			1024
#define MAX_APPL_PATHNAME		MAX_AFP_STRING_LEN
#define MAX_OPEN_DESKTOPS		16
#define MAX_COMMENT_SIZE		199
#define DONT_USE_INDEX			0

#define ENTRY_TYPE_EMPTY		0
#define ENTRY_TYPE_ICON			1
#define ENTRY_TYPE_APPL			2
#define ENTRY_TYPE_CMNT			3
#define ENTRY_TYPE_RESERVED		4

#if DEBUG
#define DUMP_DT_ENTRY(e,i)		(e)->dump(i)
#else
#define DUMP_DT_ENTRY(e,i)
#endif

struct DESKTOP_ENTRY
{
	int8		entryType; 	//Entry type (icon, APPL, etc.)
	uint32		tag; 		//For private use by Apple and the AFP Client.
	
	FILETYPE	fileType;
	FILECREATOR	fileCreator;
	int32		dirID;
	int8		iconType;
	uint16		dataSize;
	int8		pathType;
	char		path[MAX_AFP_PATH+sizeof(char)];
	char		data[ICON_DATA_SIZE];
	
	DESKTOP_ENTRY() :
		entryType(ENTRY_TYPE_EMPTY),
		tag(0),
		fileType(0),
		fileCreator(0),
		dirID(0),
		iconType(0),
		dataSize(0),
		pathType(0),
		path{},
		data{}
	{
	}
	
	bool equal_comment(const DESKTOP_ENTRY* item) const
    {
        return entryType == ENTRY_TYPE_CMNT && item->entryType == ENTRY_TYPE_CMNT &&
            dirID == item->dirID &&
            strcmp(path, item->path) == 0;
    }

    bool equal_aapl(const DESKTOP_ENTRY* item) const
    {
        return entryType == ENTRY_TYPE_APPL && item->entryType == ENTRY_TYPE_APPL &&
            fileCreator == item->fileCreator;
    }

    bool equal_icon(const DESKTOP_ENTRY* item) const
    {
        return entryType == ENTRY_TYPE_ICON && item->entryType == ENTRY_TYPE_ICON &&
            fileCreator == item->fileCreator &&
            fileType == item->fileType &&
            iconType == item->iconType;
    }
	
	static std::string convert_to_os_type(const uint32 value)
    {
        union union_char
        {
            uint32 d_word;
            char c_array[4];
        };

        std::string result;
        union union_char temp;

        temp.d_word = value;

        result.push_back(temp.c_array[3]);
        result.push_back(temp.c_array[2]);
        result.push_back(temp.c_array[1]);
        result.push_back(temp.c_array[0]);

        return result;
    }

    [[nodiscard]] std::string creator_as_string() const
    {
        return convert_to_os_type(fileCreator);
    }

    [[nodiscard]] std::string file_type_as_string() const
    {
        return convert_to_os_type(fileType);
    }
	
	DESKTOP_ENTRY* operator=(const DESKTOP_ENTRY* item)  // NOLINT(misc-unconventional-assign-operator)
    {
        this->entryType = item->entryType;
        this->fileType = item->fileType;
        this->fileCreator = item->fileCreator;
        this->tag = item->tag;
        this->dirID = item->dirID;
        this->iconType = item->iconType;
        this->dataSize = item->dataSize;
        this->pathType = item->pathType;
        strncpy(this->path, item->path, sizeof(this->path));
        memcpy(this->data, item->data, sizeof(data));

        return this;
    }
	
	void dump(bool include_data = true)
	{
		DBGWRITE(dbg_level_info, "itemType:    %d\n", entryType);
		DBGWRITE(dbg_level_info, "fileType:    %s\n", file_type_as_string().c_str());
		DBGWRITE(dbg_level_info, "fileCreator: %s\n", creator_as_string().c_str());
		DBGWRITE(dbg_level_info, "tag:         %d\n", tag);
		DBGWRITE(dbg_level_info, "dirID:       %d\n", dirID);
		DBGWRITE(dbg_level_info, "iconType:    %d\n", iconType);
		DBGWRITE(dbg_level_info, "dataSize:    %d\n", dataSize);
		DBGWRITE(dbg_level_info, "path:        %s\n", path);

		if (include_data)
		{
			DBG_DUMP_BUFFER(data, dataSize, dbg_level_info);
		}
	}
};

#define NUM_DESK_ENTRIES_TO_CACHE	64
#define DESK_ENTRY_CACHE_SIZE		(NUM_DESK_ENTRIES_TO_CACHE * sizeof(DESKTOP_ENTRY))


AFPERROR FPOpenDT(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPCloseDT(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPAddIcon(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetIcon(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPGetIconInfo(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPAddComment(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetComment(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPRemoveComment(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
AFPERROR FPAddAPPL(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPGetAPPL(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);

AFPERROR FPRemoveAPPL(
	afp_session*	afpSession,
	int8*			afpReqBuffer,
	int8*			afpReplyBuffer,
	int32*			afpDataSize
	);
	
//---------------------------------------------------

AFPERROR afp_FindDTEntry(
	afp_session*	afpSession,
	int16			refnum,
	DESKTOP_ENTRY*	searchCriteria,
	uint16			searchIndex,
	DESKTOP_ENTRY*	foundEntry,
	int32*			foundPosition = nullptr
	);
	
AFPERROR afp_AddEntry(
	afp_session*	afpSession,
	int16			refnum,
	DESKTOP_ENTRY*	dtEntry
	);

AFPERROR afp_RemoveEntry(
	afp_session*	afpSession,
	int16			refnum,
	DESKTOP_ENTRY*	dtEntry
	);

#endif //__afpdesk__
