const http = require('http');
const fs = require('fs');
const path = require('path');
const { DatabaseSync } = require('node:sqlite');   // built-in, no install

const db = new DatabaseSync(path.join(__dirname, '..', 'nixel.db'), { readOnly: true });

const ACCOUNT = 'test-account';

const latestStmt = db.prepare(`
  SELECT c.host, c.port, c.status, c.latency_ms, c.ts   
    FROM checks c
    JOIN (
        SELECT host, port, MAX(ts) AS max_ts
        FROM checks
        WHERE account_id = ?
        GROUP BY host, port        
    ) latest
    ON c.host = latest.host
    AND c.port = latest.port         
    AND c.ts   = latest.max_ts
    WHERE c.account_id = ?
    ORDER BY c.host, c.port;
`);

const server = http.createServer((req, res) => {
  if (req.url === '/api/status') {
    const rows = latestStmt.all(ACCOUNT, ACCOUNT);   // same call shape
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(rows));
    return;
  }
  const file = req.url === '/' ? '/index.html' : req.url;
  fs.readFile(path.join(__dirname, 'public', file), (err, data) => {
    if (err) { res.writeHead(404); res.end('Not found'); return; }
    res.end(data);
  });
});

server.listen(3000, () => console.log('dashboard on http://localhost:3000'));
