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

const pool = new Pool({
  connectionString : process.env['DATABASE_URL'],
  //user: 'postgres',
  //database: 'postgres',
  //password : process.env['DATABASE_PASSWORD'],
  //port: 5432
  //host: 'postgres'
  //ssl: { rejectUnauthorized: false }
});
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

app.post('/api/register', async (req, res) => {
  console.log('Starting Register');
  const { username, password } = req.body;
  try {
  console.log('Obtaining result');
  const result = await pool.query('INSERT INTO users (username, password) VALUES ($1, crypt($2,gen_salt(\'bf\'))) RETURNING *', [username, password]);
  console.log('Got a result');
  res.json(result.rows[0]);
  } catch (err) {
    console.error(err);
  }
  console.log('Ending registration');
});
app.post('/api/login', async (req, res) => {
  const { username, password } = req.body;
  const result = await pool.query('SELECT (crypt($1,gen_salt(\'bf\')) = password) As is_match FROM users WHERE (username = $2) RETURNING *', [username, password]);
});
app.put('/api/update/:id', async (req, res) => {
  const { id } = req.params;
  const { username, password } = req.body;
  await pool.query('UPDATE users SET username = $1, password = $2 WHERE id = $3', [username, password, id]);
  res.send('Updated');
});


// Heroku DB
/*
const Client = require('pg');
const client = new Client({
  connectionString: process.env['DATABASE_URL'],
  ssl: {
    rejectUnauthorized: false
  }
});

client.connect();

client.query('SELECT table_schema,table_name FROM information_schema.tables;', (err:HttpErrorResponse, res:Response) => {
  if (err) throw err;
  for (let row of res.rows) {
    console.log(JSON.stringify(row));
  }
  client.end();
});
*/

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
