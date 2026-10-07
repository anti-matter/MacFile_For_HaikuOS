#if DEBUG

#include <cctype>
#include <cstddef>
#include <string>
#include <OS.h>
#include "debug.h"

const std::string currentDateTime()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

void afp_debug_write(const dbg_level debug_level, const std::string& function, const char* format, ...)
{
	if (debug_level > current_debug_level)
	{
		return;
	}

	va_list args;
	va_start(args, format);
	
	std::string timestamp = "[" + currentDateTime() + "]";

	std::string level;
    switch (debug_level)
    {
    case dbg_level_error:
		level = "[ERROR]";
		break;
    case dbg_level_warning:
		level = "[WARNING]";
		break;
    case dbg_level_info:
		level = "[INFO]";
		break;
    case dbg_level_trace:
		level ="[TRACE]";
		break;
    case dbg_level_dump_in:
		level = "[DUMP__IN]";
        break;
    case dbg_level_dump_out:
		level = "[DUMP_OUT]";
        break;
    }

	std::string combined;

	if (debug_level != dbg_level_dump_in && debug_level != dbg_level_dump_out)
	{
		combined = timestamp + level + "[" + function + "]" + format;
	}
	else
	{
		// Dumping buffer.
		combined = level + " " + format;
	}

	char buffer[1024];
	vsprintf(buffer, combined.c_str(), args);
	
	printf("%s", buffer);
	
	va_end(args);
}

std::string char_to_hex(unsigned char value)
{
    static constexpr char hexChars[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(2);

    result.push_back(hexChars[(value >> 4) & 0x0F]);
    result.push_back(hexChars[value & 0x0F]);

    return result;
}

void hex_dump(
    const void* data,
    std::size_t length,
    dbg_level level)
{
    if (level > current_debug_level)
        return;

    const auto* buffer =
        static_cast<const unsigned char*>(data);

    std::string line;
    std::string readable;

    line.reserve(16 * 3 + 4 + 16);
    readable.reserve(16);

    for (std::size_t i = 0; i < length; ++i)
    {
        if (i != 0 && i % 16 == 0)
        {
            line.append("    ");

            for (unsigned char ch : readable)
            {
                line.push_back(
                    std::isprint(static_cast<unsigned char>(ch))
                        ? static_cast<char>(ch)
                        : '.');
            }

            DBGWRITE(level, "%s\n", line.c_str());

            line.clear();
            readable.clear();
        }

        const unsigned char ch = buffer[i];

        line.append(char_to_hex(ch));
        line.push_back(' ');
        readable.push_back(static_cast<char>(ch));
    }

    if (!readable.empty())
    {
        const std::size_t missing = 16 - readable.size();
        line.append(missing * 3, ' ');
        line.append("    ");

        for (unsigned char ch : readable)
        {
            line.push_back(
                std::isprint(static_cast<unsigned char>(ch))
                    ? static_cast<char>(ch)
                    : '.');
        }

        DBGWRITE(level, "%s\n", line.c_str());
    }
}

#else // DEBUG

/*
 * Release-mode logging.
 *
 * In release builds only warnings and errors reach afp_debug_write() — every
 * lower level is compiled out at the DBGWRITE call site (see debug.h). Each
 * accepted message is formatted like the debug output ("[timestamp][LEVEL]
 * [function]message") and appended to a fixed 64 MiB in-memory ring buffer.
 * A background flusher thread periodically rewrites ~/afpserverlog.txt with
 * the ring's current contents, so the file always holds approximately the
 * newest 64 MiB of warnings/errors and never grows beyond that.
 *
 * Design notes:
 *  - The file is rewritten at most once per flush interval and only when
 *    something new was logged (dirty flag) — never per message.
 *  - The mutex is held only for the ring memcpy into the scratch buffer,
 *    never during file I/O.
 *  - The log fd is opened once (lazily, on first flush) and kept open for the
 *    lifetime of the server; it is closed during shutdown after a final flush.
 *  - Any failure (home directory lookup, open failure, short write) silently
 *    and permanently disables file logging. The logging system never logs its
 *    own failures, and a disabled/broken logger cannot affect AFP serving.
 */

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <FindDirectory.h>
#include <OS.h>
#include <Path.h>

#include <fcntl.h>
#include <unistd.h>

#include "debug.h"

namespace {

constexpr size_t   kLogCapacity     = 64 * 1024 * 1024;		// max retained log data
constexpr bigtime_t kLogFlushInterval = 1 * 1000 * 1000;	// file rewrite period (us)
constexpr bigtime_t kLogSleepSlice    = 100 * 1000;			// stop-check granularity (us)

struct release_log
{
	char   ring[kLogCapacity];
	size_t head    = 0;		// offset of the oldest byte once wrapped
	size_t len     = 0;		// valid bytes currently held
	bool   wrapped = false;	// ring has overflowed; oldest byte lives at head
	bool   dirty   = false;	// ring changed since the last file rewrite
	bool   started = false;	// flusher thread already spawned
};

release_log gLog;

// Static snapshot buffer for log_flush_once() — far too large for a thread
// stack at this capacity, so it lives beside the ring instead.
char gLogScratch[kLogCapacity];

std::mutex gLogMutex;
std::thread gLogFlusher;
int gLogFd = -1;
std::atomic<bool> gLogDisabled{false};
std::atomic<bool> gLogStop{false};

const std::string currentDateTime()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

	return buf;
}

const char* level_tag(const dbg_level level)
{
	switch (level)
	{
		case dbg_level_error:   return "[ERROR]";
		case dbg_level_warning: return "[WARNING]";
		default:                return "[INFO]";
	}
}

// Append src[0..n) to the ring, discarding the oldest bytes on overflow.
// Must be called with gLogMutex held.
void ring_append_locked(const char* src, size_t n)
{
	if (n >= kLogCapacity)
	{
		// Keep only the newest bytes of this single message.
		src += n - kLogCapacity;
		n = kLogCapacity;
	}

	const size_t writePos = (gLog.head + gLog.len) % kLogCapacity;
	const size_t first    = (kLogCapacity - writePos < n) ? kLogCapacity - writePos : n;

	memcpy(gLog.ring + writePos, src, first);
	if (n != first)
		memcpy(gLog.ring, src + first, n - first);

	if (gLog.len < kLogCapacity)
	{
		if (gLog.len + n <= kLogCapacity)
		{
			gLog.len += n;
		}
		else
		{
			gLog.head    = (gLog.head + (gLog.len + n - kLogCapacity)) % kLogCapacity;
			gLog.len     = kLogCapacity;
			gLog.wrapped = true;
		}
	}
	else
	{
		gLog.head = (gLog.head + n) % kLogCapacity;
	}

	gLog.dirty = true;
}

// Copy the ring's contents into scratch in chronological order; returns bytes.
// Must be called with gLogMutex held.
size_t ring_snapshot_locked(char* scratch)
{
	if (!gLog.wrapped)
	{
		memcpy(scratch, gLog.ring, gLog.len);
		return gLog.len;
	}

	memcpy(scratch, gLog.ring + gLog.head, kLogCapacity - gLog.head);
	memcpy(scratch + (kLogCapacity - gLog.head), gLog.ring, gLog.head);

	return kLogCapacity;
}

bool open_log_file()
{
	BPath path;

	if (find_directory(B_USER_DIRECTORY, &path) != B_OK)
		return false;

	if (path.Append("afpserverlog.txt") != B_OK)
		return false;

	const int fd = open(path.Path(), O_RDWR | O_CREAT | O_TRUNC, 0644);

	if (fd < 0)
		return false;

	gLogFd = fd;

	return true;
}

bool write_all(int fd, const char* data, size_t n)
{
	while (n > 0)
	{
		const ssize_t written = write(fd, data, n);

		if (written < 0)
		{
			if (errno == EINTR)
				continue;
			return false;
		}

		data += written;
		n -= (size_t)written;
	}

	return true;
}

// Rewrite the log file with the ring's current contents. No-op when nothing
// changed since the last rewrite.
void log_flush_once()
{
	size_t n    = 0;
	bool   have = false;

	{
		std::lock_guard<std::mutex> lock(gLogMutex);

		if (gLog.dirty)
		{
			n    = ring_snapshot_locked(gLogScratch);
			gLog.dirty = false;
			have = true;
		}
	}

	if (!have)
		return;

	if (gLogFd < 0 && !open_log_file())
	{
		// Cannot create the log file: silently disable file logging.
		gLogDisabled.store(true);
		return;
	}

	// ftruncate() empties the file but leaves the fd's offset at the old EOF;
	// without the rewind, the next write() lands at that stale offset and the
	// vacated range reads back as NUL bytes (a sparse hole).
	if (ftruncate(gLogFd, 0) != 0 || lseek(gLogFd, 0, SEEK_SET) < 0 || !write_all(gLogFd, gLogScratch, n))
	{
		gLogDisabled.store(true);
		return;
	}
}

int log_flusher_thread()
{
	while (!gLogStop.load())
	{
		// Sleep in short slices so shutdown stays responsive.
		for (bigtime_t waited = 0; waited < kLogFlushInterval && !gLogStop.load(); waited += kLogSleepSlice)
			snooze(kLogSleepSlice);

		log_flush_once();
	}

	// Final flush so the last messages survive shutdown.
	log_flush_once();

	return 0;
}

// Stops the flusher (final flush included) and closes the log file when the
// translation unit's statics are destroyed at process exit.
struct release_log_shutdown
{
	~release_log_shutdown()
	{
		gLogStop.store(true);

		if (gLogFlusher.joinable())
			gLogFlusher.join();

		if (gLogFd >= 0)
		{
			::close(gLogFd);
			gLogFd = -1;
		}
	}
};

release_log_shutdown gLogShutdown;

} // namespace

void afp_debug_write(const dbg_level debug_level, const std::string& function, const char* format, ...)
{
	if (gLogDisabled.load(std::memory_order_relaxed))
		return;

	std::string combined;

	combined = "[" + currentDateTime() + "]" + level_tag(debug_level) + "[" + function + "]" + format;

	char buffer[1024];

	va_list args;
	va_start(args, format);
	const int written = vsnprintf(buffer, sizeof(buffer), combined.c_str(), args);
	va_end(args);

	if (written < 0)
		return;

	const size_t len = ((size_t)written >= sizeof(buffer)) ? sizeof(buffer) - 1 : (size_t)written;

	bool startFlusher = false;

	{
		std::lock_guard<std::mutex> lock(gLogMutex);
		ring_append_locked(buffer, len);
		startFlusher = !gLog.started;
		gLog.started = true;
	}

	if (startFlusher)
	{
		try
		{
			gLogFlusher = std::thread(log_flusher_thread);
		}
		catch (...)
		{
			// Could not spawn the flusher: disable logging rather than fail.
			gLogDisabled.store(true);
		}
	}
}

#endif // DEBUG
