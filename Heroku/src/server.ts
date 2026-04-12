import {
  AngularNodeAppEngine,
  createNodeRequestHandler,
  isMainModule,
  writeResponseToNodeResponse,
} from '@angular/ssr/node';
import express from 'express';
import { join } from 'node:path';
import { Pool } from 'pg'; // From 1st DB SSR attempt
import { Request, Response, NextFunction } from 'express';
import { HttpErrorResponse } from '@angular/common/http';


const browserDistFolder = join(import.meta.dirname, '../browser');

const app = express();
const angularApp = new AngularNodeAppEngine();

// From 1st DB SSR attempt
/*
const pool = new Pool({
  connectionString : process.env['DATABASE_URL'],
  //user: 'postgres',
  //database: 'postgres',
  //password : process.env['DATABASE_PASSWORD'],
  //port: 5432,
  //host: 'postgres'
  //ssl: { rejectUnauthorized: false }
});
*/
const cors = require('cors');

/**
 * Example Express Rest API endpoints can be defined here.
 * Uncomment and define endpoints as necessary.
 *
 * Example:
 * ```ts
 * app.get('/api/{*splat}', (req, res) => {
 *   // Handle API request
 * });
 * ```
 */

// Added Attempt for DATABASE via SSR
app.use(express.json()); // For correct form parsing for db
app.use(cors({
    origin: ['http://localhost:4200',
            'https://senior-t-fd5496756068.herokuapp.com'],// Allow only app's origin
    methods: ['GET', 'POST'],//Allow designated RESTful methods
    allowedHeaders: ['Content-Type','Authorization'], // Allow only these headers
    optionsSuccessStatus: 200
    //credentials: true // Later for auth. found may be part of that
}));

/**
 * Serve static files from /browser
 */
app.use(
  express.static(browserDistFolder, {
    maxAge: '1y',
    index: false,
    redirect: false,
  }),
);

/**
 * Handle all other requests by rendering the Angular application.
 */
app.use((req, res, next) => {
  angularApp
    .handle(req)
    .then((response) =>
      response ? writeResponseToNodeResponse(response, res) : next(),
    )
    .catch(next);
});

/**
 * Start the server if this module is the main entry point, or it is ran via PM2.
 * The server listens on the port defined by the `PORT` environment variable, or defaults to 4000.
 */
if (isMainModule(import.meta.url) || process.env['pm_id']) {
  const port = process.env['PORT'] || 4000;
  app.listen(port, (error) => {
    if (error) {
      throw error;
    }

    console.log(`Node Express server listening on http://localhost:${port}`);
  });
}

/**
 * Request handler used by the Angular CLI (for dev-server and during build) or Firebase Cloud Functions.
 */
export const reqHandler = createNodeRequestHandler(app);
