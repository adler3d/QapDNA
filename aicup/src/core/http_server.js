const http = require('http-server');

const server = http.createServer({
  root: './',
  headers: {
    'Cross-Origin-Opener-Policy': 'same-origin',
    'Cross-Origin-Embedder-Policy': 'require-corp',
    'Cache-Control': 'no-store, no-cache, must-revalidate, proxy-revalidate, max-age=0',
    'Pragma': 'no-cache',
    'Expires': '0'
  }
});

server.listen(9010, () => {
  console.log('Server running on http://localhost:9010');
});