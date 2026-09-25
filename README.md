# papercolor-cal

Daily Google Calendar face for the M5Stack PaperColor (SKU C151, ESP32-S3R8, 400x600 Spectra 6).
A private fork of [m5stack/M5PaperColor-UserDemo](https://github.com/m5stack/M5PaperColor-UserDemo)
(remote `upstream`), stripped to the vendor HAL, the Wi-Fi config portal and one app: the calendar.

## Build

ESP-IDF v5.5.1 (the vendor's pin), beside v5.4, which the rest of esp-devwork uses.

```bash
git submodule update --init --recursive
source ~/esp/esp-idf-v5.5.1/export.sh
idf.py set-target esp32s3   # once
idf.py build
cd build && esptool.py --chip esp32s3 merge_bin -o ../dist/calendar-$(git rev-parse --short HEAD).bin @flash_args
```

Host tests: `make -C test/cal test` (the window parser, against the firmware's own
`main/cal/cal_model.c`) and `cc test/next_wake_test.c -o /tmp/nw && /tmp/nw` (the 04:00 date maths).

Partition table (16 MB flash): `nvs` 0x9000 +0x10000 (the day blobs), `phy_init` 0x19000 +0x1000,
`factory` 0x20000 +0x9E0000, `storage` (FAT, unused by the calendar) 0xA00000 +0x600000.

## First flash (Trevor, in this order)

Flashing is yours; agents only build. Each image is one file written at 0x0:
`esptool.py --chip esp32s3 -p /dev/cu.usbmodemXXXX write_flash 0x0 dist/<image>.bin`.

a. **Vanilla first, to prove the power scheme on this unit.** Flash `dist/vanilla-1ff998e.bin`
   (the unmodified upstream build, 16 MiB). Hold A for 5 s, scan the QR, join the portal, set
   Wi-Fi and turn **low-power ON**. Leave it alone: it must power off after 60 s idle, and the
   power button must wake it.
b. **Deploy the proxy.** Deploy `tools/gcal-proxy/Code.gs` per its README in esp-devwork (as
   trevor.cardozo@gmail.com). `cp main/secrets.h.example main/secrets.h` and put the `/exec` URL
   in `GCAL_URL`. Check it with `curl -sL "$GCAL_URL?ping=1"`.
c. **Rebuild and flash the calendar.** Build as above, flash `dist/calendar-<sha>.bin`
   (`dist/` is gitignored; `dist/calendar-071b8ef.bin` is the first build, from commit 071b8ef). **This wipes
   NVS:** the merged image writes 0xFF over the whole nvs range (and nvs grew, so the vanilla
   layout could not be reused anyway). Consequences, in order: the first boot shows the vendor
   "Press to ON" guide, sets the clock to 2026-01-01 and powers off; press the power button.
   Then hold A 5 s and set Wi-Fi again in the portal. Low-power mode now defaults ON, so only
   Wi-Fi needs re-entering. The phone must join the AP within the 60 s idle window.
d. **First face.** With the store empty the face shows the RTC's date (01 JANUARY 2026 until
   SNTP lands) on the blue TODAY band, `NO CALENDAR DATA` in the list, and `NEVER UPDATED` on a
   yellow cell. Long-press B (1.5 s) to fetch now: Wi-Fi, SNTP to the RTC, fetch, store, render.
   The face should come back with today's events and an `UPDATED <day> <hh:mm>` cell on white.
e. **What to check on glass.**
   1. Orientation: the face upright with A, B, C reading left to right. If it is upside down,
      set `CAL_ROTATION` to 2 in `main/apps/calendar/app_calendar.cpp` and rebuild.
   2. Fringe: the fonts are 1 bit and dithering is off, so caps should show no green or yellow
      speckle, including black on the yellow cell and white on the blue band.
   3. Tabs: every event's 6 px colour tab is visible, yellow being the faintest.
   4. Buttons: A and C walk the days and stop dead at the window edge (3 back, 13 ahead);
      B returns to today. Each accepted press is one 15 to 30 s refresh.
   5. The 04:00 wake next morning: the board powers itself on around 04:00, fetches, renders
      and powers off; the footer then reads `UPDATED <that day> 04:00` on white.


> **Stale-build trap:** `app_calendar.cpp` includes `secrets.h` through `__has_include`. If you create
> or change `main/secrets.h` after a build, ninja does not know the object depends on it. Run
> `touch main/apps/calendar/app_calendar.cpp` (or `idf.py fullclean`) before `idf.py build`, then confirm
> with `strings -n 20 build/paper_color.bin | grep -c script.google.com`. Bitten live 2026-09-25.

## Serial monitor

```bash
idf.py -p /dev/cu.usbmodemXXXX monitor --no-reset
```

The console is UART0 on GPIO5/4 with the USB-Serial/JTAG as the secondary console. `--no-reset`
keeps the monitor from rebooting the board on attach (a reset boot is not an RTC-alarm boot).
**When the M5PM1 powers the S3 off, the USB-Serial/JTAG device disappears** from the Mac
entirely, and it reappears only when the power button or the 04:00 alarm powers the S3 again, so
the monitor has to be re-attached after every power-off; the first lines of a wake are usually
missed. To watch a whole cycle, turn low-power off in the portal while debugging.

## Secrets

`cp main/secrets.h.example main/secrets.h` and set `GCAL_URL` to the Apps Script web app URL.
`main/secrets.h` is gitignored; without it the tree still builds (the URL is empty).

## Power scheme

1. No ESP32 deep sleep: `M5PM1_SYS_CMD_OFF` cuts the S3 entirely; the power button or the RTC alarm powers it back on.
2. Before powering off, the RX8130CE alarm is armed for the next 04:00 Asia/Bangkok (the RTC holds Bangkok local time, set from SNTP).
3. Alarm boot: join Wi-Fi, SNTP to the RTC, fetch, render today, power off immediately.
4. Any other boot: render today, then buttons walk the days (A prev, C next, B today, B hold 1.5 s = fetch now; A hold 5 s = Wi-Fi portal); power off after 60 s idle.
5. Battery under 3100 mV: power off without rendering. Power-off and the alarm run only with low-power mode on (portal setting, default ON here; upstream defaults it off, which leaves the board awake forever).

## Docs

In the esp-devwork repo: `docs/adr/0014-papercolor-is-the-lane-on-the-vendor-base.md` (the decision)
and `docs/superpowers/specs/2026-09-25-papercolor-calendar-design.md` (behaviour, data, face).

