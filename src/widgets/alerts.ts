import { getConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';
import type { Widget } from './types.js';

export interface AlertData {
  event:    string;
  severity: string;
}

interface AlertCache {
  data:      AlertData | null;
  fetchedAt: number;
  lat:       string;
  lon:       string;
}

const CACHE_TTL_MS   = 15 * 60 * 1000;
const SEVERITY_ORDER = ['Extreme', 'Severe', 'Moderate', 'Minor'];

let cache: AlertCache | null = null;

async function fetchAlerts(lat: string, lon: string): Promise<AlertData | null> {
  const now = Date.now();
  if (cache && now - cache.fetchedAt < CACHE_TTL_MS && cache.lat === lat && cache.lon === lon) return cache.data;

  const url = `https://api.weather.gov/alerts/active?point=${lat},${lon}`;
  const response = await fetch(url, {
    headers: { 'User-Agent': 'eInkDashboard/1.0 (home-display)' },
    signal: AbortSignal.timeout(8000),
  });

  if (!response.ok) { cache = { data: null, fetchedAt: now, lat, lon }; return null; }

  const json = await response.json() as { features: any[] };
  const active = (json.features ?? [])
    .filter((f: any) =>
      f.properties?.status === 'Actual' &&
      f.properties?.messageType !== 'Cancel' &&
      (!f.properties?.expires || new Date(f.properties.expires).getTime() > now)
    )
    .sort((a: any, b: any) => {
      const ai = SEVERITY_ORDER.indexOf(a.properties.severity);
      const bi = SEVERITY_ORDER.indexOf(b.properties.severity);
      return (ai === -1 ? 99 : ai) - (bi === -1 ? 99 : bi);
    });

  const data = active.length === 0 ? null : {
    event:    active[0].properties.event    ?? 'Weather Alert',
    severity: active[0].properties.severity ?? 'Unknown',
  };

  cache = { data, fetchedAt: now, lat, lon };
  return data;
}

export const alertsWidget: Widget<AlertData | null> = {
  id: 'alerts',
  fetch() {
    const lat = getConfig(DB_KEYS.LAT);
    const lon = getConfig(DB_KEYS.LON);
    return fetchAlerts(lat, lon);
  },
};
