# PaperColor Calendar

A once-a-day Google Calendar for your fridge, on the M5Stack PaperColor (4" six-colour E Ink,
ESP32-S3, 1250 mAh battery). It wakes itself at 04:00, fetches a two-week window of your
calendars, draws today, and powers off completely. Three buttons walk the days. A 3D-printed
magnetic cradle holds it on the door.

- **No OAuth on the device.** A ten-line Google Apps Script on your own account serves a compact
  JSON window; the board does one HTTPS GET.
- **Real battery life.** The board is not sleeping, it is off: the M5PM1 PMIC cuts the ESP32-S3
  and the RX8130 RTC alarm powers it back on. Standby is the PMIC's own ~90 µA.
- **Six colours, no dithering.** The face uses the panel's six exact primaries as flat fills and
  1-bit fonts, so type stays crisp on Spectra 6.

Built on M5Stack's own [M5PaperColor-UserDemo](https://github.com/m5stack/M5PaperColor-UserDemo)
(ESP-IDF, M5GFX, M5Unified, M5PM1), keeping its HAL, power scheme and Wi-Fi setup portal, and
replacing the photo apps with one calendar app.

## What it shows

400 × 600 portrait. A full-bleed band with the date as the hero (blue for today, green when you
have walked to another day, with the word TODAY / TOMORROW / IN 3 DAYS), an all-day strip, a list
of timed events with a colour tab per calendar, and a black-ink footer: battery, event count, and
`UPDATED SAT 04:00`. The footer cell turns yellow when the last 04:00 refresh was missed, so a
stale page is never mistaken for a fresh one.

## Buttons

| Button | Click | Hold |
|---|---|---|
| A (upper side) | day back | 5 s: Wi-Fi setup portal (QR code on the glass) |
| B (lower side) | day forward | |
| C (top) | refetch the calendar now, stay on this day | |
| Power | on (or restart while on); double-click off | |

Every power-on and the 04:00 wake show today. The LED is dim white while the board is busy
(booting, fetching, refreshing) and off otherwise. The board powers off 60 s after the last press.

## Setup

You need: a PaperColor, a Google account, ESP-IDF **v5.5.1** (the vendor's pin), and 2.4 GHz
Wi-Fi.

1. **Deploy the calendar proxy.** Follow [tools/gcal-proxy/README.md](tools/gcal-proxy/README.md).
   Five minutes in the Apps Script editor; you end up with a `/exec` URL.
2. **Configure.** `cp main/secrets.h.example main/secrets.h` and paste the URL as `GCAL_URL`.
   If you are not in UTC+7, set `CAL_UTC_OFFSET_S` in `main/config.h` and `TZ` in `Code.gs`.
   `CAL_WAKE_HOUR` is the daily refresh hour.
3. **Build and flash.**
   ```bash
   git clone --recursive https://github.com/Tr3v0r86/papercolor-cal.git
   cd papercolor-cal
   . ~/esp/esp-idf-v5.5.1/export.sh
   idf.py set-target esp32s3
   idf.py build
   idf.py -p /dev/cu.usbmodemXXXX flash
   ```
   The board's USB port only exists while the ESP32 is powered, so press the power button first
   and flash within the 60 s idle window. A full flash wipes the vendor settings; a rebuild after
   that can be flashed app-only with `idf.py app-flash` and keeps them.
4. **Join Wi-Fi.** After the first boot (the vendor's "Press to ON" screen, then power off), press
   power, hold A for 5 s, scan the QR with your phone and enter your Wi-Fi in the portal.
   Low-power mode is on by default in this build.
5. **First fetch.** Press C. The board joins, syncs its clock from NTP, fetches, and redraws with
   today's events. From then on it does this itself every morning.

## How the day cycle works

```
04:00 RTC alarm -> M5PM1 powers the ESP32-S3 on
  join Wi-Fi (APSTA, up to 3 attempts) -> SNTP -> write local time to the RTC
  GET <GCAL_URL>?back=3&days=14  (follows the 302 to script.googleusercontent.com)
  parse -> one NVS blob per day -> draw today -> arm tomorrow's alarm
  deauth from the AP -> M5PM1 SYS_CMD_OFF
```

Two things that bit during bring-up and are handled: the alarm boot must bring the radio up the
same way a normal boot does (STA-only never authenticated), and the board must deauth before
cutting power or the router keeps the old association and ignores the next morning's auth.

## Project layout

```
main/apps/calendar/   app_calendar.{h,cpp}     the face, the daily cycle, day walking
main/cal/             cal_model.{h,c}          window parser, plain C, host-tested
                      cal_store.{h,cpp}        NVS: one blob per day + meta
                      cal_fetch.{h,cpp}        esp_http_client, cert bundle, redirects
main/apps/app_manager app_manager.cpp          buttons, idle power-off, alarm arming (vendor, trimmed)
main/hal/             vendor HAL: M5PM1, RX8130, SHT40, Wi-Fi manager; next_wake.h added
main/config.h         time zone offset, wake hour
tools/gcal-proxy/     Code.gs + deploy README
test/                 host tests: make -C test/cal test ; cc test/next_wake_test.c
case/                 fridge cradle: STLs, STEP, CadQuery source, README
```

## The case

![Cradle](case/preview.png)

Four 6 × 2 mm magnets, two fixed lower hooks, two M2 screw-down retainers. Print
`case/papercolor-fridge-mount-v3-m2-plate.stl`. Details, hardware list and assembly in
[case/README.md](case/README.md).

## Status

Running daily on one unit since 2026-09-26. Not yet measured: battery life off USB (estimate from
the PMIC's 90 µA standby plus one ~45 s active cycle a day is months). Known rough edge: right
after a full flash the vendor sets the clock to 2026-01-01, so the first face shows that date until
the first fetch syncs it.

## Credits and licence

MIT. The HAL, power scheme and Wi-Fi portal are M5Stack's (M5PaperColor-UserDemo). M5GFX and
M5Unified are M5Stack's libraries, pulled in as submodules. The calendar app, data path, tests,
face design and case are new in this repo.
