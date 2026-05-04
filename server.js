//Install express server
const express = require('express');
const jwt = require('jsonwebtoken');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;

// BLE setup
const Bluetooth = require('webbluetooth').Bluetooth;
const deviceFound = (device, selectFn) => {
    // If device can be automatically selected, do so by returning true
    if (device.name === 'MyESP32') {
      console.log([device.name]); // Get device name for here
      return true;
      }
    else { 
      console.log("Device not found"); 
      return false
    }
    // Otherwise store the selectFn somewhere and execute it later to select this device
};
const bluetooth = new Bluetooth({ deviceFound });

// Await for Bluetooth API calls

// For local connections/runs, use Pool

const {Pool} = require('pg');
//require('dotenv').config(); // Need for inserting .env variables

const pool = new Pool({
  connectionString : process.env['DATABASE_URL'],
  ssl: {
    rejectUnauthorized: false // Keep in for Heroku!
  }
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


// Heroku DB, use client rather than pool
// However, found less reliable than pool, and pool happens to work
// But for reference/alternative, kept the client code in but commented out
/*
const Client = require('pg').Client;
const client = new Client({
  connectionString: process.env['DATABASE_URL'],
  ssl: {
    rejectUnauthorized: false
  }
});
*/

//client.connect();

app.use(express.json()); // For correct form parsing for db
app.post('/api/register', async (req, res) => {
  const { username, password } = req.body;
  try {
    const result = await pool.query('INSERT INTO users (username, password) VALUES ($1, crypt($2,gen_salt(\'bf\'))) RETURNING username', [username, password])
    // Later, have check if no value returned
    res.json(result.rows[0]);
  } catch (err) {
    console.error(err);
  }
});
app.post('/api/login', async (req, res) => {
  const { username, password } = req.body;
  try {
    const result = await pool.query('SELECT COALESCE((password = crypt($1, password)), false) As is_match FROM users WHERE (username = $2)', [password, username])
    // Later, have check if no value returned
    const isMatch = result.rows[0]?.is_match || false;
    if(isMatch){
      res.json({is_match : isMatch});
    }
    else{
      res.status(401).json({is_match : isMatch});
    }
  } catch (err) {
    console.error(err);
  }
});
app.post("/api/generateToken", async (req, res) => {
  const { username, password } = req.body;
  try {
    const result = await pool.query('SELECT COALESCE((password = crypt($1, password)), false) As is_match FROM users WHERE (username = $2)', [password, username])
    // Later, have check if no value returned
    const isMatch = result.rows[0]?.is_match || false;
    if(isMatch){
      let jwtSecretKey = process.env.JWT_SECRET_KEY;
      let data = {
        time:Date(),
        userId:username
      }
      const token = jwt.sign(data, jwtSecretKey);
      res.status(200).send(token);
    }
    else{
      res.status(401).json({is_match : isMatch});
    }
  } catch (err) {
    console.error(err);
  }
});
app.get("/api/verifyToken", async (req,res) => {
  let tokenHeaderKey = process.env.TOKEN_KEY
  let jwtSecretKey = process.env.JWT_KEY
  try{
    const token = req.header(tokenHeaderKey);
    const verified = jwt.verify(token, jwtSecretKey);
    if(verified) {
      return res.send("Successfully verified");
    }
    else{
      return res.status(401).send(error);
    }
  }
  catch(error){
    return res.status(401).send(error);
  }
});
app.put('/api/update/:id', async (req, res) => {
  const { id } = req.params;
  const { username, password } = req.body;
  //await client.query('UPDATE users SET username = $1, password = crypt($2,gen_salt(\'bf\')) WHERE id = $3', [username, password, id])
  // Later, have check if no value returned?
  res.send('Updated');
});
// NOTE: Later, change to be for a specific id
app.post('/api/data', async (req, res) => {
  const { user, time, temp_avg, temp_1, temp_2, temp_3 } = req.body;
  console.print("Made to assignment");
  if (temp_avg == -0.01) {
    return res.json({is_inserted : true});
  }
  console.print("Made to start query");
  try {
  const result = await pool.query('INSERT INTO temp_data (user_id, record_date, temperature_avg, temperature_1, temperature_2, temperature_3) VALUES ($1, $2, $3, $4, $5, $6) RETURNING temperature_avg', [user, time, temp_avg, temp_1,temp_2,temp_3]);
  console.print("Made past query");
  const resNum = parseFloat(result.rows[0].temperature_avg);
  console.print("Made to parse");
  if (resNum < 100) {
    isInserted = Number(parseFloat(resNum).toFixed(2)) == temp_avg;
  } else {
    isInserted = Number(parseFloat(resNum).toFixed(1)) === temp_avg;
  }
  console.print("Made to return value");
  res.json({is_inserted : isInserted});
  } catch (err) {
    console.error(err);
  }
});
// NOTE: Later change to be for a specific id
app.get('/api/data-init', async (req, res) => {
  const user = 1; // Hardcode for now
  // Example for when switch to specific id
  //const user = users.find(u => u.id === parseInt(req.params.id));
  try {
    const result = await pool.query('SELECT record_date, temperature_avg FROM temp_data WHERE (user_id = $1) ORDER BY record_date DESC LIMIT 100', [user]);
    res.json(result.rows); // Want all data points
  } catch (err) {
    console.error(err);
  }
});

//client.end();

// Serve only the static files form the dist directory
app.use(express.static(path.join(__dirname,'dist/vital-vest/browser')));

app.use((req, res) => {
  res.sendFile(path.join(__dirname, 'dist/vital-vest/browser/index.csr.html'));
});

// Start the app by listening on the default Heroku port
app.listen(PORT, () => {
    console.log(`Server listening on port ${PORT}`);
});
