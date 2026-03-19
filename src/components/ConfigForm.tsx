import React, { useState, useEffect } from 'react';
import { Save, Loader2, Calendar, CheckCircle } from 'lucide-react';

interface Config {
  locationLat: string;
  locationLon: string;
  isGoogleConnected: boolean;
}

type SaveState = 'idle' | 'saving' | 'saved' | 'error';

export default function ConfigForm() {
  const [config, setConfig] = useState<Config>({
    locationLat: '',
    locationLon: '',
    isGoogleConnected: false,
  });
  const [loading, setLoading]     = useState(true);
  const [saveState, setSaveState] = useState<SaveState>('idle');

  useEffect(() => {
    fetchConfig();

    const handleMessage = (event: MessageEvent) => {
      if (event.origin !== window.location.origin) return;
      if (event.data?.type === 'OAUTH_SUCCESS') fetchConfig();
    };

    window.addEventListener('message', handleMessage);
    return () => window.removeEventListener('message', handleMessage);
  }, []);

  const fetchConfig = async () => {
    setLoading(true);
    try {
      const res = await fetch('/api/config');
      if (res.ok) setConfig(await res.json());
    } catch (err) {
      console.error('Failed to load config', err);
    } finally {
      setLoading(false);
    }
  };

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value } = e.target;
    setConfig(prev => ({ ...prev, [name]: value }));
  };

  const handleLatPaste = (e: React.ClipboardEvent<HTMLInputElement>) => {
    const text = e.clipboardData.getData('text').trim();
    const parts = text.split(',').map(s => s.trim());
    if (parts.length === 2 && parts.every(p => p !== '' && !isNaN(Number(p)))) {
      e.preventDefault();
      setConfig(prev => ({ ...prev, locationLat: parts[0], locationLon: parts[1] }));
    }
  };

  const handleConnectGoogle = async () => {
    try {
      const res = await fetch('/api/auth/google/url');
      if (!res.ok) throw new Error('Failed to get auth URL');
      const { url } = await res.json() as { url: string };
      const w = 600, h = 700;
      const left = window.screen.width  / 2 - w / 2;
      const top  = window.screen.height / 2 - h / 2;
      window.open(url, 'google_oauth', `width=${w},height=${h},top=${top},left=${left}`);
    } catch (err) {
      console.error('Failed to initiate Google connection', err);
      alert('Failed to connect to Google. Check server configuration.');
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setSaveState('saving');
    try {
      const res = await fetch('/api/config', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          locationLat: config.locationLat,
          locationLon: config.locationLon,
        }),
      });
      setSaveState(res.ok ? 'saved' : 'error');
      if (res.ok) setTimeout(() => setSaveState('idle'), 2500);
    } catch (err) {
      console.error('Failed to save config', err);
      setSaveState('error');
    }
  };

  if (loading) {
    return (
      <div className="flex justify-center p-8">
        <Loader2 className="animate-spin text-neutral-400" />
      </div>
    );
  }

  return (
    <form onSubmit={handleSubmit} className="space-y-6">

      {/* Location */}
      <div>
        <h2 className="text-sm font-medium text-neutral-900 mb-3">Location</h2>
        <div className="grid grid-cols-2 gap-4">
          <div>
            <label className="block text-xs font-medium text-neutral-500 mb-1">Latitude</label>
            <input
              type="text"
              name="locationLat"
              value={config.locationLat}
              onChange={handleChange}
              onPaste={handleLatPaste}
              placeholder="e.g. 47.6062"
              className="w-full px-3 py-2 border border-neutral-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-neutral-900"
            />
          </div>
          <div>
            <label className="block text-xs font-medium text-neutral-500 mb-1">Longitude</label>
            <input
              type="text"
              name="locationLon"
              value={config.locationLon}
              onChange={handleChange}
              placeholder="e.g. -122.3321"
              className="w-full px-3 py-2 border border-neutral-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-neutral-900"
            />
          </div>
        </div>
      </div>

      {/* Google Calendar */}
      <div className="border-t border-neutral-200 pt-6">
        <h2 className="text-sm font-medium text-neutral-900 mb-3">Integrations</h2>
        <div className="flex items-center justify-between p-4 bg-neutral-50 rounded-lg border border-neutral-200">
          <div className="flex items-center gap-3">
            <div className="p-2 bg-white rounded-md border border-neutral-200">
              <Calendar className="w-5 h-5 text-blue-600" />
            </div>
            <div>
              <p className="text-sm font-medium text-neutral-900">Google Calendar</p>
              <p className="text-xs text-neutral-500">
                {config.isGoogleConnected ? 'Connected' : 'Connect to show daily events'}
              </p>
            </div>
          </div>
          {config.isGoogleConnected ? (
            <div className="flex items-center gap-2">
              <div className="flex items-center gap-1.5 text-green-600 text-sm font-medium">
                <CheckCircle className="w-4 h-4" />
                <span>Connected</span>
              </div>
              <button
                type="button"
                onClick={handleConnectGoogle}
                className="px-3 py-1.5 text-sm font-medium text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50 transition-colors"
              >
                Reconnect
              </button>
            </div>
          ) : (
            <button
              type="button"
              onClick={handleConnectGoogle}
              className="px-3 py-1.5 text-sm font-medium text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50 transition-colors"
            >
              Connect
            </button>
          )}
        </div>
      </div>

      {/* Save */}
      <div className="pt-2">
        <button
          type="submit"
          disabled={saveState === 'saving'}
          className="w-full flex items-center justify-center gap-2 bg-neutral-900 text-white py-2 px-4 rounded-md hover:bg-neutral-800 transition-colors disabled:opacity-50"
        >
          {saveState === 'saving'
            ? <><Loader2 className="w-4 h-4 animate-spin" /> Saving...</>
            : saveState === 'saved'
            ? <><CheckCircle className="w-4 h-4" /> Saved</>
            : saveState === 'error'
            ? <><Save className="w-4 h-4" /> Save Failed — Try Again</>
            : <><Save className="w-4 h-4" /> Save Changes</>
          }
        </button>
      </div>

    </form>
  );
}
