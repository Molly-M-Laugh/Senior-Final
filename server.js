//Install express server
const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;

// For local connections/runs, use Pool
/*
const {Pool} = require('pg');
require('dotenv').config(); // Need for inserting .env variables

const pool = new Pool({
  connectionString : process.env['DATABASE_URL']
});
const cors = require('cors'); // Link Angular, NodeJS

// Put most specific link here
app.use(express.json()); // For correct form parsing for db
app.use(cors({
    origin: ['http://localhost:4200',
            'https://senior-t-fd5496756068.herokuapp.com'],// Allow only app's origin
    methods: ['GET', 'POST'],//Allow designated RESTful methods
    allowedHeaders: ['Content-Type','Authorization'], // Allow only these headers
    optionsSuccessStatus: 200
    //credentials: true // Later for auth. found may be part of that
}));

app.post('/api/register', async (req, res) => {
  const { username, password } = req.body;
  try {
  const result = await pool.query('INSERT INTO users (username, password) VALUES ($1, crypt($2,gen_salt(\'bf\'))) RETURNING username', [username, password]);
  res.json(result.rows[0]);
  } catch (err) {
    console.error(err);
  }
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
*/

// Heroku DB, use client rather than pool
const Client = require('pg').Client;
const client = new Client({
  connectionString: process.env['DATABASE_URL'],
  ssl: {
    rejectUnauthorized: false
  }
});

client.connect();

app.use(express.json()); // For correct form parsing for db
app.post('/api/register', async (req, res) => {
  const { username, password } = req.body;
  try {
    client.query('INSERT INTO users (username, password) VALUES ($1, crypt($2,gen_salt(\'bf\')))', [username, password])
    //const result = await pool.query('INSERT INTO users (username, password) VALUES ($1, crypt($2,gen_salt(\'bf\'))) RETURNING username', [username, password]);
    res.json(result.rows[0]);
  } catch (err) {
    console.error(err);
  }
  client.end();
});
app.post('/api/login', async (req, res) => {
  const { username, password } = req.body;
  const result = client.query('SELECT (crypt($1,gen_salt(\'bf\')) = password) As is_match FROM users WHERE (username = $2) RETURNING username', [username, password])
  //const result = await pool.query('SELECT (crypt($1,gen_salt(\'bf\')) = password) As is_match FROM users WHERE (username = $2) RETURNING username', [username, password]);
  client.end();
});
app.put('/api/update/:id', async (req, res) => {
  const { id } = req.params;
  const { username, password } = req.body;
  client.query('UPDATE users SET username = $1, password = crypt($2,gen_salt(\'bf\')) WHERE id = $3', [username, password, id])
  //await pool.query('UPDATE users SET username = $1, password = crypt($2,gen_salt(\'bf\')) WHERE id = $3', [username, password, id]);
  res.send('Updated');
  client.end();
});

// Regular remainder of paths, but less specific -> below specific routes

// Serve only the static files form the dist directory
app.use(express.static(path.join(__dirname,'dist/senior-et/browser')));

app.use((req, res) => {
  res.sendFile(path.join(__dirname, 'dist/senior-et/browser/index.csr.html'));
});
//app.get('/:path*', (req, res) =>
//    res.sendFile(path.join(__dirname,'dist/senior-et/browser/index.html'))
//);

// Cors V2
/*
app.use(cors({
  origin: function(origin, callback) {
    if(!origin) return callback(null,true);
    if(origins.indexOf(origin) === -1) {
      var msg = 'CORS policy of site does not allow access from intended origins';
      return callback(new Error(msg),false);
    }
    return callback(null,true);
  }
}));
*/

// Start the app by listening on the default Heroku port
app.listen(PORT, () => {
    console.log(`Server listening on port ${PORT}`);
});