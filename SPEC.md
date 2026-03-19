# E-Ink Dashboard — Specification

## Overview

A personal desk display driven by a Waveshare 7.5" three-color e-ink screen and a Seeed Studio XIAO ESP32-S3. A Node.js server running on an always-on Linux machine serves a single JSON endpoint that the ESP32 polls on a configurable interval. A React web UI on the same server lets the user configure the display and connect Google Calendar.

---

## Hardware

| Component | Detail |
|-----------|--------|
| Display | Waveshare 7.5" e-Paper HAT (B), 800×480, red/black/white, SPI |
| Microcontroller | Seeed Studio XIAO ESP32-S3 |
| Connection | SPI (display HAT wired directly to ESP32) |
| Network | Wi-Fi, local network only |

---

## Goals

- Show date, weather, and today's Google Calendar events on a physical e-ink display
- Minimize refresh rate (no clock shown) to reduce display wear and power use
- Provide a simple web UI for configuration
- Run entirely on the local network — no cloud deployment

## Non-Goals

- Multiple displays / multi-device support
- Mobile app or notifications
- Any AI/LLM features
- Public internet exposure

---

## Display Layout

The display is 800×480, three-color (red, black, white).

```
+----------------------------------------------------------+
|                                                          |
|   WEDNESDAY, FEBRUARY 25          [red, large, bold]     |
|                                                          |
+----------------------------+-----------------------------+
|                            |                             |
|  WEATHER                   |  TODAY                      |
|                            |                             |
|  72°  Partly Cloudy        |  9:00 AM  Standup           |
|                            |  1:00 PM  Lunch w/ Sarah    |
|  H: 78°  L: 61°            |  3:30 PM  Code Review       |
|  Rain: 20%                 |  + 2 more                   |
|                            |                             |
+----------------------------+-----------------------------+
```

### Color Usage
- **Red:** Today's date line only
- **Black:** All other text, borders, section headers
- **White:** Background

### Display Rules
- No clock or time-of-day shown
- Calendar shows up to 3 events; if more exist, show `+ N more` at the bottom
- All-day events display `All Day` instead of a time
- Event format: `HH:MM AM/PM  Event Title` (truncated if too long)
- Weather section always shown (no toggle)
- Calendar section always shown (falls back to a message if not connected)

---

## Data Sources

### Weather — Open-Meteo
- **API:** [open-meteo.com](https://open-meteo.com) — free, no API key required
- **Input:** Latitude + longitude (from config)
- **Fields fetched:**
  - Current temperature (°F)
  - Current weather condition (mapped from WMO weather code)
  - Daily high/low (°F)
  - Precipitation probability (%)
- **Caching:** Server caches the Open-Meteo response for 30 minutes to avoid redundant requests on every ESP32 poll

### Calendar — Google Calendar API
- **Auth:** OAuth 2.0, read-only scopes
  - `https://www.googleapis.com/auth/calendar.readonly`
  - `https://www.googleapis.com/auth/calendar.events.readonly`
- **Data:** Today's events from the user's primary calendar
  - `timeMin`: start of current day
  - `timeMax`: end of current day
  - `singleEvents: true`, `orderBy: startTime`
  - `maxResults: 10` (server fetches up to 10, sends all to client, display shows 3)
- **Token storage:** Access token, refresh token, and expiry stored in SQLite
- **Token refresh:** Handled server-side via googleapis library before each calendar fetch

---

## Server

### Stack
- **Runtime:** Node.js
- **Server:** Express
- **Frontend:** React 19 + Vite + Tailwind CSS (served as built static files in production)
- **Database:** SQLite via better-sqlite3
- **Language:** TypeScript

### Environment Variables

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `REFRESH_RATE_MINUTES` | No | `60` | How often the ESP32 should sleep between polls |
| `GOOGLE_CLIENT_ID` | Yes (for calendar) | — | Google OAuth client ID |
| `GOOGLE_CLIENT_SECRET` | Yes (for calendar) | — | Google OAuth client secret |
| `PORT` | No | `3000` | Port the server listens on |

> `APP_URL` is not needed — the redirect URI is constructed dynamically from the incoming request host, which works correctly on a local network.

### API Endpoints

#### `GET /api/dashboard-data`
Polled by the ESP32. Returns all data needed to render the display.

**Response:**
```json
{
  "date": "Wednesday, February 25",
  "weather": {
    "tempF": 72,
    "condition": "Partly Cloudy",
    "highF": 78,
    "lowF": 61,
    "precipitationPct": 20
  },
  "calendar": {
    "events": [
      { "title": "Standup", "time": "9:00 AM", "allDay": false },
      { "title": "Lunch w/ Sarah", "time": "1:00 PM", "allDay": false },
      { "title": "Code Review", "time": "3:30 PM", "allDay": false }
    ],
    "totalCount": 5
  },
  "settings": {
    "refreshRateMinutes": 60
  }
}
```

**Notes:**
- `calendar.events` always contains at most 3 items
- `calendar.totalCount` is the full count — ESP32 uses `totalCount - 3` to compute "+ N more"
- If calendar is not connected, `calendar` is `null`
- Weather is always present (Open-Meteo requires no auth)

---

#### `GET /api/config`
Read by the web UI. Returns current config. OAuth tokens are never sent to the frontend.

**Response:**
```json
{
  "locationLat": "47.6062",
  "locationLon": "-122.3321",
  "isGoogleConnected": true
}
```

---

#### `PUT /api/config`
Saves config from the web UI in a single request (replaces N sequential POSTs).

**Request body:**
```json
{
  "locationLat": "47.6062",
  "locationLon": "-122.3321"
}
```

**Response:**
```json
{ "success": true }
```

---

#### `GET /api/auth/google/url`
Returns a Google OAuth URL for the web UI to open in a popup.

**Response:**
```json
{ "url": "https://accounts.google.com/o/oauth2/auth?..." }
```

**Notes:**
- Generates a random `state` parameter and stores it in the DB for CSRF validation
- Redirect URI is constructed from `req.headers.host`

---

#### `GET /auth/callback`
OAuth redirect target. Validates `state`, exchanges code for tokens, stores tokens in DB, closes the popup.

- On success: sends HTML that posts `{ type: 'OAUTH_SUCCESS' }` to `window.opener` using the server's own origin (not `'*'`), then closes the window
- On failure: returns a plain error page

---

### Database Schema

**Table: `config`** (key-value store)

| Key | Default | Description |
|-----|---------|-------------|
| `location_lat` | `''` | Latitude |
| `location_lon` | `''` | Longitude |
| `google_access_token` | `''` | OAuth access token |
| `google_refresh_token` | `''` | OAuth refresh token |
| `google_token_expiry` | `''` | Token expiry timestamp |
| `oauth_state` | `''` | CSRF state token (ephemeral) |

> Removed from current code: `weatherApiKey`, `showWeather`, `showCalendar`, `customMessage`, `theme`, `refreshRate` (refresh rate moves to env var; weather and calendar are always shown)

---

## Web UI

Single-page app with two tabs:

### Configuration Tab (default)
- Lat/lon input fields
- Google Calendar connect button (shows connected status + green checkmark when connected)
- Save button (single `PUT /api/config` request)

### Arduino Code Tab
- Displays the generated `.ino` sketch with Copy and Download buttons
- Injects the server's local IP/hostname automatically

### Display Preview Tab
- Renders a pixel-accurate simulation of the 800×480 three-color display in the browser
- Uses the live data from `GET /api/dashboard-data`
- Red/black/white color scheme matching the physical display
- Manual refresh button

---

## Arduino Sketch

### Libraries Required
| Library | Version | Purpose |
|---------|---------|---------|
| GxEPD2 | latest | Waveshare e-ink display driver |
| ArduinoJson | v7.x | JSON parsing |
| WiFi | built-in (ESP32 core) | Wi-Fi connection |
| HTTPClient | built-in (ESP32 core) | HTTP requests |

### Sketch Structure

```
setup()
  └── Connect to WiFi
  └── Fetch /api/dashboard-data via HTTP GET
  └── Parse JSON (ArduinoJson v7 — JsonDocument)
  └── Initialize GxEPD2 display
  └── Render display
       ├── Draw date in RED
       ├── Draw weather section in BLACK
       └── Draw calendar section in BLACK
  └── Enter deep sleep for refreshRateMinutes
       └── esp_sleep_enable_timer_wakeup(refreshRateMinutes * 60 * 1000000ULL)
       └── esp_deep_sleep_start()

loop()
  └── empty — deep sleep wakes into setup()
```

### Key Implementation Notes
- Use `esp_deep_sleep_start()` — never `delay()` for long sleeps
- `refreshRateMinutes` is read from the JSON response, not hardcoded
- Three-color rendering: use `GxEPD_RED` for date, `GxEPD_BLACK` for everything else
- JSON document size: 1024 bytes is sufficient for this payload
- Use `JsonDocument` (ArduinoJson v7) — not `DynamicJsonDocument`

---

## Dependency Changes

### Remove
- `@google/genai` — unused, was a leftover from the AI Studio template

### Keep
- `express`, `googleapis`, `better-sqlite3`, `dotenv`
- All React/Vite/Tailwind frontend deps

### No additions needed
- Open-Meteo requires no SDK — plain `fetch` to their REST API

---

## Out of Scope (for now)
- Battery level or WiFi RSSI reporting from ESP32 back to server
- Multiple calendar sources
- Weather forecast beyond today
- Display themes or layout customization
