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
require('dotenv').config(); // Need for inserting .env variables

const pool = new Pool({
  connectionString : process.env['DATABASE_URL']
});
const cors = require('cors'); // Link Angular, NodeJS

// Bluetooth variables
const service = 'ab2d02b4-ad53-400f-bf7e-d603a657d07d';
const dataChar = '05ac146f-aee8-4659-aba5-882c1f7e0372';
const commandChar = '58bb99f3-75cb-48cb-81e4-346cc4f0687d';
var isInserted = false; // Mutual declaration, for keeping track that data inserted properly

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
/*
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
  try {
    const result = await pool.query('SELECT COALESCE((password = crypt($1, password)), false) As is_match FROM users WHERE (username = $2)', [password, username]);
    const isMatch = result.rows[0]?.is_match || false;
    res.json({is_match : isMatch});
  } catch (err) {
    console.error(err);
  }
});
// Wait until cookies to implement
app.put('/api/update-username/:id', async (req, res) => {
  const { id } = req.params;
  const { username, username_new } = req.body;
  try {
    await pool.query('UPDATE users SET username = $1 WHERE username = $2', [username_new, username]);
    res.send('Updated username');
  } catch (err) {
    console.error(err);
  }
});
app.put('/api/update-password', async (req, res) => {
  const { id } = req.params;
  const { username, password } = req.body;
  try {
    await pool.query('UPDATE users SET password = $1 WHERE username = $2', [password, username]);
    res.send('Updated password');
  } catch (err) {
    console.error(err);
  }
});
// NOTE: Later, change to be for a specific id
app.post('/api/data', async (req, res) => {
  const { user, time, temp } = req.body;
  if (temp == -0.01) {
    console.log("Should not store, returning early");
    return res.json({is_inserted : true});
  }
  console.log('Values:', user, ", ", time, "; ", temp);
  try {
  console.log("Made it to insert");
  const result = await pool.query('INSERT INTO temp_data (user_id, record_date, temperature) VALUES ($1, $2, $3) RETURNING temperature', [user, time, temp]);
  console.log("Inserted some value");
  const resNum = parseFloat(result.rows[0].temperature);
  if (resNum < 100) {
    isInserted = Number(parseFloat(resNum).toFixed(2)) == temp;
  } else {
    isInserted = Number(parseFloat(resNum).toFixed(1)) === temp;
  }
  console.log("Return boolean is: ", isInserted, ", for: ", temp, ", ", resNum);
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
    const result = await pool.query('SELECT (record_date, temperature) FROM temp_data WHERE (user_id = $1)', [user]);
    res.json(result.rows); // Want all data points
  } catch (err) {
    console.error(err);
  }
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
  //client.connect();
  const { username, password } = req.body;
  try {
    const result = await client.query('INSERT INTO users (username, password) VALUES ($1, crypt($2,gen_salt(\'bf\'))) RETURNING username', [username, password])
    // Later, have check if no value returned
    res.json(result.rows[0]);
  } catch (err) {
    console.error(err);
  }
  //client.end();
});
app.post('/api/login', async (req, res) => {
  //client.connect();
  const { username, password } = req.body;
  try {
    const result = await client.query('SELECT COALESCE((password = crypt($1, password)), false) As is_match FROM users WHERE (username = $2)', [password, username])
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
  //client.end();
});
app.post("/api/generateToken", async (req, res) => {
  const { username, password } = req.body;
  try {
    const result = await client.query('SELECT COALESCE((password = crypt($1, password)), false) As is_match FROM users WHERE (username = $2)', [password, username])
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
  //client.connect();
  const { id } = req.params;
  const { username, password } = req.body;
  //await client.query('UPDATE users SET username = $1, password = crypt($2,gen_salt(\'bf\')) WHERE id = $3', [username, password, id])
  // Later, have check if no value returned?
  res.send('Updated');
  //client.end();
});
// NOTE: Later, change to be for a specific id
app.post('/api/data', async (req, res) => {
  const { user, time, temp } = req.body;
  if (temp == -0.01) {
    return res.json({is_inserted : true});
  }
  try {
  //client.connect();
  const result = await client.query('INSERT INTO temp_data (user_id, record_date, temperature) VALUES ($1, $2, $3) RETURNING temperature', [user, time, temp]);
  const resNum = parseFloat(result.rows[0].temperature);
  if (resNum < 100) {
    isInserted = Number(parseFloat(resNum).toFixed(2)) == temp;
  } else {
    isInserted = Number(parseFloat(resNum).toFixed(1)) === temp;
  }
  res.json({is_inserted : isInserted});
  } catch (err) {
    console.error(err);
  }
});
// NOTE: Later change to be for a specific id
app.get('/api/data-init', async (req, res) => {
  //client.connect();
  const user = 1; // Hardcode for now
  // Example for when switch to specific id
  //const user = users.find(u => u.id === parseInt(req.params.id));
  try {
    const result = await client.query('SELECT (record_date, temperature) FROM temp_data WHERE (user_id = $1)', [user]);
    res.json(result.rows); // Want all data points
  } catch (err) {
    console.error(err);
  }


  client.end();
});

// NOTE: For only database, modify for Heroku and local!!!
// BLE API calls (ie. update data)
// Data should be in form of [{x:__,y:__},...{x:__,y:__}]
/*
app.get('/api/data', async (req, res) => {
  try {
        const device = await bluetooth.requestDevice({
            filters: [{ name: 'MyESP32' }],
            optionalServices: [service]
        });
        const server = await device.gatt.connect();
        const bleService = await server.getPrimaryService(service);
        const characteristic = await bleService.getCharacteristic(dataChar);
        
        const value = await characteristic.readValue();
        // Convert the DataView to a string (assuming messenger is a string)
        const decoder = new TextDecoder('utf-8');
        const decodedString = decoder.decode(value);

        res.json({ value: decodedString });
    } catch (err) {
        console.error(err);
        res.status(500).send(err.message);
    }

  /*
  console.log("Requesting device for service");
  const {device} = await bluetooth.requestDevice({
    filters:[{ 
      acceptAllDevices: true,
      optionalServices: [ service ]
     }] 
  });
  console.log("Found device: ", device.name)
  console.log("Requesting server");
  const {server} = await device.gatt.connect();
  console.log("Requesting device data");
  server.getPrimaryService(service).then(
    service => 
    {
      // Note: Ask what service/characteristic for time versus data (or how combined)
      const data = service.getCharacteristic('05ac146f-aee8-4659-aba5-882c1f7e0372').readValue()
      console.log("Data retreived");
      res.json(data.rows[0]);
    }
  );
  
  // 
});
*/

// Regular remainder of paths, but less specific -> below specific routes-----------

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