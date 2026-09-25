// Host test for cal_model. Run: make test
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cal_model.h"

static char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n);
    size_t got = buf ? fread(buf, 1, (size_t)n, f) : 0;
    assert(got == (size_t)n);
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static cal_window_t w;   // 39 KB, off the stack like on the device

int main(int argc, char **argv)
{
    size_t len;
    char *json = slurp(argc > 1 ? argv[1] : "sample.json", &len);
    assert(cal_model_parse(json, len, &w));
    free(json);

    assert(strcmp(w.tz, "Asia/Bangkok") == 0);
    assert(strcmp(w.generated, "2026-09-25T04:00:12+07:00") == 0);
    assert(w.n_days == 14);                                  // 15th entry has date "bad", skipped
    assert(strcmp(w.day[0].date, "2026-09-22") == 0 && w.day[0].n == 0);   // empty events
    assert(w.day[1].n == CAL_MAX_EVENTS);                    // 30 in, capped at 24
    assert(strcmp(w.day[1].ev[23].title, "Slot 24") == 0);
    assert(w.day[2].n == 0);                                 // "events" key missing

    const cal_day_t *t = cal_window_find(&w, "2026-09-25");
    assert(t && t->n == 9);
    const cal_event_t *e = t->ev;
    assert(e[0].allday && strcmp(e[0].title, "Holiday") == 0 && e[0].cal[0] == '\0');
    assert(e[0].rgb[0] == 0 && e[0].pal == CAL_PAL_BLACK);   // no colour: black
    assert(e[1].allday && e[1].pal == CAL_PAL_GREEN);
    assert(!e[2].allday && e[2].sh == 9 && e[2].sm == 0 && e[2].eh == 10 && e[2].em == 30);
    assert(e[2].rgb[0] == 0x79 && e[2].rgb[1] == 0x86 && e[2].rgb[2] == 0xcb);
    assert(strlen(e[3].title) == 60 && strlen(e[3].cal) == 24);   // 70 -> 60, 39 -> 24
    assert(e[4].allday && strcmp(e[4].title, "Bad start") == 0 && e[4].pal == CAL_PAL_RED);
    assert(!e[5].allday && e[5].eh == 13 && e[5].em == 0 && e[5].pal == CAL_PAL_BLACK);
    assert(!e[6].allday && e[6].eh == 24 && e[6].em == 0);   // runs to midnight
    assert(e[7].allday);                                     // end "25:00" malformed
    assert(strlen(e[8].title) == 59);                        // 59 + 2-byte e-acute: cut whole

    // find / index_of
    assert(cal_window_index_of(&w, "2026-09-22") == 0);
    assert(cal_window_index_of(&w, "2026-10-05") == 13);
    assert(cal_window_index_of(&w, "2026-10-06") == -1);
    assert(cal_window_index_of(&w, "bad") == -1);
    assert(cal_window_find(&w, "2026-09-21") == NULL);

    // not the contract
    static cal_window_t bad;
    assert(!cal_model_parse("{\"ok\":true}", 11, &bad));
    assert(!cal_model_parse("[]", 2, &bad));
    assert(!cal_model_parse("<html>", 6, &bad));
    assert(!cal_model_parse("{\"days_list\":{}}", 16, &bad));
    assert(cal_model_parse("{\"days_list\":[]}", 16, &bad) && bad.n_days == 0);

    // time formatting
    char s[6];
    cal_format_time(9, 5, s);   assert(strcmp(s, "09:05") == 0);
    cal_format_time(0, 0, s);   assert(strcmp(s, "00:00") == 0);
    cal_format_time(23, 59, s); assert(strcmp(s, "23:59") == 0);
    cal_format_time(24, 0, s);  assert(strcmp(s, "24:00") == 0);

    // palette: the six primaries map to themselves
    assert(cal_pal_nearest(0, 0, 0) == CAL_PAL_BLACK);
    assert(cal_pal_nearest(255, 255, 255) == CAL_PAL_WHITE);
    assert(cal_pal_nearest(255, 243, 56) == CAL_PAL_YELLOW);
    assert(cal_pal_nearest(191, 0, 0) == CAL_PAL_RED);
    assert(cal_pal_nearest(100, 64, 255) == CAL_PAL_BLUE);
    assert(cal_pal_nearest(67, 138, 28) == CAL_PAL_GREEN);
    // Google event colours
    assert(cal_pal_nearest(0x79, 0x86, 0xcb) == CAL_PAL_BLUE);    // lavender, the default blue
    assert(cal_pal_nearest(0xd5, 0x00, 0x00) == CAL_PAL_RED);     // tomato
    assert(cal_pal_nearest(0x33, 0xb6, 0x79) == CAL_PAL_GREEN);   // sage

    printf("sizeof cal_event_t=%zu cal_day_t=%zu cal_window_t=%zu\n",
           sizeof(cal_event_t), sizeof(cal_day_t), sizeof(cal_window_t));
    assert(sizeof(cal_day_t) < 2560);
    printf("#7986cb->%u #d50000->%u #33b679->%u\n", cal_pal_nearest(0x79, 0x86, 0xcb),
           cal_pal_nearest(0xd5, 0, 0), cal_pal_nearest(0x33, 0xb6, 0x79));
    puts("test_cal_model: all passed");
    return 0;
}
