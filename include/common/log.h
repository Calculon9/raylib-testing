/**********************************************************************************************
*
LOG MODULE - rate-limited, single-line logging behind the LOG_* macros (implementation in log.c)
*
**********************************************************************************************/
#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// ANSI Escape Codes for Colors (Works beautifully in modern terminals/cmd)
#define LOG_CLR_RESET   "\033[0m"
#define LOG_CLR_RED     "\033[1;31m"
#define LOG_CLR_YELLOW  "\033[1;33m"
#define LOG_CLR_GREEN   "\033[1;32m"

// Bounded size of one formatted log line (prefix + message + note + reset + newline). Longer messages are truncated.
#define LOG_LINE_CAPACITY 1024

// Let GCC/Clang check the caller's format string against its arguments, as direct printf calls would be.
#if defined(__GNUC__)
    #define LOG_PRINTF_CHECK(fmt_index, first_arg) __attribute__((format(printf, fmt_index, first_arg)))
#else
    #define LOG_PRINTF_CHECK(fmt_index, first_arg)
#endif

typedef enum LogLevel
{
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_COUNT
} LogLevel;

// Per-call-site token bucket. Each LOG_* expansion owns one (zero-initialised static); see Log_LimiterAllow.
// Plain statics: the job system is single-threaded today, so parallel jobs would need atomics here.
typedef struct LogSite
{
    double tokens;           // Whole tokens available (one per message).
    double last_time;        // Time of the previous check, in seconds.
    double suppressed_since; // Time of the first call dropped since the site last logged.
    unsigned suppressed;     // Calls dropped since the site last logged.
    bool started;            // False until the first call fills the bucket.
} LogSite;

// Burst value meaning "use the level's global default".
#define LOG_USE_DEFAULT (-1)

// Set a level's global defaults: enabled flag, burst (messages allowed at once) and refill rate (messages per
// second). A burst of 0 or enabled=false turns the level off; a rate of 0 allows only the burst, then silence.
void Log_SetLevelLimits(LogLevel level, bool enabled, int burst, float rate_per_sec);

// Pure rate-limit decision for one site at time `now` (seconds). Returns true if the call may log. On true,
// *suppressed_out / *window_out report the calls dropped since the site last logged and the seconds since the
// first of them (both 0 if none). A clock that goes backwards counts as zero elapsed time.
bool Log_LimiterAllow(LogSite *site, int burst, float rate_per_sec, double now,
                      unsigned *suppressed_out, double *window_out);

// Build one complete log line into buf: colour + tag + optional "(file:line) " + message + optional note + reset
// + exactly one '\n'. The message's own trailing '\n' (if any) is dropped first. Output is truncated (never
// overflowed) when it does not fit. Returns the line length.
size_t LogFormatLine(char *buf, size_t size, const char *colour, const char *tag, const char *file, int line,
                     const char *note, const char *fmt, va_list args);

// Rate-limit then write one line with a single fputs. burst < 0 uses the level defaults. The limiter runs
// before any formatting, so a suppressed call costs a clock read and a few compares.
void Log_Write(LogSite *site, LogLevel level, int burst, float rate_per_sec, const char *file, int line,
               const char *fmt, ...) LOG_PRINTF_CHECK(7, 8);

// Generic emitter: the static gives this call site its own limiter state.
#define LOG_EMIT_(level, burst, rate, fmt, ...) \
    do { \
        static LogSite log_site_; \
        Log_Write(&log_site_, (level), (burst), (rate), __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } while (0)

#endif
