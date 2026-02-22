//Install express server
const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;

const cors = require('cors'); // Link Angular, NodeJS

// Serve only the static files form the dist directory
app.use(express.static(path.join(__dirname,'dist/senior-et/browser')));

app.use((req, res) => {
  res.sendFile(path.join(__dirname, 'dist/senior-et/browser/index.csr.html'));
});
//app.get('/:path*', (req, res) =>
//    res.sendFile(path.join(__dirname,'dist/senior-et/browser/index.html'))
//);

app.use(cors({
    origin: ['http://localhost:4200',
            'https://senior-t-fd5496756068.herokuapp.com'],// Allow only app's origin
    methods: ['GET', 'POST'],//Allow designated RESTful methods
    allowedHeaders: ['Access-Control-Allow-Origin','Authorization'] // Allow only these headers
    //credentials: true // Later for auth. found may be part of that
}));

// Start the app by listening on the default Heroku port
app.listen(PORT, () => {
    console.log(`Server listening on port ${PORT}`);
});