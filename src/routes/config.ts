import { Router } from 'express';
import { getConfig, setConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';

export const configRouter = Router();

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

    if (locationLat !== undefined) {
      const latNum = parseFloat(String(locationLat));
      if (isNaN(latNum) || latNum < -90 || latNum > 90) {
        res.status(400).json({ error: 'lat must be between -90 and 90' });
        return;
      }
      setConfig(DB_KEYS.LAT, String(latNum));
    }

    if (locationLon !== undefined) {
      const lonNum = parseFloat(String(locationLon));
      if (isNaN(lonNum) || lonNum < -180 || lonNum > 180) {
        res.status(400).json({ error: 'lon must be between -180 and 180' });
        return;
      }
      setConfig(DB_KEYS.LON, String(lonNum));
    }

    res.json({ success: true });
  } catch (error) {
    next(error);
  }
});
