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

    const now = new Date();
    const date = now.toLocaleDateString('en-US', {
      weekday: 'long',
      month:   'long',
      day:     'numeric',
    });
    const updatedAt = now.toLocaleTimeString('en-US', {
      hour:     'numeric',
      minute:   '2-digit',
      hour12:   true,
      timeZone: 'America/Los_Angeles',
    });

    res.json({ date, updatedAt, ...widgetData, settings: { refreshRateMinutes } });
  } catch (error) {
    next(error);
  }
});
