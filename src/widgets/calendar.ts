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

const CALENDAR_NAMES = [
  'Personal',
  'Public Robotics',
  'Issaquah - 2026 Outdoor Track and Field',
];

async function fetchCalendarEvents(): Promise<CalendarData | null> {
  const accessToken  = getConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN);
  const refreshToken = getConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN);

  if (!accessToken || !refreshToken) return null;

  const expiryStr  = getConfig(DB_KEYS.GOOGLE_TOKEN_EXPIRY);
  const expiryDate = expiryStr ? parseInt(expiryStr, 10) : undefined;

  const oauth2Client = buildOAuthClient();
  oauth2Client.setCredentials({ access_token: accessToken, refresh_token: refreshToken, expiry_date: expiryDate });

  const calendar = google.calendar({ version: 'v3', auth: oauth2Client });

  const startOfDay = new Date();
  startOfDay.setHours(0, 0, 0, 0);

  const endOfDay = new Date();
  endOfDay.setHours(23, 59, 59, 999);

  // Resolve calendar IDs by name (primary is always available)
  const calListResponse = await calendar.calendarList.list();
  const calList = calListResponse.data.items ?? [];

  const calendarIds: string[] = [];
  for (const entry of calList) {
    if (
      entry.id &&
      entry.summary &&
      CALENDAR_NAMES.some(name => entry.summary!.trim() === name)
    ) {
      calendarIds.push(entry.id);
    }
  }

  // Always include primary as fallback
  if (calendarIds.length === 0) calendarIds.push('primary');

  // Fetch from all matched calendars in parallel
  const results = await Promise.allSettled(
    calendarIds.map(id =>
      calendar.events.list({
        calendarId:   id,
        timeMin:      startOfDay.toISOString(),
        timeMax:      endOfDay.toISOString(),
        singleEvents: true,
        orderBy:      'startTime',
        maxResults:   20,
      })
    )
  );

  // Merge and sort all events by start time
  const allItems = results.flatMap(r =>
    r.status === 'fulfilled' ? (r.value.data.items ?? []) : []
  );

  allItems.sort((a, b) => {
    const aTime = a.start?.dateTime ?? a.start?.date ?? '';
    const bTime = b.start?.dateTime ?? b.start?.date ?? '';
    return aTime.localeCompare(bTime);
  });

  const events: CalendarEvent[] = allItems.slice(0, 3).map(event => {
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

  return { events, totalCount: allItems.length };
}

export const calendarWidget: Widget<CalendarData | null> = {
  id: 'calendar',
  fetch: fetchCalendarEvents,
};
