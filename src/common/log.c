/**********************************************************************************************
*
LOG MODULE - per-call-site token-bucket rate limiting and single-line formatting
*
**********************************************************************************************/
#include "common/log.h"
#include <time.h>

typedef struct LogLevelInfo
{
    const char *colour;
    const char *tag;
    bool show_location; // Whether "(file:line)" is printed.
    bool enabled;
    int burst;
    float rate_per_sec;
} LogLevelInfo;

// Global defaults per level (adjustable via Log_SetLevelLimits).
static LogLevelInfo level_info[LOG_LEVEL_COUNT] = {
    [LOG_LEVEL_ERROR] = { LOG_CLR_RED,    "[ERROR]", true,  true, 20, 5.0f },
    [LOG_LEVEL_WARN]  = { LOG_CLR_YELLOW, "[WARN]",  true,  true, 10, 2.0f },
    [LOG_LEVEL_INFO]  = { LOG_CLR_GREEN,  "[INFO]",  false, true, 5,  1.0f },
};

// Set a level's global defaults (see log.h).
void Log_SetLevelLimits(LogLevel level, bool enabled, int burst, float rate_per_sec)
{
    if ((unsigned)level >= LOG_LEVEL_COUNT)
        return;
    level_info[level].enabled = enabled;
    level_info[level].burst = burst < 0 ? 0 : burst;
    level_info[level].rate_per_sec = rate_per_sec < 0.0f ? 0.0f : rate_per_sec;
}

// Token bucket: a site starts with `burst` tokens, each message spends one, and tokens refill at
// rate_per_sec up to `burst`. With no token the call is dropped and counted; the next message that
// does log reports the count (a final run of dropped calls is never reported if the site never fires
// again; accepted, to avoid a global registry). The decision is pure: time is passed in.
bool Log_LimiterAllow(LogSite *site, int burst, float rate_per_sec, double now,
                      unsigned *suppressed_out, double *window_out)
{
    *suppressed_out = 0;
    *window_out = 0.0;
    if (burst <= 0)
        return false;

    if (!site->started)
    {
        site->started = true;
        site->tokens = burst;
    }
    else
    {
        double elapsed = now - site->last_time;
        if (elapsed < 0.0)
            elapsed = 0.0; // Clock went backwards: no refill.
        site->tokens += elapsed * rate_per_sec;
        if (site->tokens > burst)
            site->tokens = burst;
    }
    site->last_time = now;

    if (site->tokens < 1.0)
    {
        if (site->suppressed == 0)
            site->suppressed_since = now;
        site->suppressed++;
        return false;
    }

    site->tokens -= 1.0;
    if (site->suppressed > 0)
    {
        *suppressed_out = site->suppressed;
        double window = now - site->suppressed_since;
        *window_out = window > 0.0 ? window : 0.0;
        site->suppressed = 0;
    }
    return true;
}

// Current wall-clock time in seconds from the C11 standard clock. Works before InitWindow and on every
// platform (clock() is CPU time on POSIX; raylib's GetTime needs a window).
static double LogNow(void)
{
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == 0)
        return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

size_t LogFormatLine(char *buf, size_t size, const char *colour, const char *tag, const char *file, int line,
                     const char *note, const char *fmt, va_list args)
{
    static const char reset[] = LOG_CLR_RESET;
    // Space held back for the tail: note, reset code, '\n' and the terminating NUL.
    const size_t note_len = note ? strlen(note) : 0;
    const size_t tail = note_len + sizeof(reset) - 1 + 2;
    if (!buf || size <= tail)
        return 0;

    // Prefix (colour, tag and optional source location). snprintf reports the untruncated length, so clamp it.
    int n = file ? snprintf(buf, size - tail, "%s%s (%s:%d) ", colour, tag, file, line)
                 : snprintf(buf, size - tail, "%s%s ", colour, tag);
    size_t pos = n < 0 ? 0 : (size_t)n;
    if (pos > size - tail - 1)
        pos = size - tail - 1;

    // The caller's message, formatted into whatever room is left before the tail.
    n = vsnprintf(buf + pos, size - tail - pos, fmt, args);
    if (n > 0)
    {
        size_t room = size - tail - pos - 1; // Characters that fit, excluding the NUL.
        size_t len = (size_t)n < room ? (size_t)n : room;
        pos += len;
        // Drop one trailing newline so the single appended '\n' is the only one.
        if (buf[pos - 1] == '\n')
            pos--;
    }

    // Tail: optional note, colour reset, then the one newline.
    if (note_len)
    {
        memcpy(buf + pos, note, note_len);
        pos += note_len;
    }
    memcpy(buf + pos, reset, sizeof(reset) - 1);
    pos += sizeof(reset) - 1;
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

// Rate-limit then write one line (see log.h).
void Log_Write(LogSite *site, LogLevel level, int burst, float rate_per_sec, const char *file, int line,
               const char *fmt, ...)
{
    const LogLevelInfo *info = &level_info[level];
    if (!info->enabled || info->burst <= 0)
        return;
    if (burst < 0)
    {
        burst = info->burst;
        rate_per_sec = info->rate_per_sec;
    }

    unsigned suppressed;
    double window;
    if (!Log_LimiterAllow(site, burst, rate_per_sec, LogNow(), &suppressed, &window))
        return;

    char note[64] = "";
    if (suppressed)
        snprintf(note, sizeof(note), " (suppressed %u similar in the last %.1f s)", suppressed, window);

    char buf[LOG_LINE_CAPACITY];
    va_list args;
    va_start(args, fmt);
    LogFormatLine(buf, sizeof(buf), info->colour, info->tag, info->show_location ? file : NULL, line, note, fmt, args);
    va_end(args);
    fputs(buf, level == LOG_LEVEL_INFO ? stdout : stderr);
}
