# Design: Bottom-Right Timestamp & Server-Unreachable Banner

**Date:** 2026-03-13
**Scope:** `arduino/EInkDashboard/EInkDashboard.ino` only — no server changes required.

---

## Problem

The "last updated" timestamp is currently rendered at the bottom-left of the display. The request is to move it to the bottom-right so it occupies the same visual slot as the existing error banner. When the server is unreachable (WiFi failure or HTTP error), that slot should show a "server unreachable" message instead.

---

## Design

Three targeted edits to `EInkDashboard.ino`:

### 1. Move timestamp to bottom-right (`renderDisplay()`)

Replace the current bottom-left cursor placement:
```cpp
display.setCursor(MARGIN, display.height() - 8);
display.printf("Updated %s", updatedAt);
```

With a right-aligned version using `getTextBounds()`:
```cpp
char tsBuf[32];
snprintf(tsBuf, sizeof(tsBuf), "Updated %s", updatedAt);
int16_t tx, ty; uint16_t tw, th;
display.getTextBounds(tsBuf, 0, 0, &tx, &ty, &tw, &th);
display.setCursor(display.width() - MARGIN - (int)tw, display.height() - 8);
display.print(tsBuf);
```

This ensures the text is flush with the right margin regardless of time string length.

### 2. Show error banner on WiFi failure (`setup()`)

After the WiFi connection loop fails, call `showError()` before sleeping:
```cpp
if (WiFi.status() != WL_CONNECTED) {
    showError("server unreachable");
    deepSleepMinutes(DEFAULT_REFRESH_MINUTES);
    return;
}
```

Previously, WiFi failure silently slept without updating the display.

### 3. Consistent error wording

Change only the **default value** of `errorMsg` in the HTTP-error path from `"server unavailable"` to `"server unreachable"`. The existing JSON-body-fallback logic (which reads a more specific `error` field from the server response) is preserved unchanged:
```cpp
// Only this line changes:
const char* errorMsg = "server unreachable";
// The JSON-body parse block below remains intact:
String body = http.getString();
JsonDocument errorDoc;
if (deserializeJson(errorDoc, body) == DeserializationError::Ok) {
  const char* msg = errorDoc["error"];
  if (msg) errorMsg = msg;
}
```

---

## Layout

The `showError()` partial-window coordinates (`width-260, height-30, 260×24 px`) are unchanged. The new right-aligned timestamp in `renderDisplay()` lands in the same bottom-right region, so normal and error states occupy the same visual slot.

The timestamp string `"Updated 12:34 AM"` (worst case ~17 chars at 9pt ≈ 170px wide) fits comfortably within the 258px available. No truncation is needed. Edit 2 is a **new addition** — the WiFi-failure path previously had no display update before sleeping.

---

### 4. Explicit Pacific timezone in server (`src/routes/dashboard.ts`)

The `updatedAt` field currently uses `toLocaleTimeString()` without a `timeZone` option, inheriting the server process's local timezone. Add `timeZone: 'America/Los_Angeles'` to make it explicit:

```ts
const updatedAt = now.toLocaleTimeString('en-US', {
  hour:     'numeric',
  minute:   '2-digit',
  hour12:   true,
  timeZone: 'America/Los_Angeles',
});
```

---

## Out of scope

- No changes to `showError()` function signature or partial-window geometry.
- No changes to any other Arduino sketch variants (`EInkDashboardS3`, `EInkTest`).
