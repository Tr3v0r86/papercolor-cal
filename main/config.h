// config.h - the knobs a user changes. Everything else is behaviour.
#pragma once

// Your local time as a fixed UTC offset in seconds. The RX8130CE keeps wall time in this zone and
// the daily alarm is set in it. No DST handling: if your zone observes DST, the 04:00 wake will
// drift by an hour for half the year, which is harmless for a once-a-day calendar.
// Must match TZ in tools/gcal-proxy/Code.gs.
#ifndef CAL_UTC_OFFSET_S
#define CAL_UTC_OFFSET_S (7 * 3600)   // Asia/Bangkok
#endif

// When the board wakes itself, fetches and redraws. Local time.
#ifndef CAL_WAKE_HOUR
#define CAL_WAKE_HOUR   4
#endif
#ifndef CAL_WAKE_MINUTE
#define CAL_WAKE_MINUTE 0
#endif
