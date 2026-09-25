# papercolor-cal

Daily Google Calendar face for the M5Stack PaperColor (SKU C151, ESP32-S3R8, 400x600 Spectra 6).
A private fork of [m5stack/M5PaperColor-UserDemo](https://github.com/m5stack/M5PaperColor-UserDemo)
(remote `upstream`), stripped to the vendor HAL, the Wi-Fi config portal and one app: the calendar.

## Build and flash

ESP-IDF v5.5.1 (the vendor's pin), beside v5.4, which the rest of esp-devwork uses.

```bash
git submodule update --init --recursive
source ~/esp/esp-idf-v5.5.1/export.sh
idf.py set-target esp32s3   # once
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash
```

One file flashable at 0x0 (run from `build/`):

```bash
esptool.py --chip esp32s3 merge_bin -o ../dist/papercolor-cal.bin @flash_args
esptool.py --chip esp32s3 -p /dev/cu.usbmodemXXXX write_flash 0x0 ../dist/papercolor-cal.bin
```

`dist/vanilla-<sha>.bin` is the unmodified upstream build (16 MiB, includes the vendor's photo
FAT image). Flash it first to confirm the vendor power scheme on the unit. Wi-Fi credentials and
low-power mode live in NVS namespace `papercolor` under the same keys in both builds, so settings
made on vanilla carry over.

## Secrets

`cp main/secrets.h.example main/secrets.h` and set `GCAL_URL` to the Apps Script web app URL.
`main/secrets.h` is gitignored; without it the tree still builds (the URL is empty).

## Power scheme

1. No ESP32 deep sleep: `M5PM1_SYS_CMD_OFF` cuts the S3 entirely; the power button or the RTC alarm powers it back on.
2. Before powering off, the RX8130CE alarm is armed for the next 04:00 Asia/Bangkok (the RTC holds Bangkok local time, set from SNTP).
3. Alarm boot: join Wi-Fi, SNTP to the RTC, fetch, render today, power off immediately.
4. Any other boot: render today, then buttons walk the days (A prev, C next, B today, B hold 1.5 s = fetch now; A hold 5 s = Wi-Fi portal); power off after 60 s idle.
5. Battery under 3100 mV: power off without rendering. Power-off and the alarm run only with low-power mode on (portal setting, default off as upstream).

## Docs

In the esp-devwork repo: `docs/adr/0014-papercolor-is-the-lane-on-the-vendor-base.md` (the decision)
and `docs/superpowers/specs/2026-09-25-papercolor-calendar-design.md` (behaviour, data, face).

Host check for the alarm date maths: `cc test/next_wake_test.c -o /tmp/nw && /tmp/nw`.
