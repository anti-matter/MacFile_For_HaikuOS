#ifndef __debug__
#define __debug__

#if DEBUG

#include <string>

typedef int dbg_level;
enum
{
	dbg_level_error = 1,
	dbg_level_warning,
	dbg_level_info,
	dbg_level_trace,
	dbg_level_dump_in,	// incoming data
	dbg_level_dump_out  // outgoing data
};

const dbg_level current_debug_level = dbg_level_trace;

#define DBGWRITE(level, message, ...) afp_debug_write(level, __func__, message, ##__VA_ARGS__)
void afp_debug_write(const dbg_level debug_level, const std::string& function, const char* format, ...);

void dump_bitmap(int32_t bitmap, const dbg_level level);
#define DBG_DUMP_BITMAP(bm, level) dump_bitmap(bm, level);

#ifndef __HAIKU_ARCH_X86
void hex_dump(const char* buffer, const size_t length, const dbg_level level);
#define DBG_DUMP_BUFFER(buffer, cb_buffer, level) hex_dump(buffer, cb_buffer, level)
#endif //__HAIKU_ARCH_X86

#else //DEBUG

#define DBGWRITE(level, message, ...)
#define DBG_DUMP_BITMAP(bm, level)
#define DBG_DUMP_BUFFER(buffer, cb_buffer, level)

#endif

#endif //__debug__

