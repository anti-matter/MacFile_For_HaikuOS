#ifndef __debug__
#define __debug__

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

#if DEBUG

// Debug builds: everything goes to stdout, filtered only by the runtime level.
const dbg_level current_debug_level = dbg_level_trace;

#define DBGWRITE(level, message, ...) afp_debug_write(level, __func__, message, ##__VA_ARGS__)
void afp_debug_write(const dbg_level debug_level, const std::string& function, const char* format, ...);

void dump_bitmap(int32_t bitmap, const dbg_level level);
#define DBG_DUMP_BITMAP(bm, level) dump_bitmap(bm, level);

void hex_dump(
    const void* data,
    std::size_t length,
    dbg_level level);

#define DBG_DUMP_BUFFER(buffer, cb_buffer, level) hex_dump(buffer, cb_buffer, level)

#else // DEBUG

// Release builds: info/trace/dump output is compiled out at the call site —
// the level is a compile-time constant, so the constant condition below folds
// away and the call disappears entirely (zero runtime cost). Only warnings and
// errors reach afp_debug_write(), which appends them to a bounded in-memory
// ring buffer that is periodically persisted to ~/afpserverlog.txt (see the
// release section of debug.cpp for the ring/flusher implementation).
#define DBGWRITE(level, message, ...)																			\
			do {																								\
				if ((level) <= dbg_level_warning)																	\
					afp_debug_write((level), __func__, (message), ##__VA_ARGS__);									\
			} while (0)

void afp_debug_write(const dbg_level debug_level, const std::string& function, const char* format, ...);

#define DBG_DUMP_BITMAP(bm, level)
#define DBG_DUMP_BUFFER(buffer, cb_buffer, level)

#endif

#endif // __debug__
