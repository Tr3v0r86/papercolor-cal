// Next occurrence of hour:minute strictly after `now`, as a normalised struct tm.
// Plain C with only <time.h> so test/next_wake_test.c runs it on the host.
// ponytail: mktime normalises in the process TZ; the device never sets TZ (UTC, no DST) and
// Asia/Bangkok has no DST either, so there is no DST gap to fall into.
#pragma once
#include <time.h>

static inline struct tm next_wake_at(struct tm now, int hour, int minute)
{
    struct tm t = now;
    t.tm_hour   = hour;
    t.tm_min    = minute;
    t.tm_sec    = 0;
    t.tm_isdst  = 0;
    if (hour * 60 + minute <= now.tm_hour * 60 + now.tm_min) t.tm_mday += 1;
    mktime(&t);
    return t;
}
