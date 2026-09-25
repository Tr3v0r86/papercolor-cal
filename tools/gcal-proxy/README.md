# gcal-proxy

A Google Apps Script web app that turns your selected Google Calendars into the small JSON window
the board fetches once a day. `Code.gs` is the whole server. No OAuth on the device, no client
secrets, no refresh tokens that expire: the device does one HTTPS GET.

**Trust model:** the deployment URL is the only secret, at the same level as Google's own
"secret address in iCal format". Anyone holding it can read your calendar window. Keep it in the
gitignored `main/secrets.h` and nowhere else.

## Deploy once

1. Open [script.google.com](https://script.google.com) signed in as the Google account whose
   calendars you want, **New project**, name it `gcal-proxy`.
2. Replace the editor contents with this `Code.gs`. If your time zone is not Asia/Bangkok, change
   `TZ` at the top (and `CAL_UTC_OFFSET_S` in the firmware's `main/config.h` to match). Save.
3. **Deploy > New deployment**, type **Web app**. Execute as **Me**. Who has access **Anyone**.
   Deploy.
4. Authorise. Google warns that the app is unverified: **Advanced > Go to gcal-proxy (unsafe)**,
   allow calendar read access. It is your own script running as you.
5. Copy the **Web app URL** (ends in `/exec`) into `main/secrets.h` as `GCAL_URL`.

## Redeploy after editing

Saving does not change what `/exec` serves. **Deploy > Manage deployments**, pencil, **Version:
New version**, Deploy. Same URL. A "New deployment" mints a different URL and the board keeps
fetching the old one.

## Test

```sh
curl -sL "$GCAL_URL?back=3&days=14" | python3 -m json.tool | head -40
```

`/exec` answers with a 302 to `script.googleusercontent.com`, so every client must follow
redirects (`-L`; the firmware sets `disable_auto_redirect = false`). The redirect URL is several
hundred characters, which is why the firmware enlarges the HTTP client's transmit buffer. A cold
script can take 15 to 30 s to answer; the firmware allows 45 s.

## What the script returns

```json
{"tz":"Asia/Bangkok","generated":"2026-09-25T04:00:12+07:00","back":3,"days":14,
 "days_list":[{"date":"2026-09-25","events":[
   {"allday":true,"title":"Holiday"},
   {"allday":false,"start":"09:00","end":"10:30","title":"Dentist","cal":"Personal","color":"#7986cb"}]}]}
```

Window is `today - back` to `today + days - 1` (defaults 3 and 14, clamped 0..7 and 1..21). Every
calendar with `isSelected()` is read with one `getEvents(start, end)` call (recurrences arrive
expanded) and each event is placed on every local day it overlaps; events crossing midnight are
clamped to `00:00` / `24:00`. Declined events are dropped. All-day first, then by start. Titles cut
to 60 characters, calendar names to 24, at most 24 events per day. `color` is the event's own
colour if set, else the calendar's, as `#rrggbb`; omitted when neither is valid.
