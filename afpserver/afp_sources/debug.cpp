#if DEBUG

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

#ifndef __HAIKU_ARCH_X86
std::string char_to_hex(const char n)
{
    constexpr char hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	std::string result;

	result.push_back(hex_chars[(n & 0xF0) >> 4]);
	result.push_back(hex_chars[(n & 0x0F) >> 0]);

	return result;
}

void hex_dump(const char* buffer, const size_t length, const dbg_level level)
{
	if (level > current_debug_level)
	{
		return;
	}

	std::string line;
	std::string readable;
    uint32_t j = 0;

    for (uint32_t i = 0; i < length; i++)
	{
	    if (i % 16 == 0 && j > 0)
	    {
			line.append("    ");

			for (uint32_t k = 0; k < j; k++)
			{
				line.push_back(std::isprint(readable[k]) ? readable[k] : '.');
			}

			DBGWRITE(level, "%s\n", line.c_str());

			line.erase();
			readable.erase();

			j = 0;
	    }

		char ch = buffer[i];
		if (ch < 0)
		{
			ch = 0;
		}

        if (std::string part = char_to_hex(ch); part.length() == 1)
		{
			line.push_back('0');
			line.append(part);
			line.push_back(' ');
		}
		else
		{
			line.append(part);
			line.push_back(' ');
		}

		readable.push_back(ch);
		j++;
	}

	if (j != 0)
	{
	    for (uint32_t i = j; i <= 15; i++)
	    {
			line.append("   ");
	    }

		line.append("    ");

		for (uint32_t k = 0; k < j; k++)
		{
			line.push_back(std::isprint(readable[k]) ? readable[k] : '.');
		}

		DBGWRITE(level, "%s\n", line.c_str());
	}
}

#endif //__HAIKU_ARCH_X86

void dump_bitmap(uint32 dwBitmap, const dbg_level level)
{
	if (level > current_debug_level)
	{
		return;
	}

	uint32	bit = 0x01;
	int16	x;
	char	buffer[128];
	
	printf("Bitmap dump:\n\n");
	printf("0         10        20        30\n");
	
	printf("01234567890123456789012345678901\n");
	
	memset(buffer, 0, sizeof(buffer));
	
	for(x = 1; x <= 32; x++, bit += bit)
	{
		if (dwBitmap & bit) {
			strcat(buffer, "^");
		}
		else {
			strcat(buffer, " ");
		}
	}

	strcat(buffer, "\n");
	printf(buffer);
	
	bit = 0x01;
	
	for(x = 1; x <= 32; x++, bit += bit)
	{
		printf("%#-10x = %d\t", (int)bit, ((dwBitmap & bit)?1:0));
		if (x % 3 == 0)
		{
			printf("\n");
		}
	}
}

#endif //DEBUG
