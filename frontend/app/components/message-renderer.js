(function (global) {
  'use strict';

  var md = null;

  function initMarkdown() {
    if (typeof markdownit !== 'undefined' && !md) {
      md = markdownit({ html: false, linkify: true, typographer: true });
    }
  }

  function renderMarkdown(text) {
    initMarkdown();
    if (!text) return '';
    return md ? md.render(text) : escapeHtml(text);
  }

  function renderKatex(html) {
    if (typeof katex === 'undefined') return html;
    html = html.replace(/\$\$(.+?)\$\$/g, function (match, expr) {
      try { return katex.renderToString(expr, { displayMode: true }); }
      catch (e) { return match; }
    });
    html = html.replace(/\$(.+?)\$/g, function (match, expr) {
      try { return katex.renderToString(expr, { displayMode: false }); }
      catch (e) { return match; }
    });
    return html;
  }

  function renderMessage(text) {
    return renderKatex(renderMarkdown(text));
  }

  function escapeHtml(text) {
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  global.DaffyRenderer = {
    renderMessage: renderMessage,
    renderMarkdown: renderMarkdown,
    renderKatex: renderKatex,
    escapeHtml: escapeHtml
  };
})(window);
