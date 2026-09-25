/**
 * gcal-proxy: the page's one data source. A web app on trevor.cardozo@gmail.com that returns a
 * small JSON window of every selected calendar, expanded and formatted in Asia/Bangkok.
 * Contract: docs/superpowers/specs/2026-09-25-papercolor-calendar-design.md section 2.
 *
 *   GET <exec url>?back=3&days=14   window today-back .. today+days-1 (back 0..7, days 1..21)
 *   GET <exec url>?ping=1           {"ok":true}
 */
var TZ = 'Asia/Bangkok';
var MAX_EVENTS = 24;
var TITLE_MAX = 60;
var CALNAME_MAX = 24;
var DAY_MS = 24 * 60 * 60 * 1000;

// Google's event palette as the Calendar UI renders it: [EventColor name, colorId, hex].
var EVENT_COLORS = [
  ['PALE_BLUE', '1', '#7986cb'],   // Lavender
  ['PALE_GREEN', '2', '#33b679'],  // Sage
  ['MAUVE', '3', '#8e24aa'],       // Grape
  ['PALE_RED', '4', '#e67c73'],    // Flamingo
  ['YELLOW', '5', '#f6bf26'],      // Banana
  ['ORANGE', '6', '#f4511e'],      // Tangerine
  ['CYAN', '7', '#039be5'],        // Peacock
  ['GRAY', '8', '#616161'],        // Graphite
  ['BLUE', '9', '#3f51b5'],        // Blueberry
  ['GREEN', '10', '#0b8043'],      // Basil
  ['RED', '11', '#d50000']         // Tomato
];

function doGet(e) {
  var p = (e && e.parameter) || {};
  if (p.ping) return json_({ ok: true });

  var back = clampInt_(p.back, 3, 0, 7);
  var days = clampInt_(p.days, 14, 1, 21);
  var now = new Date();
  var offset = Utilities.formatDate(now, TZ, 'XXX');   // "+07:00"
  var today = new Date(Utilities.formatDate(now, TZ, 'yyyy-MM-dd') + 'T00:00:00' + offset);
  var start = new Date(today.getTime() - back * DAY_MS);
  var end = new Date(start.getTime() + (back + days) * DAY_MS);

  // ponytail: fixed 24 h days, correct because Bangkok has no DST; step by calendar date if TZ changes.
  var list = [];
  for (var i = 0; i < back + days; i++) {
    var s = start.getTime() + i * DAY_MS;
    list.push({ date: Utilities.formatDate(new Date(s), TZ, 'yyyy-MM-dd'), s: s, e: s + DAY_MS, events: [] });
  }

  var eventHex = eventHexMap_();
  var scriptTz = Session.getScriptTimeZone();
  CalendarApp.getAllCalendars().forEach(function (cal) {
    if (!cal.isSelected()) return;
    var calColor = hex_(cal.getColor());
    var calName = cut_(cal.getName(), CALNAME_MAX);
    var evs;
    try {
      evs = cal.getEvents(start, end);   // recurrences arrive expanded
    } catch (err) {
      return;                            // one broken shared calendar must not blank the page
    }
    evs.forEach(function (ev) {
      if (declined_(ev)) return;
      var base = { title: cut_(ev.getTitle() || '(No title)', TITLE_MAX), cal: calName };
      var color = eventHex[String(ev.getColor())] || calColor;
      if (color) base.color = color;

      if (ev.isAllDayEvent()) {
        // Floating dates, reported as midnight in the script's zone; the end is exclusive.
        var a = Utilities.formatDate(ev.getAllDayStartDate(), scriptTz, 'yyyy-MM-dd');
        var b = Utilities.formatDate(ev.getAllDayEndDate(), scriptTz, 'yyyy-MM-dd');
        list.forEach(function (d) {
          if (d.date >= a && d.date < b) d.events.push(withTimes_(base, true));
        });
        return;
      }
      var t0 = ev.getStartTime().getTime(), t1 = ev.getEndTime().getTime();
      list.forEach(function (d) {
        if (t0 >= d.e || (t1 <= d.s && t0 < d.s)) return;   // no overlap with this day
        d.events.push(withTimes_(base, false,
          t0 <= d.s ? '00:00' : Utilities.formatDate(new Date(t0), TZ, 'HH:mm'),
          t1 >= d.e ? '24:00' : Utilities.formatDate(new Date(t1), TZ, 'HH:mm')));
      });
    });
  });

  return json_({
    tz: TZ,
    generated: Utilities.formatDate(now, TZ, "yyyy-MM-dd'T'HH:mm:ssXXX"),
    back: back,
    days: days,
    days_list: list.map(function (d) {
      return { date: d.date, events: d.events.sort(byAllDayThenStart_).slice(0, MAX_EVENTS) };
    })
  });
}

function withTimes_(base, allday, startHm, endHm) {
  var o = { allday: allday };
  if (!allday) { o.start = startHm; o.end = endHm; }
  o.title = base.title;
  o.cal = base.cal;
  if (base.color) o.color = base.color;
  return o;
}

function byAllDayThenStart_(a, b) {
  if (a.allday !== b.allday) return a.allday ? -1 : 1;
  var ka = (a.start || '') + (a.end || '') + a.title;
  var kb = (b.start || '') + (b.end || '') + b.title;
  return ka < kb ? -1 : ka > kb ? 1 : 0;
}

function declined_(ev) {
  try {
    return ev.getMyStatus() === CalendarApp.GuestStatus.NO;
  } catch (err) {
    return false;   // calendars where the owner has no guest status
  }
}

// Keyed by colorId ("11") and by the enum's string value, whichever getColor() hands back.
function eventHexMap_() {
  var m = {};
  EVENT_COLORS.forEach(function (c) {
    m[c[1]] = c[2];
    if (CalendarApp.EventColor[c[0]] !== undefined) m[String(CalendarApp.EventColor[c[0]])] = c[2];
  });
  return m;
}

function hex_(s) {
  return /^#[0-9a-f]{6}$/i.test(s || '') ? s.toLowerCase() : null;
}

// Cut by code point so an emoji or a Thai cluster is never split mid-surrogate.
function cut_(s, n) {
  return Array.from(String(s || '')).slice(0, n).join('');
}

function clampInt_(v, dflt, lo, hi) {
  var n = parseInt(v, 10);
  if (isNaN(n)) n = dflt;
  return Math.max(lo, Math.min(hi, n));
}

function json_(o) {
  return ContentService.createTextOutput(JSON.stringify(o)).setMimeType(ContentService.MimeType.JSON);
}
