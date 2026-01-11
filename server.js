//Install express server
const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;

// Serve only the static files form the dist directory
app.use(express.static(path.join(__dirname,'dist/team_14_portfolios/browser')));

app.use((req, res) => {
  res.sendFile(path.join(__dirname, 'dist/team_14_portfolios/browser/index.html'));
});
//app.get('/:path*', (req, res) =>
//    res.sendFile(path.join(__dirname,'dist/team_14_portfolios/browser/index.html'))
//);

// Start the app by listening on the default Heroku port
app.listen(PORT, () => {
    console.log(`Server listening on port ${PORT}`);
});