# Timestamp & Server-Unreachable Banner Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the "last updated" timestamp to the bottom-right of the e-ink display, show a "server unreachable" banner in that same spot on failure, and pin the timestamp to Pacific time on the server.

**Architecture:** Two files change independently — `src/routes/dashboard.ts` (one-line timezone fix) and `arduino/EInkDashboard/EInkDashboard.ino` (three edits: right-align timestamp, add error display on WiFi failure, update default error string). No new files. No schema or API changes.

**Tech Stack:** Node.js/TypeScript (server), C++ Arduino (GxEPD2, Adafruit GFX, ArduinoJson v7)

---

## Chunk 1: All changes

### Task 1: Fix server timezone

**Files:**
- Modify: `src/routes/dashboard.ts:34-38`

- [ ] **Step 1: Add `timeZone` to `toLocaleTimeString` call**

In `src/routes/dashboard.ts`, find this block (around line 34):
```ts
const updatedAt = now.toLocaleTimeString('en-US', {
  hour:   'numeric',
  minute: '2-digit',
  hour12: true,
});
```
Replace with:
```ts
const updatedAt = now.toLocaleTimeString('en-US', {
  hour:     'numeric',
  minute:   '2-digit',
  hour12:   true,
  timeZone: 'America/Los_Angeles',
});
```

- [ ] **Step 2: Verify TypeScript compiles**

Run: `npm run lint`
Expected: no errors

---

### Task 2: Move timestamp to bottom-right in Arduino sketch

**Files:**
- Modify: `arduino/EInkDashboard/EInkDashboard.ino:414-418`

The current bottom-left block (inside the `do { } while (display.nextPage())` loop in `renderDisplay()`):
```cpp
// ── Last updated timestamp (bottom-left) ─────────────────────────────────
display.setFont(&FreeMonoBold9pt7b);
display.setTextColor(GxEPD_BLACK);
display.setCursor(MARGIN, display.height() - 8);
display.printf("Updated %s", updatedAt);
```

- [ ] **Step 1: Replace with right-aligned version**

Replace the block above with:
```cpp
// ── Last updated timestamp (bottom-right) ────────────────────────────────
display.setFont(&FreeMonoBold9pt7b);
display.setTextColor(GxEPD_BLACK);
char tsBuf[32];
snprintf(tsBuf, sizeof(tsBuf), "Updated %s", updatedAt);
int16_t tx, ty; uint16_t tw, th;
display.getTextBounds(tsBuf, 0, 0, &tx, &ty, &tw, &th);
display.setCursor(display.width() - MARGIN - (int)tw, display.height() - 8);
display.print(tsBuf);
```

- [ ] **Step 2: Compile to verify no errors**

Run:
```
C:/Users/jsars/arduino-cli/arduino-cli.exe compile --fqbn esp32:esp32:esp32 arduino/EInkDashboard/
```
Expected: `Sketch uses ... bytes` — no compile errors.

---

### Task 3: Show error banner on WiFi failure

**Files:**
- Modify: `arduino/EInkDashboard/EInkDashboard.ino:445-448`

The current WiFi-failure block (in `setup()`):
```cpp
if (WiFi.status() != WL_CONNECTED) {
  Serial.println("\nWiFi failed — sleeping");
  deepSleepMinutes(DEFAULT_REFRESH_MINUTES);
  return;
}
```

- [ ] **Step 1: Add `showError()` call before sleeping**

Replace with:
```cpp
if (WiFi.status() != WL_CONNECTED) {
  Serial.println("\nWiFi failed — sleeping");
  showError("server unreachable");
  deepSleepMinutes(DEFAULT_REFRESH_MINUTES);
  return;
}
```

- [ ] **Step 2: Compile**

Run:
```
C:/Users/jsars/arduino-cli/arduino-cli.exe compile --fqbn esp32:esp32:esp32 arduino/EInkDashboard/
```
Expected: no errors.

---

### Task 4: Update default HTTP error message

**Files:**
- Modify: `arduino/EInkDashboard/EInkDashboard.ino:461`

The current line:
```cpp
const char* errorMsg = "server unavailable";
```

- [ ] **Step 1: Change to "server unreachable"**

Replace with:
```cpp
const char* errorMsg = "server unreachable";
```

Leave the JSON-body parse block immediately after it completely unchanged:
```cpp
String body = http.getString();
JsonDocument errorDoc;
if (deserializeJson(errorDoc, body) == DeserializationError::Ok) {
  const char* msg = errorDoc["error"];
  if (msg) errorMsg = msg;
}
```

- [ ] **Step 2: Final compile**

Run:
```
C:/Users/jsars/arduino-cli/arduino-cli.exe compile --fqbn esp32:esp32:esp32 arduino/EInkDashboard/
```
Expected: no errors.

- [ ] **Step 3: Upload to device**

Run:
```
C:/Users/jsars/arduino-cli/arduino-cli.exe upload -p COM7 --fqbn esp32:esp32:esp32 arduino/EInkDashboard/
```
Expected: `Done uploading.`

- [ ] **Step 4: Verify on device**

Open serial monitor:
```
python monitor.py
```
Expected: device connects to WiFi, fetches data, logs `Display updated`, then sleeps. Timestamp `Updated H:MM AM/PM` appears at bottom-right of display.
