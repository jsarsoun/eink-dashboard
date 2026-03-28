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
  weatherCode: number;
  forecast: ForecastDay[];
}

interface WeatherCache {
  data: WeatherData;
  fetchedAt: number;
  lat: string;
  lon: string;
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

  if (cache && now - cache.fetchedAt < CACHE_TTL_MS && cache.lat === lat && cache.lon === lon) {
    return cache.data;
  }

  const url = new URL('https://api.open-meteo.com/v1/forecast');
  url.searchParams.set('latitude', lat);
  url.searchParams.set('longitude', lon);
  url.searchParams.set('current', 'temperature_2m,weather_code,precipitation_probability');
  url.searchParams.set('daily', 'temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code');
  url.searchParams.set('temperature_unit', 'fahrenheit');
  url.searchParams.set('timezone', 'auto');
  url.searchParams.set('forecast_days', '4');

  let json: OpenMeteoResponse;
  try {
    const response = await fetch(url.toString(), { signal: AbortSignal.timeout(8000) });
    if (!response.ok) throw new Error(`weather API ${response.status}`);
    json = await response.json() as OpenMeteoResponse;
  } catch (err) {
    if (cache) {
      console.warn('Weather fetch failed, serving stale cache:', err);
      return cache.data;
    }
    throw err;
  }

  // Indices 1-3 are the 3 forecast days (index 0 = today)
  const forecast: ForecastDay[] = json.daily.time.slice(1, 4).map((isoDate, i) => ({
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
    weatherCode:      json.current.weather_code,
    forecast,
  };

  cache = { data, fetchedAt: now, lat, lon };
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
