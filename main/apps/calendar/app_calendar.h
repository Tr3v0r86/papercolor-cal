#pragma once

/** Draws the day at `day_offset` from today (RTC local date) and pushes it to the panel once. */
void app_calendar_render(int day_offset);

/** The 04:00 job: join Wi-Fi, set the RTC from SNTP, fetch, then render today. */
void app_calendar_daily_cycle(void);
