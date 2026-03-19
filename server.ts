import 'dotenv/config';
import express from 'express';
import { createServer as createViteServer } from 'vite';
import { configRouter } from './src/routes/config.js';
import { dashboardRouter } from './src/routes/dashboard.js';
import { authRouter } from './src/routes/auth.js';
import { errorHandler } from './src/middleware/errorHandler.js';

async function startServer() {
  const app = express();
  const PORT = parseInt(process.env.PORT || '3000', 10);

  app.use(express.json());

  app.use(configRouter);
  app.use(dashboardRouter);
  app.use(authRouter);
  app.use(errorHandler);

  if (process.env.NODE_ENV === 'production') {
    app.use(express.static('dist'));
    app.get('*', (_req, res) => res.sendFile('dist/index.html', { root: '.' }));
  } else {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  }

  app.listen(PORT, '0.0.0.0', () => {
    console.log(`Server running on http://localhost:${PORT}`);
  });
}

startServer();
