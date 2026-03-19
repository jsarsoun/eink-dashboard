# Widget Refactor Design
**Date:** 2026-03-08
**Status:** Approved

## Goal
Refactor the eInk Dashboard backend into a widget module system to clean up existing code quality issues and make adding new data sources (stocks, RSS, sports scores, etc.) simple and consistent.

## Two Tracks

### Track 1 — Widget System
Move `services/weather.ts` and `services/calendar.ts` into `src/widgets/`. Each widget implements a shared `Widget<T>` interface. A registry file exports the active widget list. The dashboard route iterates the registry.

### Track 2 — Cleanup
1. Extract duplicate `buildOAuthClient()` to `src/utils/oauth.ts`
2. Move all DB key strings to `src/constants/dbKeys.ts`
3. Add centralized Express error handler at `src/middleware/errorHandler.ts`
4. Add lat/lon bounds validation in config route (no new dependencies)

## Widget Interface

```typescript
// src/widgets/types.ts
export interface Widget<T = unknown> {
  id: string
  fetch(db: Database): Promise<T>
}
```

## Widget Registry

```typescript
// src/widgets/index.ts
import { weatherWidget } from './weather'
import { calendarWidget } from './calendar'

export const widgets: Widget[] = [weatherWidget, calendarWidget]
```

## Dashboard Route (post-refactor)

```typescript
const results = await Promise.allSettled(widgets.map(w => w.fetch(db)))
const data = Object.fromEntries(
  widgets.map((w, i) => [w.id, results[i].status === 'fulfilled' ? results[i].value : null])
)
res.json({ ...data, settings: { refreshRateMinutes } })
```

Uses `Promise.allSettled` so one failing widget never returns a 500 — the ESP32 gets partial data.

## Final File Structure

```
src/
  widgets/
    types.ts          — Widget interface
    index.ts          — registry
    weather.ts        — moved + adapted from services/
    calendar.ts       — moved + adapted from services/
  utils/
    oauth.ts          — shared OAuth client builder
  constants/
    dbKeys.ts         — all DB key strings
  middleware/
    errorHandler.ts   — centralized Express error handler
  routes/             — unchanged structure, simplified internals
  components/         — untouched
  db.ts               — untouched
  main.tsx            — untouched
  App.tsx             — untouched
```

## Constraints
- No behavior changes — ESP32 JSON response stays identical
- No new dependencies
- `src/services/` folder replaced by `src/widgets/`
