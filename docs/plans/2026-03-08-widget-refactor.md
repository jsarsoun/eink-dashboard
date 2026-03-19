# Widget Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Refactor the eInk Dashboard backend into a widget module system, fixing code quality issues and making new data sources trivial to add.

**Architecture:** Each data source implements a `Widget<T>` interface with an `id` and a self-contained `fetch()` method. A central registry array drives the dashboard route, which uses `Promise.allSettled` so one failing widget never kills the whole response.

**Tech Stack:** Node.js, Express, TypeScript (strict), better-sqlite3, googleapis, tsx

---

## Verification Command
After every task run:
```bash
npm run lint
```
Expected: zero errors. This is the type-level test for this refactor — behavior is unchanged, TypeScript catches structural mistakes.

---

### Task 1: DB Key Constants

**Goal:** Eliminate raw string DB keys scattered across files.

**Files:**
- Create: `src/constants/dbKeys.ts`
- Modify: `src/db.ts`
- Modify: `src/routes/config.ts`
- Modify: `src/routes/auth.ts`
- Modify: `src/services/calendar.ts`

**Step 1: Create the constants file**

```typescript
// src/constants/dbKeys.ts
export const DB_KEYS = {
  LAT:                  'location_lat',
  LON:                  'location_lon',
  GOOGLE_ACCESS_TOKEN:  'google_access_token',
  GOOGLE_REFRESH_TOKEN: 'google_refresh_token',
  GOOGLE_TOKEN_EXPIRY:  'google_token_expiry',
  OAUTH_STATE:          'oauth_state',
} as const;
```

**Step 2: Update `src/db.ts`**

Replace the `defaults` object (lines 19–26) with imports from DB_KEYS:

```typescript
import { DB_KEYS } from './constants/dbKeys.js';

// replace the defaults object
const defaults: Record<string, string> = {
  [DB_KEYS.LAT]:                 '',
  [DB_KEYS.LON]:                 '',
  [DB_KEYS.GOOGLE_ACCESS_TOKEN]: '',
  [DB_KEYS.GOOGLE_REFRESH_TOKEN]:'',
  [DB_KEYS.GOOGLE_TOKEN_EXPIRY]: '',
  [DB_KEYS.OAUTH_STATE]:         '',
};
```

**Step 3: Update `src/routes/config.ts`**

Replace raw string keys with DB_KEYS:

```typescript
import { DB_KEYS } from '../constants/dbKeys.js';

// in GET handler:
const locationLat  = getConfig(DB_KEYS.LAT);
const locationLon  = getConfig(DB_KEYS.LON);
const accessToken  = getConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN);
const refreshToken = getConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN);

// in PUT handler:
if (locationLat !== undefined) setConfig(DB_KEYS.LAT, String(locationLat));
if (locationLon !== undefined) setConfig(DB_KEYS.LON, String(locationLon));
```

**Step 4: Update `src/routes/auth.ts`**

```typescript
import { DB_KEYS } from '../constants/dbKeys.js';

// replace string literals:
setConfig(DB_KEYS.OAUTH_STATE, state);
// ...
const savedState = getConfig(DB_KEYS.OAUTH_STATE);
// ...
if (tokens.access_token)  setConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN,  tokens.access_token);
if (tokens.refresh_token) setConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN, tokens.refresh_token);
if (tokens.expiry_date)   setConfig(DB_KEYS.GOOGLE_TOKEN_EXPIRY,  String(tokens.expiry_date));
setConfig(DB_KEYS.OAUTH_STATE, '');
```

**Step 5: Update `src/services/calendar.ts`**

```typescript
import { DB_KEYS } from '../constants/dbKeys.js';

// replace string literals:
const accessToken  = getConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN);
const refreshToken = getConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN);
// ...
if (tokens.access_token) setConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN, tokens.access_token);
if (tokens.expiry_date)  setConfig(DB_KEYS.GOOGLE_TOKEN_EXPIRY,  String(tokens.expiry_date));
```

**Step 6: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 2: Shared OAuth Client Utility

**Goal:** Eliminate the duplicate `buildOAuthClient()` function in `auth.ts` and `services/calendar.ts`.

**Files:**
- Create: `src/utils/oauth.ts`
- Modify: `src/routes/auth.ts`
- Modify: `src/services/calendar.ts`

**Step 1: Create the shared utility**

```typescript
// src/utils/oauth.ts
import { google } from 'googleapis';
import { setConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';

export function buildOAuthClient() {
  const oauth2Client = new google.auth.OAuth2(
    process.env.GOOGLE_CLIENT_ID,
    process.env.GOOGLE_CLIENT_SECRET,
  );

  // Persist auto-refreshed tokens to DB
  oauth2Client.on('tokens', (tokens) => {
    if (tokens.access_token) setConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN, tokens.access_token);
    if (tokens.expiry_date)  setConfig(DB_KEYS.GOOGLE_TOKEN_EXPIRY,  String(tokens.expiry_date));
  });

  return oauth2Client;
}
```

**Step 2: Update `src/routes/auth.ts`**

Remove the local `buildOAuthClient` function (lines 13–18) and add the import:

```typescript
import { buildOAuthClient } from '../utils/oauth.js';
```

The rest of `auth.ts` stays identical — it already calls `buildOAuthClient()`.

**Step 3: Update `src/services/calendar.ts`**

Remove the local `buildOAuthClient` function (lines 15–28) and add the import:

```typescript
import { buildOAuthClient } from '../utils/oauth.js';
```

**Step 4: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 3: Widget Interface

**Goal:** Define the `Widget<T>` TypeScript interface that all data sources implement.

**Files:**
- Create: `src/widgets/types.ts`

**Step 1: Create the interface**

```typescript
// src/widgets/types.ts
export interface Widget<T = unknown> {
  /** Unique key used in the dashboard JSON response (e.g. "weather", "calendar") */
  id: string;

  /**
   * Fetch this widget's data. Reads its own config from the DB directly.
   * Throws on failure — the dashboard route uses Promise.allSettled to handle this gracefully.
   */
  fetch(): Promise<T>;
}
```

**Step 2: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 4: Weather Widget

**Goal:** Move `src/services/weather.ts` into the widget system.

**Files:**
- Create: `src/widgets/weather.ts`

**Step 1: Create the weather widget**

Copy `src/services/weather.ts` in full, then add the widget export at the bottom and a DB import at the top:

```typescript
// src/widgets/weather.ts
import { getConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';
import type { Widget } from './types.js';

export interface WeatherData {
  tempF: number;
  condition: string;
  highF: number;
  lowF: number;
  precipitationPct: number;
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

interface OpenMeteoResponse {
  current: {
    temperature_2m: number;
    weather_code: number;
    precipitation_probability: number;
  };
  daily: {
    temperature_2m_max: number[];
    temperature_2m_min: number[];
    precipitation_probability_max: number[];
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
  url.searchParams.set('daily', 'temperature_2m_max,temperature_2m_min,precipitation_probability_max');
  url.searchParams.set('temperature_unit', 'fahrenheit');
  url.searchParams.set('timezone', 'auto');
  url.searchParams.set('forecast_days', '1');

  const response = await fetch(url.toString());
  if (!response.ok) throw new Error('weather unavailable');

  const json = await response.json() as OpenMeteoResponse;

  const data: WeatherData = {
    tempF:            Math.round(json.current.temperature_2m),
    condition:        WMO_CONDITIONS[json.current.weather_code] ?? 'Unknown',
    highF:            Math.round(json.daily.temperature_2m_max[0]),
    lowF:             Math.round(json.daily.temperature_2m_min[0]),
    precipitationPct: json.daily.precipitation_probability_max[0]
                        ?? json.current.precipitation_probability
                        ?? 0,
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

**Step 2: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 5: Calendar Widget

**Goal:** Move `src/services/calendar.ts` into the widget system.

**Files:**
- Create: `src/widgets/calendar.ts`

**Step 1: Create the calendar widget**

```typescript
// src/widgets/calendar.ts
import { google } from 'googleapis';
import { getConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';
import { buildOAuthClient } from '../utils/oauth.js';
import type { Widget } from './types.js';

export interface CalendarEvent {
  title: string;
  time: string;
  allDay: boolean;
}

export interface CalendarData {
  events: CalendarEvent[];
  totalCount: number;
}

async function fetchCalendarEvents(): Promise<CalendarData | null> {
  const accessToken  = getConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN);
  const refreshToken = getConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN);

  if (!accessToken || !refreshToken) return null;

  const oauth2Client = buildOAuthClient();
  oauth2Client.setCredentials({ access_token: accessToken, refresh_token: refreshToken });

  const calendar = google.calendar({ version: 'v3', auth: oauth2Client });

  const startOfDay = new Date();
  startOfDay.setHours(0, 0, 0, 0);

  const endOfDay = new Date();
  endOfDay.setHours(23, 59, 59, 999);

  const response = await calendar.events.list({
    calendarId:   'primary',
    timeMin:      startOfDay.toISOString(),
    timeMax:      endOfDay.toISOString(),
    singleEvents: true,
    orderBy:      'startTime',
    maxResults:   10,
  });

  const items = response.data.items ?? [];

  const events: CalendarEvent[] = items.slice(0, 3).map(event => {
    const allDay = !event.start?.dateTime;
    const time = allDay
      ? 'All Day'
      : new Date(event.start!.dateTime!).toLocaleTimeString('en-US', {
          hour:   'numeric',
          minute: '2-digit',
          hour12: true,
        });
    return { title: event.summary ?? 'Untitled', time, allDay };
  });

  return { events, totalCount: items.length };
}

export const calendarWidget: Widget<CalendarData | null> = {
  id: 'calendar',
  fetch: fetchCalendarEvents,
};
```

**Step 2: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 6: Widget Registry

**Goal:** Create the central registry that drives the dashboard route.

**Files:**
- Create: `src/widgets/index.ts`

**Step 1: Create the registry**

```typescript
// src/widgets/index.ts
import { weatherWidget } from './weather.js';
import { calendarWidget } from './calendar.js';
import type { Widget } from './types.js';

export const widgets: Widget[] = [weatherWidget, calendarWidget];
```

**Step 2: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 7: Refactor Dashboard Route

**Goal:** Replace the manual parallel fetch with a registry-driven loop using `Promise.allSettled`.

**Files:**
- Modify: `src/routes/dashboard.ts`

**Step 1: Replace the entire file**

```typescript
// src/routes/dashboard.ts
import { Router } from 'express';
import { getConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';
import { widgets } from '../widgets/index.js';

export const dashboardRouter = Router();

dashboardRouter.get('/api/dashboard-data', async (_req, res, next) => {
  try {
    const lat = getConfig(DB_KEYS.LAT);
    const lon = getConfig(DB_KEYS.LON);

    if (!lat || !lon) {
      res.status(400).json({ error: 'location not configured' });
      return;
    }

    const refreshRateMinutes = parseInt(process.env.REFRESH_RATE_MINUTES || '60', 10);

    const results = await Promise.allSettled(widgets.map(w => w.fetch()));
    const widgetData = Object.fromEntries(
      widgets.map((w, i) => [
        w.id,
        results[i].status === 'fulfilled' ? results[i].value : null,
      ])
    );

    const date = new Date().toLocaleDateString('en-US', {
      weekday: 'long',
      month:   'long',
      day:     'numeric',
    });

    res.json({ date, ...widgetData, settings: { refreshRateMinutes } });
  } catch (error) {
    next(error);
  }
});
```

**Step 2: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 8: Input Validation in Config Route

**Goal:** Return a 400 with a clear message if lat/lon values are out of range.

**Files:**
- Modify: `src/routes/config.ts`

**Step 1: Add a validation helper and use it in the PUT handler**

Full updated file:

```typescript
// src/routes/config.ts
import { Router } from 'express';
import { getConfig, setConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';

export const configRouter = Router();

function validateLatLon(lat: string, lon: string): string | null {
  const latNum = parseFloat(lat);
  const lonNum = parseFloat(lon);
  if (isNaN(latNum) || latNum < -90  || latNum > 90)  return 'lat must be between -90 and 90';
  if (isNaN(lonNum) || lonNum < -180 || lonNum > 180) return 'lon must be between -180 and 180';
  return null;
}

configRouter.get('/api/config', (_req, res, next) => {
  try {
    res.json({
      locationLat:       getConfig(DB_KEYS.LAT),
      locationLon:       getConfig(DB_KEYS.LON),
      isGoogleConnected: !!(getConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN) && getConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN)),
    });
  } catch (error) {
    next(error);
  }
});

configRouter.put('/api/config', (req, res, next) => {
  try {
    const { locationLat, locationLon } = req.body as Record<string, string>;

    if (locationLat !== undefined && locationLon !== undefined) {
      const err = validateLatLon(String(locationLat), String(locationLon));
      if (err) {
        res.status(400).json({ error: err });
        return;
      }
    }

    if (locationLat !== undefined) setConfig(DB_KEYS.LAT, String(locationLat));
    if (locationLon !== undefined) setConfig(DB_KEYS.LON, String(locationLon));

    res.json({ success: true });
  } catch (error) {
    next(error);
  }
});
```

**Step 2: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 9: Centralized Error Handler

**Goal:** Replace duplicated `console.error + res.status(500)` in each route with a single middleware.

**Files:**
- Create: `src/middleware/errorHandler.ts`
- Modify: `src/routes/auth.ts`
- Modify: `server.ts`

**Step 1: Create the middleware**

```typescript
// src/middleware/errorHandler.ts
import type { Request, Response, NextFunction } from 'express';

export function errorHandler(err: unknown, _req: Request, res: Response, _next: NextFunction): void {
  const message = err instanceof Error ? err.message : 'Internal server error';
  console.error(err);
  res.status(500).json({ error: message });
}
```

**Step 2: Update `src/routes/auth.ts` to use `next`**

The callback route already has the only try/catch that matters. Replace its catch block:

```typescript
// change function signature to include next:
authRouter.get('/auth/callback', async (req, res, next) => {
  // ... existing logic unchanged ...
  } catch (error) {
    next(error);  // was: console.error(...); res.status(500).send(...)
  }
});
```

**Step 3: Register the error handler in `server.ts`**

Add import and register it as the last middleware, after all routers:

```typescript
import { errorHandler } from './src/middleware/errorHandler.js';

// after app.use(authRouter):
app.use(errorHandler);
```

**Step 4: Verify**

```bash
npm run lint
```
Expected: zero errors.

---

### Task 10: Delete Old Services Directory

**Goal:** Remove the now-replaced `src/services/` folder and confirm the build is clean.

**Files:**
- Delete: `src/services/weather.ts`
- Delete: `src/services/calendar.ts`
- Delete: `src/services/` (directory)

**Step 1: Delete the files**

```bash
rm -rf src/services/
```

**Step 2: Verify lint still passes**

```bash
npm run lint
```
Expected: zero errors (nothing should import from services/ anymore).

**Step 3: Smoke test**

```bash
npm run dev
```

Open `http://localhost:3000` — config UI should load. Hit `http://localhost:3000/api/dashboard-data` — should return `{ error: 'location not configured' }` if no lat/lon saved, or full JSON if it is.

---

## Adding a New Widget (Post-Refactor)

To add e.g. a stocks widget:

1. Create `src/widgets/stocks.ts` implementing `Widget<StocksData>`
2. Add one line to `src/widgets/index.ts`:
   ```typescript
   import { stocksWidget } from './stocks.js';
   export const widgets: Widget[] = [weatherWidget, calendarWidget, stocksWidget];
   ```
3. The dashboard JSON response will automatically include `"stocks": { ... }`
