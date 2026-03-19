import { weatherWidget } from './weather.js';
import { calendarWidget } from './calendar.js';
import { alertsWidget } from './alerts.js';
import type { Widget } from './types.js';

export const widgets: Widget[] = [weatherWidget, calendarWidget, alertsWidget];
