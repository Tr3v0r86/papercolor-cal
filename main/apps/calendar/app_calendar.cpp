// The calendar face. Implements esp-devwork boards/papercolor/calendar/face-spec.md literally:
// 400x600 portrait canvas, no dither, uint32_t RGB888 colours, ASCII-folded titles.
#include "app_calendar.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal/hal.h"
#include "hal/wifi/hal_wifi.h"
#include "cal/cal_fetch.h"
#include "cal/cal_model.h"
#include "cal/cal_store.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef GCAL_URL
#define GCAL_URL ""
#endif

using namespace hal_wifi;

static const char* TAG = "Calendar";

// ---- face-spec.md section 4, verbatim ----

// Palette, RGB888. uint32_t on purpose: LovyanGFX reads int32_t as RGB565.
static constexpr uint32_t C_BLACK  = 0x000000u;
static constexpr uint32_t C_WHITE  = 0xFFFFFFu;
static constexpr uint32_t C_YELLOW = 0xFFF338u;
static constexpr uint32_t C_RED    = 0xBF0000u;
static constexpr uint32_t C_BLUE   = 0x6440FFu;
static constexpr uint32_t C_GREEN  = 0x438A1Cu;

// Fonts (M5GFX built-ins). Hero is Font7 at setTextSize(2), top_left datum.
#define F_HERO  (&fonts::Font7)
#define F_BIG   (&fonts::FreeSansBold18pt7b)
#define F_TEXT  (&fonts::FreeSansBold12pt7b)
#define F_SMALL (&fonts::FreeSansBold9pt7b)
static constexpr int HERO_SIZE = 2;

// Canvas and margins
static constexpr int SCR_W = 400, SCR_H = 600;
static constexpr int MARGIN_L = 20, MARGIN_R = 380;

// R1 band
static constexpr int BAND_Y = 0, BAND_H = 172;
static constexpr int HERO_X = 16, HERO_Y = 16;          // ink x 20..137, y 18..109
static constexpr int RCOL_X = 153;
static constexpr int WEEKDAY_BL = 43, MONTH_BL = 80, YEAR_BL = 109;
static constexpr int BANDWORD_X = 20, BANDWORD_BL = 153;

// Rows (shared by R2 and R3)
static constexpr int ROW_H = 32;
static constexpr int ROW_BL = 23;                       // baseline offset from row top
static constexpr int RULE_X = 20, RULE_W = 360, RULE_DY = 30, RULE_H = 2;
static constexpr int TIME_X = 20;
static constexpr int TAB_X = 90, TAB_W = 6, TAB_DY = 6, TAB_H = 18;
static constexpr int ALLDAY_X = 20, ALLDAY_W = 76;      // block spans time column + tab
static constexpr int TITLE_X = 104, TITLE_W = 276;      // x 104..380
static constexpr int MORE_X = 104;

// R2 all-day strip
static constexpr int STRIP_Y = 172, STRIP_H = 64, STRIP_SLOTS = 2;

// R3 timed list
// LIST_H is renamed LIST_H_PX: FreeRTOS list.h uses LIST_H as its include-guard macro.
static constexpr int LIST_Y = 236, LIST_H_PX = 320, LIST_ROWS = 10;
static constexpr int EMPTY_CX = 200, EMPTY_BL = 404;    // NOTHING SCHEDULED / NO CALENDAR DATA

// R4 footer
static constexpr int FOOT_Y = 556, FOOT_H = 44, FOOT_RULE_H = 2;
static constexpr int FOOT_BL = 584;
static constexpr int FOOT_FILL_Y = 565, FOOT_FILL_H = 26, FOOT_FILL_PAD = 6;
static constexpr int COUNT_X = 20, UPD_X = 134, BATT_XR = 380;

// Knobs
static constexpr int BATT_MV_EMPTY = 3300, BATT_MV_FULL = 4200;
static constexpr int BATT_LOW_PCT  = 15;
static constexpr int STALE_SLACK_S = 600;               // stale = generated < last 04:00 - slack
static constexpr int CLOCK_OK_YEAR = 2026;

static_assert(BAND_H + STRIP_H + LIST_H_PX + FOOT_H == SCR_H, "vertical budget");
static_assert(STRIP_SLOTS * ROW_H == STRIP_H && LIST_ROWS * ROW_H == LIST_H_PX, "row budget");
static_assert(STRIP_Y == BAND_Y + BAND_H && LIST_Y == STRIP_Y + STRIP_H && FOOT_Y == LIST_Y + LIST_H_PX, "regions abut");
static_assert(TITLE_X + TITLE_W == MARGIN_R, "title column ends at the margin");

// ---- not in the spec ----
static constexpr int CAL_ROTATION    = 0;          // verify on glass: 0 or 2, whichever puts A,B,C left to right
static constexpr int VENDOR_ROTATION = 3;          // Hal::init()'s; the portal QR path draws in that frame
static constexpr int REFRESH_HOUR    = 4;          // same 04:00 as app_manager's DAILY_WAKE_HOUR
static constexpr size_t FETCH_CAP    = 32 * 1024;  // the proxy answers < 16 KB for a normal fortnight

static const char* const WEEKDAY[7] = {"SUNDAY",   "MONDAY", "TUESDAY", "WEDNESDAY",
                                       "THURSDAY", "FRIDAY", "SATURDAY"};
static const char* const WDAY3[7]   = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char* const MONTH[12]  = {"JANUARY", "FEBRUARY", "MARCH",     "APRIL",   "MAY",      "JUNE",
                                       "JULY",    "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};

// ---- dates: days since 1970-01-01 on the RTC's local (Asia/Bangkok) calendar, no TZ, no mktime ----

static int days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const int era      = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int)doe - 719468;
}

static void civil_from_days(int z, int* y, int* m, int* d)
{
    z += 719468;
    const int era      = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp  = (5 * doy + 2) / 153;
    *d                 = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m                 = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y                 = (int)yoe + era * 400 + (*m <= 2);
}

static int weekday_of(int days)  // 0 = Sunday; 1970-01-01 was a Thursday
{
    return ((days % 7) + 11) % 7;
}

struct Now {
    bool believed;  // RTC read and year >= CLOCK_OK_YEAR
    int days;       // local date
    int secs;       // local seconds into the day
};

static Now rtc_now()
{
    Now n{};
    m5::rtc_datetime_t t;
    if (!M5.Rtc.getDateTime(&t)) return n;
    n.days     = days_from_civil(t.date.year, t.date.month, t.date.date);
    n.secs     = t.time.hours * 3600 + t.time.minutes * 60 + t.time.seconds;
    n.believed = t.date.year >= CLOCK_OK_YEAR;
    return n;
}

static void date_string(int days, char out[11])
{
    int y, m, d;
    civil_from_days(days, &y, &m, &d);
    snprintf(out, 11, "%04d-%02d-%02d", y, m, d);
}

// "2026-09-25T04:00:12+07:00": the stamp already carries +07:00, so its local fields are read as is.
static bool parse_generated(const char* s, int* days, int* secs)
{
    int y, mo, d, h, mi, se;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &se) != 6) return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59) return false;
    *days = days_from_civil(y, mo, d);
    *secs = h * 3600 + mi * 60 + se;
    return true;
}

// ---- text ----

// The fonts cover 0x20..0x7E only (face-spec section 1 item 6).
static void ascii_fold(const char* s, char* out, size_t cap)
{
    size_t o = 0;
    auto put = [&](const char* t) {
        while (*t && o + 1 < cap) out[o++] = *t++;
    };
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        uint32_t cp;
        int n;
        if (*p < 0x80)                cp = *p, n = 1;
        else if ((*p & 0xE0) == 0xC0) cp = *p & 0x1F, n = 2;
        else if ((*p & 0xF0) == 0xE0) cp = *p & 0x0F, n = 3;
        else if ((*p & 0xF8) == 0xF0) cp = *p & 0x07, n = 4;
        else                          cp = 0xFFFD, n = 1;  // stray continuation or invalid lead
        for (int i = 1; i < n; i++) {
            if ((p[i] & 0xC0) != 0x80) {  // also stops at the NUL of a cut sequence
                cp = 0xFFFD, n = 1;
                break;
            }
            cp = (cp << 6) | (p[i] & 0x3F);
        }
        p += n;
        switch (cp) {
            case 0x2013: case 0x2014: put("-"); break;
            case 0x2018: case 0x2019: put("'"); break;
            case 0x201C: case 0x201D: put("\""); break;
            case 0x2026: put("..."); break;
            case 0x00A0: put(" "); break;
            default: {
                const char c[2] = {(cp >= 0x20 && cp <= 0x7E) ? (char)cp : '?', 0};
                put(c);
            }
        }
    }
    out[o] = '\0';
}

static uint32_t tab_rgb(uint8_t pal)
{
    switch (pal) {
        case CAL_PAL_YELLOW: return C_YELLOW;
        case CAL_PAL_RED:    return C_RED;
        case CAL_PAL_BLUE:   return C_BLUE;
        case CAL_PAL_GREEN:  return C_GREEN;
        default:             return C_BLACK;  // BLACK, and WHITE drawn as BLACK: a white tab vanishes on white
    }
}

// Rows share one state: F_SMALL/F_TEXT, BLACK ink, baseline_left.
static void draw_title(M5Canvas* c, const char* raw, int y)
{
    char t[CAL_TITLE_LEN + 4];
    ascii_fold(raw, t, sizeof t);
    c->setFont(F_SMALL);
    if (c->textWidth(t) > TITLE_W) {
        char p[sizeof t + 3] = "...";
        for (size_t n = strlen(t); n-- > 0;) {  // longest prefix first; at most 60 textWidth calls
            size_t k = n;
            while (k > 0 && t[k - 1] == ' ') k--;
            memcpy(p, t, k);
            strcpy(p + k, "...");
            if (c->textWidth(p) <= TITLE_W) break;
        }
        strcpy(t, p);
    }
    c->drawString(t, TITLE_X, y + ROW_BL);
}

static void draw_rule(M5Canvas* c, int y)
{
    c->fillRect(RULE_X, y + RULE_DY, RULE_W, RULE_H, C_BLACK);
}

static void draw_more(M5Canvas* c, int y, int n)
{
    char s[16];
    snprintf(s, sizeof s, "+%d MORE", n);
    c->setFont(F_SMALL);
    c->drawString(s, MORE_X, y + ROW_BL);
}

static void draw_allday(M5Canvas* c, int y, const cal_event_t* e)
{
    c->fillRect(ALLDAY_X, y + TAB_DY, ALLDAY_W, TAB_H, tab_rgb(e->pal));
    draw_title(c, e->title, y);
}

static void draw_timed(M5Canvas* c, int y, const cal_event_t* e)
{
    char hm[6];
    cal_format_time(e->sh, e->sm, hm);
    c->setFont(F_TEXT);
    c->drawString(hm, TIME_X, y + ROW_BL);
    c->fillRect(TAB_X, y + TAB_DY, TAB_W, TAB_H, tab_rgb(e->pal));
    draw_title(c, e->title, y);
}

// ---- canvas + push ----

static M5Canvas* canvas()
{
    static M5Canvas* s = nullptr;
    if (!s) {
        s = new M5Canvas(&M5.Display);  // PSRAM (M5Canvas with a parent), 480 KB at RGB565
        if (!s->createSprite(SCR_W, SCR_H)) {
            ESP_LOGE(TAG, "no memory for the %dx%d canvas", SCR_W, SCR_H);
            delete s;
            s = nullptr;
            return nullptr;
        }
    }
    assert(s->width() == SCR_W && s->height() == SCR_H);
    return s;
}

static void push(M5Canvas* c)
{
    M5.Display.setRotation(CAL_ROTATION);             // 400x600 portrait, the panel's native frame
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);   // no dither: plain nearest-of-six
    hal.statusEventSend(OPERATION_EVENT_REFRESH_START);
    c->pushSprite(0, 0);                              // blocks through the Spectra refresh
    hal.statusEventSend(OPERATION_EVENT_REFRESH_COMPLETE);
    M5.Display.setRotation(VENDOR_ROTATION);          // leave the vendor frame as found for the portal
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
}

// ---- the face ----

static void band_word(int offset, char* s, size_t n)
{
    if (offset == 0)       snprintf(s, n, "TODAY");
    else if (offset == 1)  snprintf(s, n, "TOMORROW");
    else if (offset == -1) snprintf(s, n, "YESTERDAY");
    else if (offset > 1)   snprintf(s, n, "IN %d DAYS", offset);
    else                   snprintf(s, n, "%d DAYS AGO", -offset);
}

bool app_calendar_day_stored(int day_offset)
{
    static cal_day_t day;  // 2.3 KB, off the task stack
    Now n = rtc_now();
    if (!n.believed) return false;
    char date[11];
    date_string(n.days + day_offset, date);
    return cal_store_load_day(date, &day) == ESP_OK;
}

void app_calendar_render(int day_offset)
{
    M5Canvas* c = canvas();
    if (!c) return;

    static cal_day_t day;
    static cal_meta_t meta;
    const Now now = rtc_now();
    const int days = now.days + day_offset;
    char date[11] = "";
    bool day_ok = false;
    if (now.believed) {
        date_string(days, date);
        day_ok = cal_store_load_day(date, &day) == ESP_OK;
    }
    const bool meta_ok = cal_store_load_meta(&meta) == ESP_OK;
    int gen_days = 0, gen_secs = 0;
    const bool gen_ok = meta_ok && parse_generated(meta.generated, &gen_days, &gen_secs);

    // Stale = missed the most recent 04:00 refresh (face-spec R4), or nothing trustworthy to compare.
    int64_t last_refresh = (int64_t)now.days * 86400 + REFRESH_HOUR * 3600;
    if (now.secs < REFRESH_HOUR * 3600) last_refresh -= 86400;
    const bool stale = !now.believed || !day_ok || !gen_ok ||
                       (int64_t)gen_days * 86400 + gen_secs < last_refresh - STALE_SLACK_S;

    ESP_LOGI(TAG, "render %s offset %d: clock %s, day %s (%u events), window %s..%s, generated %s, %s",
             date[0] ? date : "-", day_offset, now.believed ? "ok" : "NOT SET", day_ok ? "stored" : "absent",
             day_ok ? (unsigned)day.n : 0u, meta_ok ? meta.first : "-", meta_ok ? meta.last : "-",
             meta_ok ? meta.generated : "never", stale ? "STALE" : "fresh");

    c->fillScreen(C_WHITE);
    char s[40];

    // R1 band. Clock not believed: blue, date hidden, CLOCK NOT SET.
    const bool blue  = !now.believed || day_offset == 0;
    const uint32_t ink = blue ? C_WHITE : C_BLACK;
    c->fillRect(0, BAND_Y, SCR_W, BAND_H, blue ? C_BLUE : C_GREEN);
    c->setTextColor(ink);
    if (!now.believed) {
        c->setTextDatum(textdatum_t::baseline_left);
        c->setFont(F_BIG);
        c->drawString("CLOCK NOT SET", BANDWORD_X, BANDWORD_BL);
    } else {
        int y, m, d;
        civil_from_days(days, &y, &m, &d);
        c->setTextDatum(textdatum_t::top_left);
        c->setFont(F_HERO);
        c->setTextSize(HERO_SIZE);
        snprintf(s, sizeof s, "%02d", d);
        c->drawString(s, HERO_X, HERO_Y);
        c->setTextSize(1);

        c->setTextDatum(textdatum_t::baseline_left);
        c->setFont(F_BIG);
        c->drawString(WEEKDAY[weekday_of(days)], RCOL_X, WEEKDAY_BL);
        c->drawString(MONTH[m - 1], RCOL_X, MONTH_BL);
        c->setFont(F_TEXT);
        snprintf(s, sizeof s, "%d", y);
        c->drawString(s, RCOL_X, YEAR_BL);
        c->setFont(F_BIG);
        band_word(day_offset, s, sizeof s);
        c->drawString(s, BANDWORD_X, BANDWORD_BL);
    }

    // R2 strip + R3 list, black on white.
    c->setTextColor(C_BLACK);
    c->setTextDatum(textdatum_t::baseline_left);
    if (!day_ok || day.n == 0) {
        c->setTextDatum(textdatum_t::baseline_center);
        c->setFont(F_TEXT);
        c->drawString(day_ok ? "NOTHING SCHEDULED" : "NO CALENDAR DATA", EMPTY_CX, EMPTY_BL);
        c->setTextDatum(textdatum_t::baseline_left);
    } else {
        const cal_event_t* all[CAL_MAX_EVENTS];
        const cal_event_t* tim[CAL_MAX_EVENTS];
        int na = 0, nt = 0;
        for (int i = 0; i < day.n; i++) {
            if (day.ev[i].allday) all[na++] = &day.ev[i];
            else                  tim[nt++] = &day.ev[i];
        }

        for (int i = 0; i < na && i < STRIP_SLOTS; i++) {
            const int y = STRIP_Y + i * ROW_H;
            if (na > STRIP_SLOTS && i == STRIP_SLOTS - 1) draw_more(c, y, na - (STRIP_SLOTS - 1));
            else draw_allday(c, y, all[i]);
            draw_rule(c, y);
        }

        for (int i = 0; i < nt && i < LIST_ROWS; i++) {
            const int y = LIST_Y + i * ROW_H;
            if (nt > LIST_ROWS && i == LIST_ROWS - 1) draw_more(c, y, nt - (LIST_ROWS - 1));
            else draw_timed(c, y, tim[i]);
            if (i != LIST_ROWS - 1) draw_rule(c, y);  // row 9's foot sits on the footer rule
        }
    }

    // R4 footer.
    c->fillRect(0, FOOT_Y, SCR_W, FOOT_RULE_H, C_BLACK);
    c->setFont(F_SMALL);
    if (day_ok) {
        snprintf(s, sizeof s, day.n == 1 ? "%d EVENT" : "%d EVENTS", day.n);
        c->drawString(s, COUNT_X, FOOT_BL);
    }

    if (gen_ok) {
        snprintf(s, sizeof s, "UPDATED %s %02d:%02d", WDAY3[weekday_of(gen_days)], gen_secs / 3600,
                 gen_secs / 60 % 60);
    } else {
        snprintf(s, sizeof s, "NEVER UPDATED");
    }
    if (stale) {
        c->fillRect(UPD_X - FOOT_FILL_PAD, FOOT_FILL_Y, c->textWidth(s) + 2 * FOOT_FILL_PAD, FOOT_FILL_H, C_YELLOW);
    }
    c->drawString(s, UPD_X, FOOT_BL);

    uint16_t mv = 0;
    if (hal.pm1.readVbat(&mv) == M5PM1_OK) {  // ponytail: a failed read leaves the cell white, no dash
        int pct = ((int)mv - BATT_MV_EMPTY) * 100 / (BATT_MV_FULL - BATT_MV_EMPTY);
        pct     = pct < 0 ? 0 : pct > 100 ? 100 : pct;
        snprintf(s, sizeof s, "%d%%", pct);
        const int tw = c->textWidth(s);
        if (pct <= BATT_LOW_PCT) {
            c->fillRect(BATT_XR - tw - FOOT_FILL_PAD, FOOT_FILL_Y, tw + 2 * FOOT_FILL_PAD, FOOT_FILL_H, C_RED);
            c->setTextColor(C_WHITE);
        }
        c->setTextDatum(textdatum_t::baseline_right);
        c->drawString(s, BATT_XR, FOOT_BL);
        ESP_LOGI(TAG, "battery %u mV, %d%%", mv, pct);
    }

    push(c);
}

// ---- daily cycle ----

static void fetch_into_store()
{
    const char* url = GCAL_URL;
    if (!url[0] || strstr(url, "CHANGE_ME")) {
        static bool said = false;
        if (!said) ESP_LOGW(TAG, "no GCAL_URL in main/secrets.h, fetch skipped");
        said = true;
        return;
    }
    char* buf       = (char*)heap_caps_malloc(FETCH_CAP, MALLOC_CAP_SPIRAM);
    cal_window_t* w = (cal_window_t*)heap_caps_malloc(sizeof(cal_window_t), MALLOC_CAP_SPIRAM);
    size_t len      = 0;
    esp_err_t err;
    if (!buf || !w) {
        ESP_LOGE(TAG, "fetch: no PSRAM for the buffers");
    } else if ((err = cal_fetch(url, buf, FETCH_CAP, &len)) != ESP_OK) {
        ESP_LOGW(TAG, "fetch failed: %s", esp_err_to_name(err));
    } else if (!cal_model_parse(buf, len, w)) {
        ESP_LOGW(TAG, "parse failed: %u bytes are not the window contract", (unsigned)len);
    } else if ((err = cal_store_save(w)) != ESP_OK) {
        ESP_LOGW(TAG, "store refused or failed: %s (%u days)", esp_err_to_name(err), (unsigned)w->n_days);
    }
    heap_caps_free(buf);
    heap_caps_free(w);
}

void app_calendar_daily_cycle(void)
{
    // Budget: Wi-Fi 15 s + SNTP 10 s + fetch (20 s per network operation) keeps a normal run under 60 s.
    const int64_t t0 = esp_timer_get_time();
    bool online      = hal.settings.wifi_ssid[0] &&
                  (WiFi.isConnected() ||
                   WiFi.connect(hal.settings.wifi_ssid, hal.settings.wifi_password, 15000) == ESP_OK);
    if (online) {
        hal.syncRtcFromSntp(10000);
        fetch_into_store();
    } else {
        ESP_LOGW(TAG, "wifi: %s, rendering from what is stored",
                 hal.settings.wifi_ssid[0] ? "join failed" : "no SSID configured");
    }
    ESP_LOGI(TAG, "network steps took %lld ms", (long long)((esp_timer_get_time() - t0) / 1000));
    app_calendar_render(0);
}
