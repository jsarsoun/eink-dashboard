import { Router, Request, NextFunction } from 'express';
import crypto from 'crypto';
import { getConfig, setConfig } from '../db.js';
import { DB_KEYS } from '../constants/dbKeys.js';
import { buildOAuthClient } from '../utils/oauth.js';

export const authRouter = Router();

const SCOPES = [
  'https://www.googleapis.com/auth/calendar.readonly',
  'https://www.googleapis.com/auth/calendar.events.readonly',
];

function buildRedirectUri(req: Request): string {
  const protocol = req.headers['x-forwarded-proto'] || 'http';
  const host = req.headers.host;
  return `${protocol}://${host}/auth/callback`;
}

authRouter.get('/api/auth/google/url', (req, res) => {
  if (!process.env.GOOGLE_CLIENT_ID || !process.env.GOOGLE_CLIENT_SECRET) {
    res.status(500).json({ error: 'Google OAuth credentials not configured' });
    return;
  }

  const state = crypto.randomBytes(16).toString('hex');
  setConfig(DB_KEYS.OAUTH_STATE, state);

  const oauth2Client = buildOAuthClient();
  const url = oauth2Client.generateAuthUrl({
    access_type: 'offline',
    scope: SCOPES,
    redirect_uri: buildRedirectUri(req),
    state,
    prompt: 'consent',
  });

  res.json({ url });
});

authRouter.delete('/api/auth/google', (req, res) => {
  setConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN, '');
  setConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN, '');
  setConfig(DB_KEYS.GOOGLE_TOKEN_EXPIRY, '');
  res.json({ success: true });
});

authRouter.get('/auth/callback', async (req, res, next: NextFunction) => {
  const { code, state } = req.query;

  if (!code) {
    res.status(400).send('No authorization code provided');
    return;
  }

  const savedState = getConfig(DB_KEYS.OAUTH_STATE);
  if (!state || state !== savedState) {
    setConfig(DB_KEYS.OAUTH_STATE, '');
    res.status(400).send('Invalid state parameter');
    return;
  }

  try {
    const redirectUri = buildRedirectUri(req);
    const oauth2Client = buildOAuthClient();
    const { tokens } = await oauth2Client.getToken({ code: String(code), redirect_uri: redirectUri });

    if (tokens.access_token)  setConfig(DB_KEYS.GOOGLE_ACCESS_TOKEN,  tokens.access_token);
    if (tokens.refresh_token) setConfig(DB_KEYS.GOOGLE_REFRESH_TOKEN, tokens.refresh_token);
    if (tokens.expiry_date)   setConfig(DB_KEYS.GOOGLE_TOKEN_EXPIRY,  String(tokens.expiry_date));
    setConfig(DB_KEYS.OAUTH_STATE, '');

    res.send(`
      <html><body>
        <script>
          if (window.opener) {
            window.opener.postMessage({ type: 'OAUTH_SUCCESS' }, '*');
            window.close();
          } else {
            window.location.href = '/';
          }
        </script>
        <p>Authentication successful. You can close this window.</p>
      </body></html>
    `);
  } catch (error) {
    next(error);
  }
});
