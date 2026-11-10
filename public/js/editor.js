// editor.js - lightweight code editor: textarea + highlighted overlay + gutter + error markers.
(function () {
  'use strict';

  class CodeEditor {
    constructor(host, { value = '', onChange = null, onRun = null } = {}) {
      this.onChange = onChange;
      this.onRun = onRun;
      this.marks = new Map();            // line -> 'error' | 'warning'
      host.classList.add('ce');
      host.innerHTML =
        '<div class="ce-gutter"></div>' +
        '<div class="ce-scroll">' +
        '  <pre class="ce-hl" aria-hidden="true"></pre>' +
        '  <textarea class="ce-input" spellcheck="false" autocapitalize="off" autocomplete="off" wrap="off"></textarea>' +
        '</div>';
      this.host = host;
      this.gutter = host.querySelector('.ce-gutter');
      this.hl = host.querySelector('.ce-hl');
      this.ta = host.querySelector('.ce-input');
      this.ta.value = value;
      this.ta.addEventListener('input', () => { this.marks.clear(); this.render(); this.onChange && this.onChange(this.ta.value); });
      this.ta.addEventListener('scroll', () => this.syncScroll());
      this.ta.addEventListener('keydown', (e) => this.onKey(e));
      this.render();
    }

    get value() { return this.ta.value; }
    set value(v) { this.ta.value = v; this.marks.clear(); this.render(); }

    setMarks(diags) {
      this.marks.clear();
      for (const d of diags || []) {
        if (d.file !== 'Lesson.inl') continue;
        if (this.marks.get(d.line) !== 'error') this.marks.set(d.line, d.severity);
      }
      this.render();
    }

    goToLine(line) {
      const lines = this.ta.value.split('\n');
      let pos = 0;
      for (let i = 0; i < Math.min(line - 1, lines.length); i++) pos += lines[i].length + 1;
      this.ta.focus();
      this.ta.setSelectionRange(pos, pos + (lines[line - 1] || '').length);
      const lh = parseFloat(getComputedStyle(this.ta).lineHeight) || 20;
      this.ta.scrollTop = Math.max(0, (line - 5) * lh);
      this.syncScroll();
    }

    render() {
      const code = this.ta.value;
      const lines = code.split('\n');
      const html = window.Highlight.highlight(code + (code.endsWith('\n') ? ' ' : '\n '));
      // wrap highlighted html per line so we can tint error lines
      const hlLines = html.split('\n');
      let open = [];             // carry open spans across line breaks (block comments)
      const out = hlLines.map((l, i) => {
        const prefix = open.join('');
        const tags = l.match(/<span class="[^"]+">|<\/span>/g) || [];
        for (const t of tags) { if (t === '</span>') open.pop(); else open.push(t); }
        const suffix = '</span>'.repeat(open.length);
        const mark = this.marks.get(i + 1);
        return '<span class="ce-line' + (mark ? ' ce-' + mark : '') + '">' + prefix + l + suffix + '</span>';
      });
      this.hl.innerHTML = out.join('\n');
      let g = '';
      for (let i = 1; i <= lines.length; i++) {
        const mark = this.marks.get(i);
        g += '<div class="' + (mark ? 'g-' + mark : '') + '">' + i + '</div>';
      }
      this.gutter.innerHTML = g;
      this.syncScroll();
    }

    syncScroll() {
      this.hl.style.transform = `translate(${-this.ta.scrollLeft}px, ${-this.ta.scrollTop}px)`;
      this.gutter.scrollTop = this.ta.scrollTop;
    }

    insert(text) {
      const ta = this.ta, s = ta.selectionStart, e = ta.selectionEnd;
      ta.setRangeText(text, s, e, 'end');
      ta.dispatchEvent(new Event('input'));
      ta.focus();
    }

    onKey(e) {
      const ta = this.ta;
      if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') { e.preventDefault(); this.onRun && this.onRun(); return; }
      if (e.key === 'Tab') {
        e.preventDefault();
        const s = ta.selectionStart, en = ta.selectionEnd, v = ta.value;
        if (s !== en && v.slice(s, en).includes('\n')) {           // indent / outdent block
          const ls = v.lastIndexOf('\n', s - 1) + 1;
          const block = v.slice(ls, en);
          const nb = e.shiftKey ? block.replace(/^ {1,4}/gm, '') : block.replace(/^/gm, '    ');
          ta.setRangeText(nb, ls, en, 'select');
        } else if (!e.shiftKey) ta.setRangeText('    ', s, en, 'end');
        ta.dispatchEvent(new Event('input'));
        return;
      }
      if (e.key === 'Enter') {                                     // keep indentation
        e.preventDefault();
        const s = ta.selectionStart, v = ta.value;
        const ls = v.lastIndexOf('\n', s - 1) + 1;
        const indent = (v.slice(ls, s).match(/^\s*/) || [''])[0];
        const extra = /\{\s*$/.test(v.slice(ls, s)) ? '    ' : '';
        ta.setRangeText('\n' + indent + extra, s, ta.selectionEnd, 'end');
        ta.dispatchEvent(new Event('input'));
        return;
      }
      if (e.key === '}') {                                         // auto-outdent closing brace
        const s = ta.selectionStart, v = ta.value;
        const ls = v.lastIndexOf('\n', s - 1) + 1;
        if (/^\s{4,}$/.test(v.slice(ls, s))) {
          e.preventDefault();
          ta.setRangeText(v.slice(ls, s - 4) + '}', ls, s, 'end');
          ta.dispatchEvent(new Event('input'));
        }
      }
    }
  }

  window.CodeEditor = CodeEditor;
})();
