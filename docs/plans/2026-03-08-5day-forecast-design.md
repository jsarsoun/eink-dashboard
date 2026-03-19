# 5-Day Forecast Design
**Date:** 2026-03-08
**Status:** Approved

## Goal
Add a 5-day forecast strip under the current weather in the left column of the e-ink display. Each row shows abbreviated day name, high/low temps, and a 16×16 bitmap icon for the weather condition.

## Backend Changes (`src/widgets/weather.ts`)

- Change `forecast_days` from `1` to `6`
- Add `weather_code` and `time` to the `daily` fields in the Open-Meteo request
- Add `ForecastDay` interface and `forecast: ForecastDay[]` to `WeatherData`
- Populate `forecast` from daily indices 1–5 (skip index 0 = today)
- Derive abbreviated day name server-side from the ISO date string (`"2026-03-09"` → `"MON"`)
- Send raw WMO code — icon mapping stays on the Arduino

```typescript
export interface ForecastDay {
  day: string;       // "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
  high: number;
  low: number;
  weatherCode: number;
}
// WeatherData gains: forecast: ForecastDay[]
```

## Arduino Changes (`arduino/EInkDashboard/EInkDashboard.ino`)

### Bitmaps
8 × 16×16 1-bit bitmaps in PROGMEM:
- sun, partCloud, cloud, fog, drizzle, rain, snow, storm

### Icon mapping
`getWeatherIcon(int wmoCode)` returns `const uint8_t*` pointer:
- 0,1 → sun
- 2 → partCloud
- 3 → cloud
- 45,48 → fog
- 51,53,55 → drizzle
- 61,63,65,80,81,82 → rain
- 71,73,75,77,85,86 → snow
- 95,96,99 → storm

### Layout
- Thin separator line above forecast strip at y≈338
- 5 rows, 27px each, starting at y=338
- Per row: icon (16×16) at x=MARGIN, day (9pt) at x=MARGIN+22, high/low (9pt) at x=MARGIN+80
- Format: `"MON"` + `"50/38"`

### JSON parsing
Read `doc["weather"]["forecast"]` as `JsonArray`, iterate up to 5 entries.

### renderDisplay signature
Add `JsonArray forecast` parameter.

## Constraints
- No new dependencies
- Left column only — calendar column untouched
- ESP32 JSON response shape changes (adds `forecast` array) but is backward-compatible
