// builder.js - compiles lesson code into the sandbox engine with MSVC and launches it.
'use strict';
const fs = require('fs');
const path = require('path');
const { spawn, execFile } = require('child_process');

const ROOT      = path.resolve(__dirname, '..');
const SANDBOX   = path.join(ROOT, 'sandbox');
const IMGUI_DIR = path.join(SANDBOX, 'external', 'imgui');
const ENGINE    = path.join(SANDBOX, 'engine', 'OGL.c');
const CACHE_OBJ = path.join(SANDBOX, 'cache', 'obj');
const WORK      = process.env.LAB_WORK ? path.resolve(process.env.LAB_WORK) : path.join(SANDBOX, 'work');
const EXE       = path.join(WORK, 'ImGuiLab.exe');

const IMGUI_SOURCES = ['imgui.cpp', 'imgui_draw.cpp', 'imgui_widgets.cpp', 'imgui_tables.cpp',
                       'imgui_demo.cpp', 'imgui_impl_win32.cpp', 'imgui_impl_opengl2.cpp'];

let msvcEnv = null;          // cached environment from vcvars64.bat
let msvcInfo = null;         // { vsPath, vcvars }
let runningProc = null;      // currently running lesson exe
let queue = Promise.resolve();

function run(cmd, args, opts) {
  return new Promise((resolve) => {
    const p = spawn(cmd, args, { windowsHide: true, ...opts });
    let out = '';
    p.stdout && p.stdout.on('data', d => out += d.toString());
    p.stderr && p.stderr.on('data', d => out += d.toString());
    p.on('error', e => resolve({ code: -1, out: out + String(e) }));
    p.on('close', code => resolve({ code, out }));
  });
}

function findVcvars() {
  if (msvcInfo) return msvcInfo;
  const vswhere = path.join(process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)',
                            'Microsoft Visual Studio', 'Installer', 'vswhere.exe');
  if (!fs.existsSync(vswhere)) return null;
  return new Promise((resolve) => {
    execFile(vswhere, ['-latest', '-products', '*', '-requires',
      'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
      { windowsHide: true }, (err, stdout) => {
        const vsPath = (stdout || '').trim().split(/\r?\n/)[0];
        const vcvars = vsPath && path.join(vsPath, 'VC', 'Auxiliary', 'Build', 'vcvars64.bat');
        if (!err && vcvars && fs.existsSync(vcvars)) { msvcInfo = { vsPath, vcvars }; resolve(msvcInfo); }
        else resolve(null);
      });
  });
}

async function getMsvcEnv() {
  if (msvcEnv) return msvcEnv;
  const info = await findVcvars();
  if (!info) throw new Error('MSVC (Visual Studio C++ tools) not found. Run setup.bat.');
  const installer = path.join(process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)', 'Microsoft Visual Studio', 'Installer');
  const env = { ...process.env, PATH: installer + ';' + process.env.PATH };
  // cmd /s /c ""vcvars" >nul && set"
  const r = await run('cmd.exe', ['/d', '/s', '/c', `""${info.vcvars}" >nul && set"`],
                      { env, windowsVerbatimArguments: true });
  if (r.code !== 0) throw new Error('vcvars64.bat failed:\n' + r.out);
  const parsed = {};
  for (const line of r.out.split(/\r?\n/)) {
    const i = line.indexOf('=');
    if (i > 0) parsed[line.slice(0, i)] = line.slice(i + 1);
  }
  if (!parsed.INCLUDE) throw new Error('Could not capture MSVC environment.');
  msvcEnv = parsed;
  return msvcEnv;
}

// Created once per machine by setup.bat. When it changes, the website resets saved progress.
function readInstallId() {
  try { return fs.readFileSync(path.join(ROOT, '.install_id'), 'utf8').trim() || null; } catch { return null; }
}

async function status() {
  const info = await findVcvars();
  return {
    msvc: !!info,
    vsPath: info ? info.vsPath : null,
    imguiSources: IMGUI_SOURCES.every(f => fs.existsSync(path.join(IMGUI_DIR, f))),
    imguiCompiled: IMGUI_SOURCES.every(f => fs.existsSync(path.join(CACHE_OBJ, f.replace('.cpp', '.obj')))),
    running: !!runningProc,
    node: process.version,
    installId: readInstallId(),
  };
}

let imguiInFlight = null;     // shared so a warm-up and a build never compile ImGui twice at once
function ensureImGui(env, log) {
  if (!imguiInFlight) imguiInFlight = compileImGui(env, log).finally(() => { imguiInFlight = null; });
  else log.push('Waiting for Dear ImGui library compile already in progress...');
  return imguiInFlight;
}

async function compileImGui(env, log) {
  fs.mkdirSync(CACHE_OBJ, { recursive: true });
  const missing = IMGUI_SOURCES.filter(f => !fs.existsSync(path.join(CACHE_OBJ, f.replace('.cpp', '.obj'))));
  if (!missing.length) return true;
  log.push('Compiling Dear ImGui library (first build only, ~20-40 s)...');
  const r = await run('cl.exe', ['/nologo', '/c', '/EHsc', '/std:c++17', '/O2', '/MD', '/MP', '/wd4005',
    '/I', IMGUI_DIR, ...missing.map(f => path.join(IMGUI_DIR, f)), '/Fo' + CACHE_OBJ + '\\'],
    { env, cwd: CACHE_OBJ });
  log.push(r.out.trim());
  return r.code === 0;
}

// Parse MSVC diagnostics like:  C:\...\Lesson.inl(12): error C2065: 'x': undeclared identifier
function parseDiagnostics(out) {
  const diags = [];
  const re = /^(.*?)\((\d+)(?:,\d+)?\)\s*:\s*(fatal error|error|warning)\s+(\w+)\s*:\s*(.*)$/gm;
  let m;
  while ((m = re.exec(out))) {
    const file = path.basename(m[1].trim());
    diags.push({ file, line: parseInt(m[2], 10), severity: m[3].includes('error') ? 'error' : 'warning',
                 code: m[4], message: m[5].trim() });
  }
  return diags;
}

function stopRunning() {
  return new Promise((resolve) => {
    if (!runningProc) return resolve(false);
    const pid = runningProc.pid;
    runningProc = null;
    execFile('taskkill', ['/PID', String(pid), '/T', '/F'], { windowsHide: true }, () => resolve(true));
  });
}

function launch(title) {
  const safeTitle = String(title || 'Playground').replace(/[^\w \-.:()#+&,']/g, '').slice(0, 80);
  const p = spawn(EXE, [safeTitle], { cwd: WORK, detached: true, stdio: 'ignore' });
  runningProc = p;
  p.on('exit', () => { if (runningProc === p) runningProc = null; });
  p.unref();
}

async function doBuild({ code, title, runAfter = true }) {
  const t0 = Date.now();
  const log = [];
  let env;
  try { env = await getMsvcEnv(); }
  catch (e) { return { ok: false, stage: 'msvc', log: e.message, diagnostics: [] }; }

  if (!(await ensureImGui(env, log)))
    return { ok: false, stage: 'imgui', log: log.join('\n'), diagnostics: [] };

  await stopRunning();
  await new Promise(r => setTimeout(r, 150));      // let Windows release the exe file lock
  fs.mkdirSync(WORK, { recursive: true });
  fs.writeFileSync(path.join(WORK, 'Lesson.inl'), String(code || ''), 'utf8');

  const objOut = path.join(WORK, 'OGL.obj');
  const c = await run('cl.exe', ['/nologo', '/c', '/EHsc', '/std:c++17', '/O2', '/MD', '/W3', '/wd4996', '/TP',
    '/I', IMGUI_DIR, '/I', WORK, ENGINE, '/Fo' + objOut], { env, cwd: WORK });
  log.push(c.out.trim());
  const diagnostics = parseDiagnostics(c.out);
  if (c.code !== 0)
    return { ok: false, stage: 'compile', log: log.join('\n'), diagnostics, ms: Date.now() - t0 };

  const objs = IMGUI_SOURCES.map(f => path.join(CACHE_OBJ, f.replace('.cpp', '.obj')));
  const l = await run('link.exe', ['/nologo', objOut, ...objs, 'user32.lib', 'gdi32.lib', 'opengl32.lib',
    'glu32.lib', '/SUBSYSTEM:WINDOWS', '/OUT:' + EXE], { env, cwd: WORK });
  if (l.out.trim()) log.push(l.out.trim());
  if (l.code !== 0)
    return { ok: false, stage: 'link', log: log.join('\n'), diagnostics: diagnostics.concat(parseDiagnostics(l.out)), ms: Date.now() - t0 };

  if (runAfter) launch(title);
  return { ok: true, stage: 'done', log: log.filter(Boolean).join('\n'), diagnostics, ms: Date.now() - t0, launched: runAfter };
}

// Builds are serialised: one compiler run at a time.
function build(opts) {
  const p = queue.then(() => doBuild(opts));
  queue = p.catch(() => {});
  return p;
}

function engineSource() { return fs.readFileSync(ENGINE, 'utf8'); }

module.exports = { build, status, stopRunning, engineSource, getMsvcEnv, ensureImGui, paths: { ROOT, SANDBOX, WORK } };
