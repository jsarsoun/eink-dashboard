# 5-Day Forecast Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a 5-day forecast strip under today's weather showing abbreviated day name, high/low temps, and a 16×16 bitmap icon per condition.

**Architecture:** Backend fetches 6 forecast days from Open-Meteo (index 0 = today, 1-5 = forecast), adds a `forecast: ForecastDay[]` array to `WeatherData`. Arduino defines 8 PROGMEM bitmaps, maps WMO codes to icons, and renders 5 compact rows in the left column below the existing weather data.

**Tech Stack:** TypeScript/Node.js (backend), C++/Arduino (ESP32), GxEPD2 + Adafruit GFX (e-ink), ArduinoJson v7

---

## Verification Command (backend)
```bash
cd /c/Users/jsars/Programming/Arduino/eInk && npm run lint
```
Expected: zero errors after each backend task.

## Verification Command (Arduino)
```bash
cd /c/Users/jsars/Programming/Arduino/eInk && ./arduino-cli.exe compile --fqbn esp32:esp32:esp32-wroom-da arduino/EInkDashboard/
```
Expected: zero errors after each Arduino task.

---

### Task 1: Update Weather Widget — Backend

**Goal:** Add `ForecastDay` interface, extend Open-Meteo request to 6 days with daily `weather_code`, populate `forecast[]` array for indices 1–5.

**Files:**
- Modify: `src/widgets/weather.ts`

**Step 1: Read the current file**
```bash
cat src/widgets/weather.ts
```

**Step 2: Replace `src/widgets/weather.ts` entirely with:**

```typescript
import { getConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';
import type { Widget } from './types.js';

export interface ForecastDay {
  day: string;         // "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
  high: number;
  low: number;
  weatherCode: number; // raw WMO code — icon mapping lives on the Arduino
}

export interface WeatherData {
  tempF: number;
  condition: string;
  highF: number;
  lowF: number;
  precipitationPct: number;
  forecast: ForecastDay[];
}

interface WeatherCache {
  data: WeatherData;
  fetchedAt: number;
}

const CACHE_TTL_MS = 30 * 60 * 1000;

let cache: WeatherCache | null = null;

const WMO_CONDITIONS: Record<number, string> = {
  0:  'Clear Sky',
  1:  'Mainly Clear',
  2:  'Partly Cloudy',
  3:  'Overcast',
  45: 'Fog',
  48: 'Fog',
  51: 'Drizzle',
  53: 'Drizzle',
  55: 'Heavy Drizzle',
  61: 'Rain',
  63: 'Rain',
  65: 'Heavy Rain',
  71: 'Snow',
  73: 'Snow',
  75: 'Heavy Snow',
  77: 'Snow Grains',
  80: 'Showers',
  81: 'Showers',
  82: 'Heavy Showers',
  85: 'Snow Showers',
  86: 'Snow Showers',
  95: 'Thunderstorm',
  96: 'Thunderstorm',
  99: 'Thunderstorm',
};

const DAY_NAMES = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'] as const;

// Add T12:00:00 to avoid UTC midnight rolling back to the prior day in local time
function dayAbbrev(isoDate: string): string {
  return DAY_NAMES[new Date(isoDate + 'T12:00:00').getDay()];
}

interface OpenMeteoResponse {
  current: {
    temperature_2m: number;
    weather_code: number;
    precipitation_probability: number;
  };
  daily: {
    time: string[];
    temperature_2m_max: number[];
    temperature_2m_min: number[];
    precipitation_probability_max: number[];
    weather_code: number[];
  };
}

async function fetchWeatherData(lat: string, lon: string): Promise<WeatherData> {
  const now = Date.now();

  if (cache && now - cache.fetchedAt < CACHE_TTL_MS) {
    return cache.data;
  }

  const url = new URL('https://api.open-meteo.com/v1/forecast');
  url.searchParams.set('latitude', lat);
  url.searchParams.set('longitude', lon);
  url.searchParams.set('current', 'temperature_2m,weather_code,precipitation_probability');
  url.searchParams.set('daily', 'temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code');
  url.searchParams.set('temperature_unit', 'fahrenheit');
  url.searchParams.set('timezone', 'auto');
  url.searchParams.set('forecast_days', '6');

  const response = await fetch(url.toString());
  if (!response.ok) throw new Error('weather unavailable');

  const json = await response.json() as OpenMeteoResponse;

  // Indices 1-5 are the 5 forecast days (index 0 = today)
  const forecast: ForecastDay[] = json.daily.time.slice(1, 6).map((isoDate, i) => ({
    day:         dayAbbrev(isoDate),
    high:        Math.round(json.daily.temperature_2m_max[i + 1]),
    low:         Math.round(json.daily.temperature_2m_min[i + 1]),
    weatherCode: json.daily.weather_code[i + 1],
  }));

  const data: WeatherData = {
    tempF:            Math.round(json.current.temperature_2m),
    condition:        WMO_CONDITIONS[json.current.weather_code] ?? 'Unknown',
    highF:            Math.round(json.daily.temperature_2m_max[0]),
    lowF:             Math.round(json.daily.temperature_2m_min[0]),
    precipitationPct: json.daily.precipitation_probability_max[0]
                        ?? json.current.precipitation_probability
                        ?? 0,
    forecast,
  };

  cache = { data, fetchedAt: now };
  return data;
}

export const weatherWidget: Widget<WeatherData> = {
  id: 'weather',
  fetch() {
    const lat = getConfig(DB_KEYS.LAT);
    const lon = getConfig(DB_KEYS.LON);
    return fetchWeatherData(lat, lon);
  },
};
```

**Step 3: Verify**
```bash
cd /c/Users/jsars/Programming/Arduino/eInk && npm run lint
```
Expected: zero errors.

**Step 4: Smoke-test the endpoint**
```bash
curl -s http://localhost:3000/api/dashboard-data | python -m json.tool | grep -A 30 '"forecast"'
```
Expected: 5-element array with `day`, `high`, `low`, `weatherCode` fields.

---

### Task 2: Arduino — Bitmap Icons + Icon Mapper

**Goal:** Add 8 PROGMEM 16×16 bitmaps and a `getWeatherIcon()` function that maps WMO codes to the correct bitmap pointer.

**Files:**
- Modify: `arduino/EInkDashboard/EInkDashboard.ino` (insert after the `#include` block, before the user-config section)

**Step 1: Read the current sketch**

Read `C:\Users\jsars\Programming\Arduino\eInk\arduino\EInkDashboard\EInkDashboard.ino`

**Step 2: Insert the following block after line 19 (after the last `#include`) and before the `// ── User configuration` comment:**

```cpp
// ── Weather condition bitmaps (16×16 px, 1-bit, MSB first, PROGMEM) ──────────
// Bit layout: byte[2*row] bit7 = col0, bit0 = col7; byte[2*row+1] bit7 = col8, bit0 = col15

// ☀  Sun: filled circle (cols 5-9, rows 4-8 core) with N/S/E/W rays
static const uint8_t PROGMEM icon_sun[] = {
  0x00,0x00,  // row  0
  0x01,0x00,  // row  1: top ray col 7
  0x01,0x00,  // row  2: top ray col 7
  0x00,0x00,  // row  3
  0x03,0x80,  // row  4: circle top cols 6-8
  0x07,0xC0,  // row  5: circle cols 5-9
  0xC7,0xC6,  // row  6: E/W rays (cols 0-1, 13-14) + circle cols 5-9
  0x07,0xC0,  // row  7: circle cols 5-9
  0x03,0x80,  // row  8: circle bottom cols 6-8
  0x00,0x00,  // row  9
  0x01,0x00,  // row 10: bottom ray col 7
  0x01,0x00,  // row 11: bottom ray col 7
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ⛅  Partly cloudy: tiny sun upper-left + cloud lower-right
static const uint8_t PROGMEM icon_partCloud[] = {
  0x20,0x00,  // row  0: sun top ray col 2
  0x70,0x00,  // row  1: sun body cols 1-3
  0x20,0x00,  // row  2: sun bottom col 2
  0x00,0x00,  // row  3
  0x03,0x80,  // row  4: cloud top cols 6-8
  0x07,0xE0,  // row  5: cloud cols 5-10
  0x0F,0xF8,  // row  6: cloud cols 4-12
  0x1F,0xFC,  // row  7: cloud cols 3-13
  0x1F,0xFC,  // row  8: cloud cols 3-13
  0x0F,0xF8,  // row  9: cloud bottom cols 4-12
  0x00,0x00,  // row 10
  0x00,0x00,  // row 11
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ☁  Cloud / Overcast
static const uint8_t PROGMEM icon_cloud[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cols 4-6
  0x1F,0x80,  // row  2: cols 3-8
  0x3F,0xE0,  // row  3: cols 2-10
  0x7F,0xF0,  // row  4: cols 1-11
  0x7F,0xF0,  // row  5: cols 1-11
  0x3F,0xE0,  // row  6: cols 2-10
  0x00,0x00,  // row  7
  0x00,0x00,  // row  8
  0x00,0x00,  // row  9
  0x00,0x00,  // row 10
  0x00,0x00,  // row 11
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// 🌫  Fog: 4 horizontal bands
static const uint8_t PROGMEM icon_fog[] = {
  0x00,0x00,  // row  0
  0x00,0x00,  // row  1
  0x00,0x00,  // row  2
  0x7F,0xFE,  // row  3: cols 1-14
  0x00,0x00,  // row  4
  0x7F,0xFE,  // row  5: cols 1-14
  0x00,0x00,  // row  6
  0x7F,0xFE,  // row  7: cols 1-14
  0x00,0x00,  // row  8
  0x7F,0xFE,  // row  9: cols 1-14
  0x00,0x00,  // row 10
  0x00,0x00,  // row 11
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// 🌦  Drizzle: cloud + 3 sparse scattered drops
static const uint8_t PROGMEM icon_drizzle[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x00,0x00,  // row  8
  0x22,0x20,  // row  9: drops cols 2, 6, 10
  0x00,0x00,  // row 10
  0x22,0x20,  // row 11: drops cols 2, 6, 10
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// 🌧  Rain: cloud + 3 vertical rain lines
static const uint8_t PROGMEM icon_rain[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x22,0x20,  // row  8: rain cols 2, 6, 10
  0x22,0x20,  // row  9
  0x22,0x20,  // row 10
  0x22,0x20,  // row 11
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ❄  Snow: cloud + asterisk snowflake
static const uint8_t PROGMEM icon_snow[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x04,0x00,  // row  8: snowflake N spoke col 5
  0x3F,0x80,  // row  9: snowflake horizontal bar cols 2-8
  0x15,0x00,  // row 10: snowflake diagonals cols 3,5,7
  0x3F,0x80,  // row 11: snowflake horizontal bar
  0x04,0x00,  // row 12: snowflake S spoke col 5
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ⛈  Storm: cloud + lightning bolt
static const uint8_t PROGMEM icon_storm[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x03,0x80,  // row  8: bolt top cols 6-8
  0x07,0x00,  // row  9: bolt cols 5-7
  0x1F,0x80,  // row 10: bolt wide cols 3-8
  0x0E,0x00,  // row 11: bolt cols 4-6
  0x1C,0x00,  // row 12: bolt tip cols 3-5
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// Maps WMO weather code to the correct PROGMEM bitmap
const uint8_t* getWeatherIcon(int code) {
  if (code <= 1)                                      return icon_sun;
  if (code == 2)                                      return icon_partCloud;
  if (code == 3)                                      return icon_cloud;
  if (code == 45 || code == 48)                       return icon_fog;
  if (code >= 51 && code <= 55)                       return icon_drizzle;
  if ((code >= 61 && code <= 65) || (code >= 80 && code <= 82)) return icon_rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)   return icon_snow;
  return icon_storm; // 95, 96, 99
}
```

**Step 3: Compile**
```bash
cd /c/Users/jsars/Programming/Arduino/eInk && ./arduino-cli.exe compile --fqbn esp32:esp32:esp32-wroom-da arduino/EInkDashboard/
```
Expected: zero errors.

---

### Task 3: Arduino — Forecast Rendering + JSON Parsing

**Goal:** Add `drawForecastRow()` helper, extend `renderDisplay()` to accept and render the 5-day forecast, update `setup()` to parse and pass `forecast` from JSON.

**Files:**
- Modify: `arduino/EInkDashboard/EInkDashboard.ino`

**Step 1: Read the current sketch to confirm current line numbers**

**Step 2: Add `drawForecastRow()` helper immediately before `renderDisplay()`**

Insert this function just before the `// ── Render ─` comment:

```cpp
// ── Forecast row helper ───────────────────────────────────────────────────────
// Draws one forecast row: 16×16 icon, abbreviated day name, and high/low temps.
// y is the text baseline; the icon is top-aligned to y-14.
void drawForecastRow(int y, const char* day, int high, int low, int wmoCode) {
  display.drawBitmap(MARGIN, y - 14, getWeatherIcon(wmoCode), 16, 16, GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(MARGIN + 20, y);
  display.print(day);
  display.setCursor(MARGIN + 62, y);
  display.printf("%d/%d", high, low);
}
```

**Step 3: Update `renderDisplay()` signature to include `JsonArray forecast`**

Change:
```cpp
void renderDisplay(
  const char* date,
  int tempF, const char* condition, int highF, int lowF, int precipPct,
  JsonArray events, int totalEventCount
) {
```

To:
```cpp
void renderDisplay(
  const char* date,
  int tempF, const char* condition, int highF, int lowF, int precipPct,
  JsonArray forecast,
  JsonArray events, int totalEventCount
) {
```

**Step 4: Add forecast rendering inside the `do { ... } while (display.nextPage())` block**

After the existing Rain line (`display.printf("Rain: %d%%", precipPct);`) and before the `// ── Calendar` comment, insert:

```cpp
    // ── 5-Day Forecast ───────────────────────────────────────────────────────
    display.drawLine(MARGIN, 337, display.width() / 2 - MARGIN, 337, GxEPD_BLACK);

    int forecastY = 357;
    int fIdx = 0;
    for (JsonObject fDay : forecast) {
      if (fIdx >= 5) break;
      const char* fName = fDay["day"]         | "---";
      int         fHigh = fDay["high"]        | 0;
      int         fLow  = fDay["low"]         | 0;
      int         fWmo  = fDay["weatherCode"] | 0;
      drawForecastRow(forecastY, fName, fHigh, fLow, fWmo);
      forecastY += 22;
      fIdx++;
    }
```

**Step 5: Update `setup()` — parse `forecast` from JSON**

In `setup()`, after the existing field parsing (around the current `JsonArray events = ...` line), add:

```cpp
  JsonArray forecast      = doc["weather"]["forecast"].as<JsonArray>();
```

**Step 6: Update the `renderDisplay()` call in `setup()`**

Change:
```cpp
  renderDisplay(date, tempF, condition, highF, lowF, precipPct, events, totalCount);
```

To:
```cpp
  renderDisplay(date, tempF, condition, highF, lowF, precipPct, forecast, events, totalCount);
```

**Step 7: Compile**
```bash
cd /c/Users/jsars/Programming/Arduino/eInk && ./arduino-cli.exe compile --fqbn esp32:esp32:esp32-wroom-da arduino/EInkDashboard/
```
Expected: zero errors.

**Step 8: Upload**
```bash
cd /c/Users/jsars/Programming/Arduino/eInk && ./arduino-cli.exe upload -p COM7 --fqbn esp32:esp32:esp32-wroom-da arduino/EInkDashboard/
```

**Step 9: Monitor serial output**
```bash
cd /c/Users/jsars/Programming/Arduino/eInk && python monitor.py
```
Expected: `WiFi connected`, `Display updated`, `Sleeping for 60 minutes`. No JSON parse errors.
