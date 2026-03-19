# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

E-Ink Dashboard: a personal desk display combining a Waveshare 7.5" three-color e-ink display (800×480, red/black/white) with a Seeed XIAO ESP32-S3 microcontroller, served by a local Node.js backend with a React config UI. The ESP32 wakes from deep sleep, fetches JSON from the local server, renders the display, and sleeps again.

## Server Commands

```bash
npm run dev      # Vite HMR + tsx server on port 3000 (development)
npm run build    # Compile React → dist/
npm start        # Production: serve static files from dist/
npm run lint     # TypeScript type check
```

## Environment Variables

Create `.env` from `.env.example`:
- `GOOGLE_CLIENT_ID` / `GOOGLE_CLIENT_SECRET` — required for Google Calendar
- `REFRESH_RATE_MINUTES` — default 60 (controls ESP32 deep sleep duration)
- `PORT` — default 3000

## Arduino Sketches

Three variants in `arduino/`:
- **`EInkDashboard/`** — primary sketch for Freenove ESP32-WROOM
- **`EInkDashboardS3/`** — XIAO ESP32-S3 variant with GPIOViewer debug (no deep sleep)
- **`EInkTest/`** — minimal wiring test (no WiFi, draws pattern only)

Flash via Arduino IDE or command-line:
```bash
./arduino-cli.exe compile --fqbn esp32:esp32:esp32-wroom:PSRAM=enabled arduino/EInkDashboard/
./arduino-cli.exe upload -p COM7 arduino/EInkDashboard/
python monitor.py   # Serial monitor on COM7 @ 115200 baud (runs 2 minutes)
```

Required Arduino libraries: `GxEPD2`, `ArduinoJson` v7, `Adafruit GFX`.

## Pin Assignments

**Freenove ESP32-WROOM** (`EInkDashboard.ino`):
| GPIO | Function |
|------|----------|
| 15   | EPD_PWR (power enable, active HIGH) |
| 5    | EPD_CS |
| 17   | EPD_DC |
| 16   | EPD_RST |
| 4    | EPD_BUSY (LOW = busy) |
| 18   | SPI CLK |
| 23   | SPI MOSI |

**XIAO ESP32-S3** (`EInkDashboardS3.ino`): D1/GPIO2=CS, D2/3=DC, D3/4=RST, D4/5=BUSY, D5/6=PWR, D8/7=CLK, D10/9=MOSI.

WiFi credentials and `SERVER_URL` are hardcoded in the sketch (search for `WIFI_SSID`).

## Architecture

**Data flow:** ESP32 wakes → HTTP GET `/api/dashboard-data` → parse JSON (ArduinoJson v7) → render display (date in RED, weather/calendar in BLACK) → deep sleep for `refreshRateMinutes`.

**Backend** (`server.ts` → `src/routes/`, `src/services/`):
- `GET /api/dashboard-data` — fetches weather + calendar in parallel, returns combined JSON
- `GET/PUT /api/config` — reads/writes location lat/lon to SQLite
- `GET /api/auth/google/url` + `GET /auth/callback` — OAuth2 popup flow for Google Calendar
- `src/services/weather.ts` — Open-Meteo API (no auth), 30-minute in-memory cache
- `src/services/calendar.ts` — googleapis OAuth2 client, auto-persists refreshed tokens
- `src/db.ts` — key-value SQLite store via `better-sqlite3` (`data/dashboard.db`)

**Frontend** (`src/components/ConfigForm.tsx`): Single React form for lat/lon + Google Calendar OAuth connection. Clipboard paste handler splits `"lat,lon"` format automatically.

**Error handling:** On HTTP 500, the sketch renders a partial display update in the bottom-right corner rather than clearing the screen. The display retains its last content if the server is unreachable.

**Deep sleep:** `setup()` contains all logic; `loop()` is intentionally empty. The ESP32 enters deep sleep at the end of `setup()` for `refreshRateMinutes`.

## Key Reference Files

- `SPEC.md` — full feature specification
- `ARCHITECTURE.md` — detailed system design with data flow diagrams
- `example/Arduino/` — Waveshare reference implementations
