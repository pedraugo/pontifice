#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define LOG_PRINTF_CHECK __attribute__((format(printf, 2, 3)))
#else
#define LOG_PRINTF_CHECK
#endif

// Game log line: [YYYY-MM-DD HH:MM:SS] [filename] - message
#define LOG(...) ::logging::Write(__FILE__, __VA_ARGS__)

namespace logging {

void Write(const char* file, const char* format, ...) LOG_PRINTF_CHECK;

}  // namespace logging
