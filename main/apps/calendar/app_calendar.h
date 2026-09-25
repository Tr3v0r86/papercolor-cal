#pragma once

/** Draws the day at `day_offset` from today (RTC local date) and pushes it to the panel once. */
void app_calendar_render(int day_offset);

/** True if the stored window has the day at `day_offset`. Buttons refuse offsets that do not. */
bool app_calendar_day_stored(int day_offset);

/** The 04:00 job: join Wi-Fi, set the RTC from SNTP, fetch into NVS, then render `day_offset`
 *  (today at 04:00; the day being looked at on a button refetch). Falls back to today if the
 *  refreshed window no longer holds that day. */
void app_calendar_daily_cycle(int day_offset);
