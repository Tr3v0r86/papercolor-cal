// cal_model.h - the calendar window the Apps Script proxy returns, as fixed-size C.
//
// Plain C99, no IDF headers, so the parser runs and is tested on the host (make test).
// Contract: docs/superpowers/specs/2026-09-25-papercolor-calendar-design.md section 2.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAL_MAX_DAYS    17  // back 3 + days 14, the default window
#define CAL_MAX_EVENTS  24  // the proxy caps at 24 too
#define CAL_TITLE_LEN   61  // 60 bytes + NUL (the proxy cuts at 60 chars; multibyte gets cut again here)
#define CAL_CALNAME_LEN 25

// Spectra 6 panel indices, same values as boards/papercolor/main/spectra6.h.
#define CAL_PAL_BLACK  0x0
#define CAL_PAL_WHITE  0x1
#define CAL_PAL_YELLOW 0x2
#define CAL_PAL_RED    0x3
#define CAL_PAL_BLUE   0x5
#define CAL_PAL_GREEN  0x6

typedef struct {
    bool    allday;
    uint8_t sh, sm, eh, em;   // timed only; eh may be 24 (event runs past midnight)
    char    title[CAL_TITLE_LEN];
    char    cal[CAL_CALNAME_LEN];
    uint8_t rgb[3];           // from "#rrggbb"; black when absent or malformed
    uint8_t pal;              // cal_pal_nearest(rgb), a CAL_PAL_* index
} cal_event_t;                // 95 bytes

typedef struct {
    char        date[11];     // "YYYY-MM-DD"
    uint8_t     n;
    cal_event_t ev[CAL_MAX_EVENTS];
} cal_day_t;                  // 2292 bytes: one NVS blob per day, under the 2.5 KB budget

typedef struct {
    char      generated[26];  // "2026-09-25T04:00:12+07:00"
    char      tz[24];
    uint8_t   n_days;
    cal_day_t day[CAL_MAX_DAYS];
} cal_window_t;               // 39015 bytes: too big for a task stack, lives in PSRAM

// Parse the proxy's JSON into *out (zeroed first). Tolerant: missing optional fields default,
// events past CAL_MAX_EVENTS and days past CAL_MAX_DAYS are dropped, a malformed time makes
// the event all-day, a day without a valid date is skipped. False only if the document is not
// the contract (not an object, or no "days_list" array). A true result can still hold zero
// days; the caller decides whether that is worth storing.
bool cal_model_parse(const char *json, size_t len, cal_window_t *out);

const cal_day_t *cal_window_find(const cal_window_t *w, const char *yyyy_mm_dd);
int cal_window_index_of(const cal_window_t *w, const char *date);   // -1 if absent

void cal_format_time(uint8_t h, uint8_t m, char out[6]);             // "09:05"

uint8_t cal_pal_nearest(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif
