// highlight.js - tiny C/C++ syntax highlighter tuned for ImGui + engine code.
(function () {
  'use strict';
  const KEYWORDS = new Set(('if else for while do switch case default break continue return goto sizeof typedef struct enum union ' +
    'static const extern inline volatile register auto void bool char short int long float double signed unsigned ' +
    'true false nullptr NULL TRUE FALSE class public private protected namespace using new delete template typename ' +
    'this operator virtual override constexpr static_cast reinterpret_cast const_cast').split(' '));
  const TYPES = new Set(('ImVec2 ImVec4 ImU32 ImGuiIO ImGuiStyle ImDrawList ImFont ImTextureID ImGuiID ImGuiTextFilter ' +
    'ImGuiWindowFlags ImGuiTreeNodeFlags ImGuiTableFlags ImGuiInputTextFlags ImGuiColorEditFlags ImGuiSelectableFlags ' +
    'ImGuiTableSortSpecs ImGuiInputTextCallbackData ImGuiPayload ImGuiListClipper ImFontConfig ImGuiKey ' +
    'Entity Camera Light LogLine TextureAsset MeshType LogLevel ' +
    'HWND HDC HGLRC HINSTANCE LRESULT WPARAM LPARAM UINT DWORD BOOL MSG WNDCLASSEX RECT ' +
    'GLuint GLint GLfloat GLdouble GLenum GLsizei size_t intptr_t uint8_t uint32_t int32_t FILE').split(' '));

  function esc(s) { return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

  // One regex, alternation order matters.
  const RE = new RegExp([
    '(\\/\\/[^\\n]*)',                                   // 1 line comment
    '(\\/\\*[\\s\\S]*?(?:\\*\\/|$))',                    // 2 block comment
    '("(?:[^"\\\\\\n]|\\\\.)*"?|\'(?:[^\'\\\\\\n]|\\\\.)*\'?)', // 3 string/char
    '(^[ \\t]*#[ \\t]*\\w+)',                            // 4 preprocessor directive
    '(\\b(?:0x[0-9a-fA-F]+|\\d+\\.?\\d*(?:[eE][+-]?\\d+)?f?)\\b)', // 5 number
    '\\b(ImGui(?:_Impl\\w+)?)(::|_)?(\\w*)',            // 6,7,8 ImGui namespace / backend funcs
    '\\b(IM_COL32\\w*|IM_ARRAYSIZE|IMGUI_CHECKVERSION|ImGui\\w+_\\w+)\\b', // 9 ImGui macros/enum values
    '\\b(Engine_\\w+|Lesson_\\w+)\\b',                   // 10 engine functions
    '\\b(g[A-Z]\\w*)\\b',                                // 11 engine globals
    '\\b(MESH_\\w+|LOG_\\w+|MAX_\\w+|FRAME_HISTORY|GL_\\w+|WM_\\w+|VK_\\w+)\\b', // 12 constants
    '\\b([A-Za-z_]\\w*)\\b(?=\\s*\\()',                  // 13 function call
    '\\b([A-Za-z_]\\w*)\\b',                             // 14 identifier
  ].join('|'), 'gm');

  function highlight(code) {
    let out = '', last = 0, m;
    RE.lastIndex = 0;
    while ((m = RE.exec(code))) {
      if (m.index > last) out += esc(code.slice(last, m.index));
      last = RE.lastIndex;
      if (m[0] === '') { RE.lastIndex++; continue; }
      if (m[1] || m[2]) out += '<span class="t-com">' + esc(m[0]) + '</span>';
      else if (m[3]) out += '<span class="t-str">' + esc(m[0]) + '</span>';
      else if (m[4]) out += '<span class="t-pre">' + esc(m[0]) + '</span>';
      else if (m[5]) out += '<span class="t-num">' + esc(m[0]) + '</span>';
      else if (m[6]) {
        out += '<span class="t-ns">' + esc(m[6]) + '</span>';
        if (m[7]) out += '<span class="t-op">' + esc(m[7]) + '</span>';
        if (m[8]) out += '<span class="t-fn">' + esc(m[8]) + '</span>';
      }
      else if (m[9]) out += '<span class="t-mac">' + esc(m[0]) + '</span>';
      else if (m[10]) out += '<span class="t-eng">' + esc(m[0]) + '</span>';
      else if (m[11]) out += '<span class="t-glob">' + esc(m[0]) + '</span>';
      else if (m[12]) out += '<span class="t-mac">' + esc(m[0]) + '</span>';
      else if (m[13]) out += (KEYWORDS.has(m[13]) ? '<span class="t-kw">' : TYPES.has(m[13]) ? '<span class="t-type">' : '<span class="t-call">') + esc(m[0]) + '</span>';
      else if (m[14]) out += KEYWORDS.has(m[14]) ? '<span class="t-kw">' + m[0] + '</span>' : TYPES.has(m[14]) ? '<span class="t-type">' + m[0] + '</span>' : esc(m[0]);
      else out += esc(m[0]);
    }
    if (last < code.length) out += esc(code.slice(last));
    return out;
  }

  // Highlight all static <pre><code class="cpp"> blocks inside an element.
  function highlightAll(root) {
    root.querySelectorAll('pre > code').forEach(el => {
      if (el.dataset.hl) return;
      el.dataset.hl = '1';
      const src = el.textContent.replace(/^\n/, '').replace(/\s+$/, '');
      const plain = el.classList.contains('txt') || el.classList.contains('bat');
      if (plain) el.textContent = src; else el.innerHTML = highlight(src);
      const pre = el.parentElement;
      if (!pre.querySelector('.copy-btn')) {
        const b = document.createElement('button');
        b.className = 'copy-btn'; b.textContent = 'Copy';
        b.onclick = () => { navigator.clipboard.writeText(src); b.textContent = 'Copied'; setTimeout(() => b.textContent = 'Copy', 1200); };
        pre.appendChild(b);
      }
    });
  }

  window.Highlight = { highlight, highlightAll, escape: esc };
})();
