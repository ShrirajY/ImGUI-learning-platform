// server.js - ImGui Learning Platform local web server (no npm dependencies)
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const builder = require('./builder');

const PORT = parseInt(process.env.PORT || '5173', 10);
const HOST = '127.0.0.1';
const PUBLIC = path.join(builder.paths.ROOT, 'public');

const MIME = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8', '.cpp': 'text/plain; charset=utf-8', '.c': 'text/plain; charset=utf-8',
  '.inl': 'text/plain; charset=utf-8', '.txt': 'text/plain; charset=utf-8', '.md': 'text/plain; charset=utf-8',
  '.svg': 'image/svg+xml', '.png': 'image/png', '.ico': 'image/x-icon', '.zip': 'application/zip',
};

function send(res, status, body, type = 'application/json; charset=utf-8', extra = {}) {
  res.writeHead(status, { 'Content-Type': type, 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff', ...extra });
  res.end(typeof body === 'string' || Buffer.isBuffer(body) ? body : JSON.stringify(body));
}

// The API can compile and run native code, so only accept requests that clearly come from
// our own page: correct Host (blocks DNS rebinding) + a custom header (forces a CORS
// preflight which we never approve, so other websites cannot POST here).
function allowedHost(req) {
  const h = String(req.headers.host || '').toLowerCase();
  return h === `127.0.0.1:${PORT}` || h === `localhost:${PORT}`;
}
function trustedApiCall(req) {
  return allowedHost(req) && req.headers['x-imgui-lab'] === '1';
}

function readBody(req, limit = 512 * 1024) {
  return new Promise((resolve, reject) => {
    let size = 0; const chunks = [];
    req.on('data', c => { size += c.length; if (size > limit) { reject(new Error('Body too large')); req.destroy(); } else chunks.push(c); });
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

function serveStatic(req, res, urlPath) {
  let rel = decodeURIComponent(urlPath.split('?')[0]);
  if (rel === '/' || rel === '') rel = '/index.html';
  const file = path.normalize(path.join(PUBLIC, rel));
  if (!file.startsWith(PUBLIC)) return send(res, 403, 'Forbidden', 'text/plain');
  fs.stat(file, (err, st) => {
    if (err || !st.isFile()) return send(res, 404, 'Not found', 'text/plain');
    const ext = path.extname(file).toLowerCase();
    res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream', 'Cache-Control': 'no-cache' });
    fs.createReadStream(file).pipe(res);
  });
}

const server = http.createServer(async (req, res) => {
  try {
    if (!allowedHost(req)) return send(res, 403, 'Forbidden host', 'text/plain');
    const url = req.url || '/';

    if (url.startsWith('/api/')) {
      if (!trustedApiCall(req)) return send(res, 403, { error: 'Forbidden' });

      if (url === '/api/status' && req.method === 'GET')
        return send(res, 200, await builder.status());

      if (url === '/api/engine-source' && req.method === 'GET')
        return send(res, 200, builder.engineSource(), 'text/plain; charset=utf-8');

      if (url === '/api/build' && req.method === 'POST') {
        if (!String(req.headers['content-type'] || '').startsWith('application/json'))
          return send(res, 415, { error: 'JSON required' });
        const body = JSON.parse(await readBody(req));
        const result = await builder.build({ code: body.code, title: body.title, runAfter: body.run !== false });
        return send(res, 200, result);
      }

      if (url === '/api/stop' && req.method === 'POST')
        return send(res, 200, { stopped: await builder.stopRunning() });

      return send(res, 404, { error: 'Unknown API' });
    }

    if (req.method !== 'GET' && req.method !== 'HEAD') return send(res, 405, 'Method not allowed', 'text/plain');
    serveStatic(req, res, url);
  } catch (e) {
    send(res, 500, { error: String(e && e.message || e) });
  }
});

server.on('error', (e) => {
  if (e.code === 'EADDRINUSE') {
    console.error(`\n  Port ${PORT} is already in use. Is the platform already running?`);
    console.error(`  Open http://localhost:${PORT}  or set another port:  set PORT=5174 && run.bat\n`);
  } else console.error(e);
  process.exit(1);
});

server.listen(PORT, HOST, () => {
  console.log('');
  console.log('  ==============================================');
  console.log('   ImGui Learning Platform is running');
  console.log(`   Open:  http://localhost:${PORT}`);
  console.log('   Press Ctrl+C in this window to stop.');
  console.log('  ==============================================');
  console.log('');
  // Warm up the MSVC environment in the background so the first build is quicker.
  builder.getMsvcEnv().then(env => builder.ensureImGui(env, [])).catch(() => {});
});

process.on('SIGINT', async () => { await builder.stopRunning(); process.exit(0); });
