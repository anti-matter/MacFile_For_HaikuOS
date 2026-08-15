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

#endif //DEBUG
