// Host check for main/hal/next_wake.h:  cc test/next_wake_test.c -o /tmp/nw && /tmp/nw
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../main/hal/next_wake.h"

static struct tm at(int y, int mo, int d, int h, int mi)
{
    struct tm t = {0};
    t.tm_year = y - 1900, t.tm_mon = mo - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = mi;
    return t;
}

static void expect(struct tm now, int y, int mo, int d)
{
    struct tm r = next_wake_at(now, 4, 0);
    assert(r.tm_year == y - 1900 && r.tm_mon == mo - 1 && r.tm_mday == d);
    assert(r.tm_hour == 4 && r.tm_min == 0 && r.tm_sec == 0);
}

int main(void)
{
    setenv("TZ", "UTC0", 1);
    tzset();
    expect(at(2026, 9, 25, 3, 59), 2026, 9, 25);   // before 04:00: today
    expect(at(2026, 9, 25, 4, 0), 2026, 9, 26);    // the alarm boot itself: tomorrow
    expect(at(2026, 9, 25, 16, 30), 2026, 9, 26);  // afternoon button boot: tomorrow
    expect(at(2026, 9, 30, 23, 10), 2026, 10, 1);  // month rollover
    expect(at(2026, 12, 31, 5, 0), 2027, 1, 1);    // year rollover
    expect(at(2028, 2, 28, 4, 1), 2028, 2, 29);    // leap day
    puts("next_wake_at: ok");
    return 0;
}
