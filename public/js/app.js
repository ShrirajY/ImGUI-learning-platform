// app.js - ImGui Lab single page app
(function () {
  'use strict';
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
  const main = $('#main');

  const store = {
    get(k, d) { try { const v = localStorage.getItem('imguilab:' + k); return v === null ? d : JSON.parse(v); } catch { return d; } },
    set(k, v) { try { localStorage.setItem('imguilab:' + k, JSON.stringify(v)); } catch { } },
    del(k) { try { localStorage.removeItem('imguilab:' + k); } catch { } },
  };

  let manifest = null;          // { sections: [{title, lessons:[id]}] }
  const lessonMeta = {};        // id -> meta json
  let order = [];               // flat list of lesson ids
  let serverOnline = false;

  // ------------------------------------------------------------------ API
  async function api(path, opts = {}) {
    const res = await fetch(path, { ...opts, headers: { 'X-ImGui-Lab': '1', ...(opts.headers || {}) } });
    if (!res.ok) throw new Error((await res.text()) || res.statusText);
    const ct = res.headers.get('content-type') || '';
    return ct.includes('json') ? res.json() : res.text();
  }

  // Progress lives in this browser's localStorage. setup.bat writes a new install id the
  // first time it runs on a machine; when the id changes, start from 0.
  function resetProgress() {
    Object.keys(localStorage).filter(k => k.startsWith('imguilab:') && k !== 'imguilab:installId')
      .forEach(k => localStorage.removeItem(k));
  }
  function checkInstallId(id) {
    const old = store.get('installId', null);
    if (!id || old === id) return;
    resetProgress();
    store.set('installId', id);
    if (old !== null && manifest) { renderSidebar(); route(); }   // re-render if we wiped visible progress
  }

  async function refreshStatus() {
    const pill = $('#statusPill');
    try {
      const s = await api('/api/status');
      serverOnline = true;
      checkInstallId(s.installId);
      pill.className = 'status-pill ' + (s.msvc ? 'ok' : 'bad');
      pill.textContent = s.msvc ? (s.running ? '● lesson running' : '● MSVC ready') : '● MSVC missing';
      pill.title = s.msvc ? 'Visual Studio: ' + s.vsPath + (s.imguiCompiled ? '\nImGui library compiled' : '\nImGui compiles on first build') : 'Run setup.bat to install the C++ build tools';
      return s;
    } catch {
      serverOnline = false;
      pill.className = 'status-pill bad';
      pill.textContent = '● offline';
      pill.title = 'Start the site with run.bat to enable native builds';
      return null;
    }
  }

  async function fetchText(url) { const r = await fetch(url, { cache: 'no-cache' }); if (!r.ok) throw new Error(url + ' ' + r.status); return r.text(); }
  async function fetchJSON(url) { return JSON.parse(await fetchText(url)); }

  // ------------------------------------------------------------------ Sidebar / progress
  function completed() { return store.get('completed', {}); }
  function setCompleted(id, v) { const c = completed(); if (v) c[id] = true; else delete c[id]; store.set('completed', c); renderSidebar(); }

  function renderSidebar(activeId) {
    if (!manifest) return;
    activeId = activeId || (location.hash.match(/#\/lesson\/(.+)/) || [])[1];
    const done = completed();
    let html = '', n = 0;
    manifest.sections.forEach(sec => {
      html += `<div class="nav-section">${sec.title}</div>`;
      sec.lessons.forEach(id => {
        const m = lessonMeta[id] || { title: id };
        n++;
        html += `<a class="nav-lesson ${id === activeId ? 'active' : ''} ${done[id] ? 'done' : ''}" href="#/lesson/${id}">
                   <span class="nav-num">${done[id] ? '&#10003;' : n}</span><span>${m.title}</span></a>`;
      });
    });
    $('#lessonNav').innerHTML = html;
    const total = order.length, count = order.filter(id => done[id]).length;
    $('#progressText').textContent = `${count} / ${total}`;
    $('#progressFill').style.width = total ? (100 * count / total) + '%' : '0';
  }

  async function loadManifest() {
    manifest = await fetchJSON('lessons/manifest.json');
    order = manifest.sections.flatMap(s => s.lessons);
    await Promise.all(order.map(async id => {
      try { lessonMeta[id] = await fetchJSON(`lessons/${id}.json`); }
      catch { lessonMeta[id] = { id, title: id + ' (missing)', summary: '' }; }
    }));
    renderSidebar();
  }

  // ------------------------------------------------------------------ IDE (editor + build)
  // wrap: optional function turning the editor text into the full Lesson.inl (used by the
  //       playground's "UI code" mode, where the editor only holds the body of Lesson_UI()).
  function createIDE(host, { storageKey, starter, title, wrap = null, hint = null, fileName = null }) {
    host.appendChild($('#editorTemplate').content.cloneNode(true));
    const ide = host.querySelector('.ide');
    const log = ide.querySelector('.ide-log');
    const state = ide.querySelector('.build-state');
    if (hint) ide.querySelector('.file-hint').innerHTML = hint;
    if (fileName) ide.querySelector('.file-tab').textContent = fileName;
    const fullCode = () => wrap ? wrap(editor.value) : editor.value;
    const saved = store.get(storageKey, null);
    const editor = new CodeEditor(ide.querySelector('.code-editor'), {
      value: saved !== null ? saved : starter,
      onChange: v => store.set(storageKey, v),
      onRun: () => run(),
    });
    if (saved !== null && saved !== starter) state.innerHTML = '<span class="muted">restored your edits</span>';

    async function run() {
      if (!serverOnline) await refreshStatus();
      if (!serverOnline) {
        log.innerHTML = '<span class="err">The local server is not reachable.</span>\nStart the platform with <b>run.bat</b> (not by opening index.html directly).';
        return;
      }
      const btn = ide.querySelector('[data-act=run]');
      btn.disabled = true;
      state.innerHTML = '<span class="spin"></span> compiling...';
      log.textContent = 'cl.exe /TP OGL.c (with your Lesson.inl) ...';
      try {
        const r = await api('/api/build', { method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ code: fullCode(), title, run: true }) });
        editor.setMarks(r.diagnostics);
        const diags = (r.diagnostics || []).filter(d => d.file === 'Lesson.inl');
        let html = '';
        if (r.ok) {
          state.innerHTML = `<span class="okc">&#10003; built in ${(r.ms / 1000).toFixed(1)} s - window launched</span>`;
          html = '<span class="okc">Build succeeded.</span> The ImGui Lab window should now be open (check your taskbar).\n';
        } else {
          state.innerHTML = `<span class="err">&#10007; ${r.stage} failed</span>`;
          html = `<span class="err">Build failed at stage: ${r.stage}</span>\n`;
        }
        const balance = CodeEditor.braceBalance(editor.value);
        if (balance !== 0) {
          html += `\n<span class="hint">&#128161; Your braces don't match: ${balance > 0 ? balance + ' <b>{</b> without a closing <b>}</b>' : (-balance) + ' extra <b>}</b>'}. ` +
                  'Every <b>{</b> needs exactly one <b>}</b>.</span>\n';
        }
        if (wrap && diags.some(d => d.code === 'C2601')) {
          html += '\n<span class="hint">&#128161; In <b>UI code</b> mode your code already runs inside Lesson_UI(), so you can\'t define functions here. ' +
                  'Switch the playground to <b>Full file</b> mode to write your own functions.</span>\n';
        }
        // Statements written outside any function produce these errors on ImGui:: lines.
        else if (diags.some(d => ['C4430', 'C2365', 'C2371', 'C2556'].includes(d.code))) {
          html += '\n<span class="hint">&#128161; Code is probably <b>outside a function</b>. Statements like ImGui::Begin(...) only work ' +
                  'inside a function body: move them between the { and } of <b>Lesson_UI()</b> (or Lesson_Init()).</span>\n';
        }
        if (diags.length) {
          html += '\n' + diags.map(d => `<a class="diag ${d.severity}" data-line="${d.line}">Line ${d.line}: ${d.severity} ${d.code}: ${Highlight.escape(d.message)}</a>`).join('\n') + '\n';
        }
        const other = (r.log || '').split('\n').filter(l => l.trim() && !/^OGL\.c$/.test(l.trim()) && !/Lesson\.inl\(\d+/.test(l)).join('\n');
        if (other && (!r.ok || /warning/i.test(other))) html += '\n<span class="muted">' + Highlight.escape(other) + '</span>';
        log.innerHTML = html;
        $$('.diag', log).forEach(a => a.onclick = () => editor.goToLine(+a.dataset.line));
      } catch (e) {
        state.innerHTML = '<span class="err">error</span>';
        log.textContent = String(e.message || e);
      } finally {
        btn.disabled = false;
        refreshStatus();
      }
    }

    ide.addEventListener('click', async (e) => {
      const act = e.target.closest('[data-act]') && e.target.closest('[data-act]').dataset.act;
      if (!act) return;
      if (act === 'run') run();
      if (act === 'stop') { try { await api('/api/stop', { method: 'POST' }); state.textContent = 'stopped'; } catch { } refreshStatus(); }
      if (act === 'copy') { navigator.clipboard.writeText(editor.value); state.textContent = 'copied to clipboard'; }
      if (act === 'reset') { if (confirm('Discard your changes and restore the starter code?')) { editor.value = starter; store.del(storageKey); state.textContent = 'reset to starter code'; } }
      if (act === 'full') showFullSource(fullCode());
    });
    return { editor, run };
  }

  async function showFullSource(lessonCode) {
    let engine = '';
    try { engine = await api('/api/engine-source'); } catch { engine = '// (start the platform with run.bat to view the engine source)'; }
    const full = engine.replace('#include "Lesson.inl"', '// ---- #include "Lesson.inl"  (your code, inlined below) ----\n' + lessonCode);
    openModal('OGL.c - the complete file that gets compiled',
      `<p class="muted">This is exactly what <code>cl.exe /TP</code> compiles: the engine host with your lesson code pasted in at the bottom.</p>
       <pre class="full-src"><code>${Highlight.highlight(full)}</code></pre>`);
  }

  function openModal(title, html) {
    $('#modalTitle').textContent = title;
    $('#modalBody').innerHTML = html;
    $('#modal').hidden = false;
  }
  $('#modalClose').onclick = () => $('#modal').hidden = true;
  $('#modal').addEventListener('click', e => { if (e.target.id === 'modal') $('#modal').hidden = true; });
  document.addEventListener('keydown', e => { if (e.key === 'Escape') $('#modal').hidden = true; });

  // ------------------------------------------------------------------ Quiz
  function renderQuiz(host, id, quiz) {
    if (!quiz || !quiz.length) return;
    const answers = store.get('quiz:' + id, {});
    const box = document.createElement('section');
    box.className = 'quiz';
    box.innerHTML = '<h2>Check your understanding</h2>' + quiz.map((q, qi) => `
      <div class="q" data-q="${qi}">
        <div class="q-text">${qi + 1}. ${q.q}</div>
        ${q.options.map((o, oi) => `<button class="q-opt" data-o="${oi}">${Highlight.escape(o)}</button>`).join('')}
        <div class="q-explain" hidden></div>
      </div>`).join('') + '<div class="q-score"></div>';
    host.appendChild(box);

    function update() {
      let correct = 0;
      $$('.q', box).forEach(qEl => {
        const qi = +qEl.dataset.q, q = quiz[qi], a = answers[qi];
        $$('.q-opt', qEl).forEach(b => {
          const oi = +b.dataset.o;
          b.classList.toggle('right', a !== undefined && oi === q.answer);
          b.classList.toggle('wrong', a === oi && oi !== q.answer);
        });
        const ex = $('.q-explain', qEl);
        if (a !== undefined) { ex.hidden = false; ex.innerHTML = (a === q.answer ? '<b class="okc">Correct.</b> ' : '<b class="err">Not quite.</b> ') + (q.explain || ''); }
        if (a === q.answer) correct++;
      });
      $('.q-score', box).textContent = `${correct} / ${quiz.length} correct`;
      if (correct === quiz.length && !completed()[id]) { setCompleted(id, true); const b = $('#completeBtn'); if (b) b.textContent = '✓ Completed'; }
    }
    box.addEventListener('click', e => {
      const b = e.target.closest('.q-opt'); if (!b) return;
      const qi = +b.closest('.q').dataset.q;
      answers[qi] = +b.dataset.o; store.set('quiz:' + id, answers); update();
    });
    update();
  }

  // ------------------------------------------------------------------ Pages
  function setNav(name) { $$('.topnav a').forEach(a => a.classList.toggle('active', a.dataset.nav === name)); }

  async function pageStatic(name, file) {
    setNav(name);
    main.className = 'main';
    main.innerHTML = '<article class="doc"><div class="loading">Loading...</div></article>';
    const html = await fetchText(file);
    main.innerHTML = `<article class="doc">${html}</article>`;
    Highlight.highlightAll(main);
    enhance(main);
    main.scrollTop = 0;
  }

  async function pageHome() {
    await pageStatic('home', 'pages/home.html');
    const s = await refreshStatus();
    const box = $('#homeStatus');
    if (box) {
      const row = (ok, label, hint) => `<li class="${ok ? 'ok' : 'bad'}"><span>${ok ? '&#10003;' : '&#10007;'}</span>${label}${ok ? '' : ' <em>' + hint + '</em>'}</li>`;
      box.innerHTML = s ? row(true, 'Local server (Node ' + s.node + ')') + row(s.msvc, 'Visual Studio C++ compiler', 'run setup.bat') + row(s.imguiSources, 'Dear ImGui sources in sandbox/external', 'run setup.bat')
                        : row(false, 'Local server', 'start with run.bat');
    }
    const c = completed(); const next = order.find(id => !c[id]) || order[0];
    const cont = $('#continueBtn'); if (cont && next) { cont.href = '#/lesson/' + next; cont.textContent = Object.keys(c).length ? 'Continue: ' + (lessonMeta[next] || {}).title : 'Start lesson 1'; }
  }

  async function pageLesson(id) {
    setNav('lesson');
    if (!lessonMeta[id]) { main.innerHTML = '<article class="doc"><h1>Lesson not found</h1></article>'; return; }
    const meta = lessonMeta[id];
    renderSidebar(id);
    main.className = 'main' + (meta.hasCode ? ' with-ide' : '');
    const idx = order.indexOf(id);
    const sec = manifest.sections.find(s => s.lessons.includes(id));
    const [html, code] = await Promise.all([
      fetchText(`lessons/${id}.html`).catch(() => '<p>Content missing.</p>'),
      meta.hasCode ? fetchText(`lessons/${id}.cpp`).catch(() => '') : Promise.resolve(''),
    ]);
    const prev = order[idx - 1], next = order[idx + 1];
    main.innerHTML = `
      <div class="lesson-wrap">
        <article class="doc lesson">
          <div class="crumb">${sec ? sec.title : ''} &middot; Lesson ${idx + 1} of ${order.length}${meta.minutes ? ' &middot; ~' + meta.minutes + ' min' : ''}</div>
          <h1>${meta.title}</h1>
          ${meta.summary ? `<p class="lead">${meta.summary}</p>` : ''}
          ${meta.hasCode ? '<div class="try-hint">&#9654; The editor on the right is live: change the code and press <b>Build &amp; Run</b> to see it in a real OpenGL window.</div>' : ''}
          <div class="lesson-body">${html}</div>
          <div id="quizHost"></div>
          <div class="lesson-foot">
            ${prev ? `<a class="btn ghost" href="#/lesson/${prev}">&larr; ${lessonMeta[prev].title}</a>` : '<span></span>'}
            <button class="btn" id="completeBtn">${completed()[id] ? '&#10003; Completed' : 'Mark as complete'}</button>
            ${next ? `<a class="btn primary" href="#/lesson/${next}">${lessonMeta[next].title} &rarr;</a>` : '<a class="btn primary" href="#/playground">Open the Playground &rarr;</a>'}
          </div>
        </article>
        ${meta.hasCode ? '<section class="ide-host" id="ideHost"></section>' : ''}
      </div>`;
    Highlight.highlightAll(main);
    enhance(main);
    renderQuiz($('#quizHost'), id, meta.quiz);
    $('#completeBtn').onclick = () => { const v = !completed()[id]; setCompleted(id, v); $('#completeBtn').innerHTML = v ? '&#10003; Completed' : 'Mark as complete'; };
    let ide = null;
    if (meta.hasCode) ide = createIDE($('#ideHost'), { storageKey: 'code:' + id, starter: code, title: meta.title });
    // "Load into editor" buttons inside lesson text
    $$('[data-load]', main).forEach(b => b.onclick = async () => {
      if (!ide) return;
      const src = b.dataset.load.startsWith('#') ? $(b.dataset.load).textContent.replace(/^\n/, '') : await fetchText(b.dataset.load);
      if (ide.editor.value !== src && !confirm('Replace the code in the editor with this example?')) return;
      ide.editor.value = src; store.set('code:' + id, src);
    });
    $('.lesson-wrap').scrollTop = 0; main.scrollTop = 0;
  }

  const SNIPPETS = [
    ['Window', 'ImGui::Begin("My Window");\n    ImGui::Text("Hello!");\nImGui::End();\n'],
    ['Button', 'if (ImGui::Button("Click me"))\n    Engine_Log("Clicked!");\n'],
    ['Checkbox', 'ImGui::Checkbox("Show grid", &gShowGrid);\n'],
    ['SliderFloat', 'ImGui::SliderFloat("FOV", &gCamera.fov, 20.0f, 100.0f, "%.0f deg");\n'],
    ['DragFloat3', 'if (gSelectedEntity >= 0)\n    ImGui::DragFloat3("Position", gEntities[gSelectedEntity].position, 0.05f);\n'],
    ['ColorEdit3', 'ImGui::ColorEdit3("Background", gClearColor);\n'],
    ['Combo', 'if (gSelectedEntity >= 0)\n    ImGui::Combo("Mesh", &gEntities[gSelectedEntity].mesh, gMeshNames, MESH_COUNT);\n'],
    ['Entity list', 'for (int i = 0; i < gEntityCount; i++)\n{\n    ImGui::PushID(i);\n    if (ImGui::Selectable(gEntities[i].name, gSelectedEntity == i))\n        gSelectedEntity = i;\n    ImGui::PopID();\n}\n'],
    ['Tree node', 'if (ImGui::TreeNode("Details"))\n{\n    ImGui::Text("Inside the tree");\n    ImGui::TreePop();\n}\n'],
    ['Collapsing header', 'if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))\n{\n    ImGui::Checkbox("Enabled", &gLight.enabled);\n    ImGui::ColorEdit3("Color", gLight.color);\n}\n'],
    ['Tab bar', 'if (ImGui::BeginTabBar("tabs"))\n{\n    if (ImGui::BeginTabItem("Scene"))  { ImGui::Text("Scene tab");  ImGui::EndTabItem(); }\n    if (ImGui::BeginTabItem("Render")) { ImGui::Text("Render tab"); ImGui::EndTabItem(); }\n    ImGui::EndTabBar();\n}\n'],
    ['Main menu bar', 'if (ImGui::BeginMainMenuBar())\n{\n    if (ImGui::BeginMenu("File"))\n    {\n        if (ImGui::MenuItem("Reset scene")) Engine_ResetScene();\n        if (ImGui::MenuItem("Quit", "Esc")) Engine_Quit();\n        ImGui::EndMenu();\n    }\n    ImGui::EndMainMenuBar();\n}\n'],
    ['Table', 'if (ImGui::BeginTable("ents", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))\n{\n    ImGui::TableSetupColumn("Name");\n    ImGui::TableSetupColumn("Mesh");\n    ImGui::TableSetupColumn("Visible");\n    ImGui::TableHeadersRow();\n    for (int i = 0; i < gEntityCount; i++)\n    {\n        ImGui::TableNextRow();\n        ImGui::TableNextColumn(); ImGui::TextUnformatted(gEntities[i].name);\n        ImGui::TableNextColumn(); ImGui::TextUnformatted(gMeshNames[gEntities[i].mesh]);\n        ImGui::TableNextColumn(); ImGui::Text(gEntities[i].visible ? "yes" : "no");\n    }\n    ImGui::EndTable();\n}\n'],
    ['Frame time plot', 'ImGui::PlotLines("Frame ms", gFrameTimes, FRAME_HISTORY, gFrameTimeOffset, NULL, 0.0f, 33.0f, ImVec2(0, 60));\n'],
    ['Tooltip', 'ImGui::Button("Hover me");\nImGui::SetItemTooltip("I am a tooltip");\n'],
    ['Modal popup', 'if (ImGui::Button("Delete all"))\n    ImGui::OpenPopup("Confirm");\nif (ImGui::BeginPopupModal("Confirm", NULL, ImGuiWindowFlags_AlwaysAutoResize))\n{\n    ImGui::Text("Delete every entity?");\n    if (ImGui::Button("Yes")) { Engine_ClearScene(); ImGui::CloseCurrentPopup(); }\n    ImGui::SameLine();\n    if (ImGui::Button("No")) ImGui::CloseCurrentPopup();\n    ImGui::EndPopup();\n}\n'],
    ['Image', 'ImGui::Image(Engine_TexID(0), ImVec2(96, 96));\n'],
    ['Draw list label', 'for (int i = 0; i < gEntityCount; i++)\n{\n    ImVec2 p;\n    if (Engine_WorldToScreen(gEntities[i].position, &p))\n        ImGui::GetForegroundDrawList()->AddText(p, IM_COL32(255, 255, 0, 255), gEntities[i].name);\n}\n'],
    ['Demo window', 'static bool showDemo = true;\nif (showDemo) ImGui::ShowDemoWindow(&showDemo);\n'],
  ];

  const PLAYGROUND_STARTER = `// Playground - anything goes!
// Engine API: see the "Engine API" page. Snippets on the left insert at the cursor.

void Lesson_Init(void)
{
    Engine_Log("Playground started");
}

void Lesson_UI(void)
{
    ImGui::Begin("Playground");
    ImGui::Text("Entities in scene: %d", gEntityCount);
    ImGui::ColorEdit3("Background", gClearColor);
    if (ImGui::Button("Reset scene"))
        Engine_ResetScene();
    ImGui::End();
}
`;

  // "UI code" mode: the editor holds only the BODY of Lesson_UI(). There are no function braces
  // to break, so any snippet, anywhere, is valid. #line 1 makes compiler errors point at the
  // editor's own line numbers.
  const PLAYGROUND_UI_STARTER = `// This code runs INSIDE Lesson_UI(), once every frame.
// Type any ImGui code or click a snippet - it is inserted at the cursor.
// Use 'static' for variables that must keep their value between frames.
static int clicks = 0;

ImGui::Begin("Playground");
ImGui::Text("Entities in scene: %d", gEntityCount);
ImGui::ColorEdit3("Background", gClearColor);
if (ImGui::Button("Click me"))
    clicks++;
ImGui::Text("Clicked %d times", clicks);
ImGui::End();
`;
  const wrapUI = code => 'void Lesson_Init(void)\n{\n    Engine_Log("Playground started");\n}\n\n' +
                         'void Lesson_UI(void)\n{\n#line 1\n' + code + '\n}\n';

  function pagePlayground() {
    setNav('playground');
    main.className = 'main';
    const mode = store.get('playgroundMode', 'ui');
    main.innerHTML = `
      <div class="playground">
        <aside class="snippets">
          <div class="mode-switch">
            <button class="${mode === 'ui' ? 'on' : ''}" data-mode="ui" title="Editor holds only the code inside Lesson_UI() - hard to break">UI code</button>
            <button class="${mode === 'full' ? 'on' : ''}" data-mode="full" title="Edit the whole Lesson.inl: your own functions, globals, Lesson_Init()">Full file</button>
          </div>
          <p class="muted small">${mode === 'ui'
            ? 'You write the <b>inside</b> of <code>Lesson_UI()</code>. Snippets go in at the cursor.'
            : 'Whole <code>Lesson.inl</code>. Snippets go inside <code>Lesson_UI()</code>.'}</p>
          <h3>Snippets</h3>
          ${SNIPPETS.map((s, i) => `<button class="snip" data-i="${i}">${s[0]}</button>`).join('')}
        </aside>
        <section class="ide-host wide" id="ideHost"></section>
      </div>`;
    const opts = mode === 'ui'
      ? { storageKey: 'code:playground-ui', starter: PLAYGROUND_UI_STARTER, title: 'Playground', wrap: wrapUI,
          fileName: 'Lesson_UI() { ... }', hint: 'runs inside <b>Lesson_UI()</b> every frame &middot; no function braces needed' }
      : { storageKey: 'code:playground', starter: PLAYGROUND_STARTER, title: 'Playground' };
    const { editor } = createIDE($('#ideHost'), opts);
    const state = $('.build-state', main);
    $$('.snip', main).forEach(b => b.onclick = () => {
      const where = editor.insertSnippet(SNIPPETS[+b.dataset.i][1], mode === 'ui');
      if (where === 'moved') state.innerHTML = '<span class="muted">cursor was outside a function - snippet added at the end of Lesson_UI()</span>';
      else if (where === 'wrapped') state.innerHTML = '<span class="muted">no Lesson_UI() found - snippet wrapped in a new one</span>';
      else state.textContent = '';
    });
    $$('.mode-switch button', main).forEach(b => b.onclick = () => { store.set('playgroundMode', b.dataset.mode); pagePlayground(); });
  }

  async function pageEngineApi() {
    await pageStatic('engine-api', 'pages/engine-api.html');
    const b = $('#viewEngineSrc');
    if (b) b.onclick = () => showFullSource('// (your Lesson.inl code goes here)');
  }

  // Small enhancements for static content: tabs and interactive demos
  function enhance(root) {
    $$('.tabs', root).forEach(tabs => {
      const btns = $$('.tab-btn', tabs), panes = $$('.tab-pane', tabs);
      btns.forEach((b, i) => b.onclick = () => { btns.forEach(x => x.classList.remove('on')); panes.forEach(x => x.classList.remove('on')); b.classList.add('on'); panes[i].classList.add('on'); });
      if (btns[0]) btns[0].click();
    });
    $$('[data-demo="frame-loop"]', root).forEach(el => frameLoopDemo(el));
  }

  // Animated "one frame" stepper for the immediate-mode lesson
  function frameLoopDemo(el) {
    const steps = [
      ['PeekMessage / WndProc', 'Win32 messages arrive. ImGui_ImplWin32_WndProcHandler() records mouse + keyboard into ImGuiIO.'],
      ['RenderScene()', 'Your engine draws the 3D world with OpenGL as usual.'],
      ['ImGui_Impl*_NewFrame()', 'Backends update display size, delta time and mouse position.'],
      ['ImGui::NewFrame()', 'ImGui starts a fresh frame. Nothing from last frame\'s UI code is kept - only window positions and widget state.'],
      ['Lesson_UI()  ← your code', 'You call Begin / Button / SliderFloat... Each call both DRAWS the widget and RETURNS what the user did with it.'],
      ['ImGui::Render()', 'All widgets are converted into vertex + index buffers (ImDrawData).'],
      ['ImGui_ImplOpenGL2_RenderDrawData()', 'The renderer backend draws those triangles on top of your scene.'],
      ['SwapBuffers()', 'The finished frame appears. Then everything repeats ~60 times per second.'],
    ];
    let i = 0, timer = null;
    el.innerHTML = `<div class="fl-steps">${steps.map((s, k) => `<div class="fl-step" data-k="${k}"><span>${k + 1}</span>${s[0]}</div>`).join('')}</div>
      <div class="fl-desc"></div><div class="fl-ctrl"><button class="btn small" data-a="prev">&larr;</button><button class="btn small" data-a="play">&#9654; Play</button><button class="btn small" data-a="next">&rarr;</button></div>`;
    function show() { $$('.fl-step', el).forEach((s, k) => s.classList.toggle('on', k === i)); $('.fl-desc', el).textContent = steps[i][1]; }
    el.addEventListener('click', e => {
      const a = e.target.dataset.a, k = e.target.closest('.fl-step');
      if (k) { i = +k.dataset.k; show(); }
      if (a === 'prev') { i = (i + steps.length - 1) % steps.length; show(); }
      if (a === 'next') { i = (i + 1) % steps.length; show(); }
      if (a === 'play') {
        if (timer) { clearInterval(timer); timer = null; e.target.innerHTML = '&#9654; Play'; }
        else { timer = setInterval(() => { if (!document.body.contains(el)) return clearInterval(timer); i = (i + 1) % steps.length; show(); }, 1400); e.target.innerHTML = '&#10074;&#10074; Pause'; }
      }
    });
    show();
  }

  // ------------------------------------------------------------------ Router
  async function route() {
    const h = location.hash || '#/home';
    const m = h.match(/^#\/([\w-]+)(?:\/(.+))?/);
    const page = m ? m[1] : 'home';
    document.body.classList.remove('sidebar-open');
    try {
      if (page === 'lesson') await pageLesson(m[2] || order[0]);
      else if (page === 'setup') await pageStatic('setup', 'pages/setup.html');
      else if (page === 'cheatsheet') await pageStatic('cheatsheet', 'pages/cheatsheet.html');
      else if (page === 'engine-api') await pageEngineApi();
      else if (page === 'playground') pagePlayground();
      else await pageHome();
      if (page !== 'lesson') renderSidebar('');
    } catch (e) {
      main.innerHTML = `<article class="doc"><h1>Could not load page</h1><p>${Highlight.escape(String(e.message || e))}</p><p>Make sure you started the site with <b>run.bat</b>.</p></article>`;
    }
  }

  $('#resetProgress').onclick = () => {
    if (!confirm('Reset all progress? Completed lessons, quiz answers and your edited code will be cleared.')) return;
    resetProgress(); renderSidebar(); route();
  };

  $('#sidebarToggle').onclick = () => document.body.classList.toggle('sidebar-open');
  window.addEventListener('hashchange', route);

  (async function init() {
    refreshStatus();
    setInterval(refreshStatus, 15000);
    try { await loadManifest(); } catch (e) { console.error(e); }
    route();
  })();
})();
