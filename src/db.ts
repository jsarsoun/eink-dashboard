import Database from 'better-sqlite3';
import path from 'path';
import fs from 'fs';
import { DB_KEYS } from './constants/dbKeys.js';

const dataDir = path.resolve('data');
if (!fs.existsSync(dataDir)) {
  fs.mkdirSync(dataDir);
}

const db = new Database(path.join(dataDir, 'dashboard.db'));

db.exec(`
  CREATE TABLE IF NOT EXISTS config (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL DEFAULT ''
  );
`);

const defaults: Record<string, string> = {
  [DB_KEYS.LAT]:                  '',
  [DB_KEYS.LON]:                  '',
  [DB_KEYS.GOOGLE_ACCESS_TOKEN]:  '',
  [DB_KEYS.GOOGLE_REFRESH_TOKEN]: '',
  [DB_KEYS.GOOGLE_TOKEN_EXPIRY]:  '',
  [DB_KEYS.OAUTH_STATE]:          '',
};

const insert = db.prepare('INSERT OR IGNORE INTO config (key, value) VALUES (?, ?)');
for (const [key, value] of Object.entries(defaults)) {
  insert.run(key, value);
}

export const getConfig = (key: string): string => {
  const row = db.prepare('SELECT value FROM config WHERE key = ?').get(key) as { value: string } | undefined;
  return row?.value ?? '';
};

export const setConfig = (key: string, value: string): void => {
  db.prepare('INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)').run(key, value);
};

export default db;
