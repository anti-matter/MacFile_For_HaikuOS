#include "finder_info.h"

#include <filesystem>

namespace fs = std::filesystem;

struct mac_extension_map
{
	char	extension[8];
	char	type[5];
	char	creator[5];
};

static mac_extension_map extMap[] = {  // NOLINT(misc-use-anonymous-namespace)
    {.extension = ".sit",   .type = "SIT5", .creator = "SIT!"},
    {.extension = ".hqx",   .type = "TEXT", .creator = "SITx"},
    {.extension = ".zip",   .type = "ZIP ", .creator = "SITx"},
    {.extension = ".bin",   .type = "BINA", .creator = "SITx"},
    {.extension = ".txt",   .type = "TEXT", .creator = "ttxt"},
    {.extension = ".img",   .type = "dimg", .creator = "ddsk"},
    {.extension = ".image", .type = "dimg", .creator = "ddsk"}
};

void FinderInfoBasedOnExtension(std::string filename, FINDER_INFO* finfo)
{
	std::string result;
	std::string extension;
	fs::path filepath = filename;
	
	extension = filepath.extension();
	
	memset(finfo, 0, sizeof(FINDER_INFO));

	for (int i = 0; i < sizeof(extMap); i++)
	{
		if (extension == extMap[i].extension)
		{
			memcpy(finfo->fdType, extMap[i].type, 4);
			memcpy(finfo->fdCreator, extMap[i].creator, 4);
			
			return;
		}
	}
	
	memcpy(finfo->fdType, "????", 4);
	memcpy(finfo->fdCreator, "????", 4);
}
