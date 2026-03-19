import { google } from 'googleapis';
import { setConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';

export function buildOAuthClient() {
  const oauth2Client = new google.auth.OAuth2(
    process.env.GOOGLE_CLIENT_ID,
    process.env.GOOGLE_CLIENT_SECRET,
  );

  // Persist auto-refreshed tokens to DB
  oauth2Client.on('tokens', (tokens) => {
    if (tokens.access_token)  setConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN,  tokens.access_token);
    if (tokens.refresh_token) setConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN, tokens.refresh_token);
    if (tokens.expiry_date)   setConfig(DB_KEYS.GOOGLE_TOKEN_EXPIRY,  String(tokens.expiry_date));
  });

  return oauth2Client;
}
