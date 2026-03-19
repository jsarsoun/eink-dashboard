# E-Ink Dashboard — Architecture

## System Overview

```
+------------------+         HTTP GET          +-------------------+
|  XIAO ESP32-S3   |  ──────────────────────>  |                   |
|  + Waveshare     |  /api/dashboard-data       |   Express Server  |
|  7.5" e-ink      |  <──────────────────────   |   (Linux machine) |
|                  |  JSON response             |                   |
+------------------+                           +-------------------+
                                                        |
+------------------+         HTTP              +-------------------+
|   Browser        |  ──────────────────────>  |                   |
|   Config UI      |  /api/config              |   Same server,    |
|   (React SPA)    |  /api/auth/google/*        |   same port       |
|                  |  <──────────────────────   |                   |
+------------------+                           +-------------------+
                                                        |
                                               +--------+--------+
                                               |                 |
                                       +-------+------+  +-------+------+
                                       | Open-Meteo   |  | Google       |
                                       | Weather API  |  | Calendar API |
                                       | (no auth)    |  | (OAuth 2.0)  |
                                       +--------------+  +--------------+
```

The ESP32 and the browser config UI both talk to the same Express server on the local network. The server is the only process that communicates with external APIs.

---

## Directory Structure

```
eInk/
├── SPEC.md
├── ARCHITECTURE.md
├── .env                          # Environment variables (gitignored)
├── .env.example                  # Template for required env vars
├── index.html                    # Vite entry point
├── vite.config.ts
├── tsconfig.json
├── package.json
│
├── server.ts                     # Entry point — app setup and server start only
│
├── src/
│   ├── main.tsx                  # React entry point
│   ├── index.css
│   ├── App.tsx                   # Single-page config UI
│   │
│   ├── routes/
│   │   ├── config.ts             # GET /api/config, PUT /api/config
│   │   ├── dashboard.ts          # GET /api/dashboard-data
│   │   └── auth.ts               # GET /api/auth/google/url, GET /auth/callback
│   │
│   ├── services/
│   │   ├── weather.ts            # Open-Meteo fetch + in-memory cache
│   │   └── calendar.ts           # Google Calendar fetch + auto token refresh
│   │
│   └── db.ts                     # SQLite wrapper (getConfig, setConfig, getAllConfig)
│
└── arduino/
    └── EInkDashboard.ino         # Static Arduino sketch (server URL hardcoded by user)
```

---

## Layer Responsibilities

### `server.ts` — Entry Point
- Creates the Express app
- Registers route modules
- Mounts Vite middleware in development
- Serves built static files in production
- Starts the HTTP listener

### `src/routes/config.ts`
- `GET /api/config` — reads all config from SQLite, strips OAuth tokens before responding
- `PUT /api/config` — accepts a full config object, saves each key to SQLite in a single transaction

### `src/routes/dashboard.ts`
- `GET /api/dashboard-data` — fetches weather and calendar in parallel via `Promise.all`
- On success: returns assembled JSON payload
- On failure: returns `500` with `{ error: string }` describing which service failed

### `src/routes/auth.ts`
- `GET /api/auth/google/url` — generates a random `state` token, saves it to DB, returns OAuth URL
- `GET /auth/callback` — validates `state`, exchanges code for tokens, saves tokens to DB, closes popup via `postMessage` scoped to server origin

### `src/services/weather.ts`
- Fetches current conditions, high/low, and precipitation probability from Open-Meteo
- Caches the last successful response in memory with a timestamp
- Cache TTL: 30 minutes
- On cache hit: returns cached data immediately (no network call)
- On cache miss or TTL expired: fetches fresh data, updates cache, returns data

```
interface WeatherCache {
  data: WeatherData;
  fetchedAt: number; // Date.now()
}

interface WeatherData {
  tempF: number;
  condition: string;
  highF: number;
  lowF: number;
  precipitationPct: number;
}
```

### `src/services/calendar.ts`
- Initializes the Google OAuth2 client with credentials from environment variables
- Sets stored tokens (access + refresh) from SQLite on each use
- Registers a `tokens` event listener on the OAuth2 client — when googleapis auto-refreshes the access token, the listener saves the new token and expiry back to SQLite
- Fetches today's events (midnight to 23:59:59) from the primary calendar
- Returns all events; the route layer limits to 3 for the response payload

### `src/db.ts`
- Thin wrapper around better-sqlite3
- Exports: `getConfig(key)`, `setConfig(key, value)`, `getAllConfig()`
- Initializes schema and seeds default values on first run

### `src/App.tsx` — Config UI
- Single page, no tabs
- Renders the `ConfigForm` component
- No preview, no Arduino code display

### `arduino/EInkDashboard.ino`
- Static file — not generated at runtime
- User hardcodes their server's local IP and port
- See sketch structure below

---

## Data Flows

### 1. ESP32 Display Refresh

```
ESP32 wakes from deep sleep
  └── WiFi.begin() → wait for WL_CONNECTED
  └── HTTPClient GET http://<SERVER_IP>:<PORT>/api/dashboard-data
        └── server: Promise.all([fetchWeather(), fetchCalendar()])
              ├── weather: cache hit → return cached data
              │            cache miss → GET open-meteo.com → cache → return
              └── calendar: set OAuth credentials from DB
                            googleapis fetches events
                            if token refreshed → save new token to DB
        └── server assembles response JSON → 200 OK
  └── ESP32: deserialize JSON (ArduinoJson v7 JsonDocument)
  └── ESP32: initialize GxEPD2 display
  └── ESP32: render display
        ├── date string → GxEPD_RED
        ├── weather section → GxEPD_BLACK
        └── calendar section → GxEPD_BLACK
  └── read refreshRateMinutes from JSON
  └── esp_sleep_enable_timer_wakeup(refreshRateMinutes * 60 * 1000000ULL)
  └── esp_deep_sleep_start()
```

### 2. ESP32 Error Handling

```
ESP32 wakes from deep sleep
  └── HTTP GET /api/dashboard-data → 500 { "error": "calendar unavailable" }
  └── ESP32: previous display contents remain unchanged
  └── ESP32: render small error message bottom-right corner
        └── e.g. "calendar unavailable" in GxEPD_BLACK
  └── deep sleep for REFRESH_RATE_MINUTES (read from env, fallback 60)
```

> On a 500 response the ESP32 cannot read `refreshRateMinutes` from the payload. It falls back to a hardcoded default matching the server's `REFRESH_RATE_MINUTES` env var. These two values should be kept in sync.

### 3. Config Save (Web UI)

```
User edits lat/lon in ConfigForm
  └── clicks Save
  └── PUT /api/config { locationLat, locationLon }
        └── server: saves each key to SQLite
        └── 200 { success: true }
  └── UI shows success state
```

### 4. Google Calendar OAuth (Web UI)

```
User clicks "Connect Google Calendar"
  └── GET /api/auth/google/url
        └── server: generate random state token
        └── server: save state to DB (key: oauth_state)
        └── return { url: "https://accounts.google.com/..." }
  └── window.open(url, popup)
  └── user authenticates in popup
  └── Google redirects to GET /auth/callback?code=...&state=...
        └── server: validate state matches DB value
        └── server: exchange code for tokens via googleapis
        └── server: save access_token, refresh_token, expiry to DB
        └── server: clear oauth_state from DB
        └── server: respond with HTML that calls:
              window.opener.postMessage({ type: 'OAUTH_SUCCESS' }, window.location.origin)
              window.close()
  └── ConfigForm message listener receives OAUTH_SUCCESS
  └── re-fetches /api/config → updates connected status in UI
```

---

## Environment Variables

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `REFRESH_RATE_MINUTES` | No | `60` | Sleep duration sent to ESP32 in dashboard response |
| `GOOGLE_CLIENT_ID` | Yes* | — | Google OAuth client ID |
| `GOOGLE_CLIENT_SECRET` | Yes* | — | Google OAuth client secret |
| `PORT` | No | `3000` | HTTP server port |

*Required only if Google Calendar integration is used. Server starts without them; OAuth routes return a clear error if they are missing.

---

## Database Schema

**Table: `config`**

| Key | Default | Description |
|-----|---------|-------------|
| `location_lat` | `''` | Latitude string |
| `location_lon` | `''` | Longitude string |
| `google_access_token` | `''` | Current OAuth access token |
| `google_refresh_token` | `''` | OAuth refresh token (long-lived) |
| `google_token_expiry` | `''` | Access token expiry (ms timestamp as string) |
| `oauth_state` | `''` | Ephemeral CSRF state token, cleared after callback |

---

## Arduino Sketch Structure

```cpp
// EInkDashboard.ino

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // v7
#include <GxEPD2_3C.h>    // Three-color Waveshare driver

// User configuration — edit these
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverUrl = "http://192.168.x.x:3000/api/dashboard-data";

// Default sleep duration — keep in sync with server REFRESH_RATE_MINUTES
const int DEFAULT_REFRESH_MINUTES = 60;

void setup() {
  // 1. Connect to WiFi
  // 2. HTTP GET serverUrl
  // 3. If response != 200:
  //      render error message bottom-right
  //      deep sleep DEFAULT_REFRESH_MINUTES
  // 4. Parse JSON with JsonDocument
  // 5. Initialize GxEPD2 display
  // 6. Render:
  //      - date (GxEPD_RED)
  //      - weather section (GxEPD_BLACK)
  //      - calendar section (GxEPD_BLACK)
  // 7. Read refreshRateMinutes from JSON
  // 8. esp_sleep_enable_timer_wakeup(refreshRateMinutes * 60ULL * 1000000ULL)
  // 9. esp_deep_sleep_start()
}

void loop() {
  // Intentionally empty — deep sleep wakes into setup()
}
```

---

## Development vs. Production

| | Development | Production |
|--|-------------|------------|
| Start command | `npm run dev` | `npm run build && npm start` |
| Frontend | Vite dev middleware (HMR) | Compiled static files served by Express |
| Static files | Served by Vite | Served from `dist/` by `express.static` |
| Environment | `.env` via dotenv | `.env` or system environment variables |

---

## Dependencies

### Removed
- `@google/genai` — unused leftover from AI Studio template

### Unchanged
- `express`, `googleapis`, `better-sqlite3`, `dotenv`
- `react`, `react-dom`, `vite`, `tailwindcss`, `lucide-react`
- `tsx` (dev server runner)

### No new dependencies needed
- Open-Meteo uses the native `fetch` API — no SDK required
