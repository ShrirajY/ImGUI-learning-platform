// Usage: node server/build-cli.js <file.cpp> [more files...] [--run]
// Compiles each lesson file into the sandbox engine; exits non-zero if any fail.
'use strict';
const fs = require('fs');
const path = require('path');
const builder = require('./builder');
(async () => {
  const args = process.argv.slice(2);
  const runAfter = args.includes('--run');
  const files = args.filter(a => !a.startsWith('--'));
  let failed = 0;
  for (const f of files) {
    const r = await builder.build({ code: fs.readFileSync(f, 'utf8'), title: path.basename(f), runAfter });
    const errs = r.diagnostics.filter(d => d.severity === 'error');
    const warns = r.diagnostics.filter(d => d.severity === 'warning' && d.file === 'Lesson.inl');
    console.log(`${r.ok ? 'OK  ' : 'FAIL'} ${path.basename(f)} (${r.ms || 0} ms) ${warns.length ? warns.length + ' warning(s)' : ''}`);
    if (!r.ok) { failed++; console.log(r.log.split('\n').filter(l => /error|warning/i.test(l)).slice(0, 15).join('\n') || r.log); }
    else warns.forEach(w => console.log(`   warn line ${w.line}: ${w.message}`));
  }
  process.exit(failed ? 1 : 0);
})();
