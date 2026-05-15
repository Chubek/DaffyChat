(function () {
  if (typeof window === 'undefined' || !window.hicon || !window.hicon.icons) return;

  var MAP = {
    'bxs-chat': 'message-circle',
    'bxs-book-open': 'book-open',
    'bxs-home': 'home',
    'bxs-sun': 'sun',
    'bxs-moon': 'moon',
    'bxs-microphone': 'mic',
    'bxs-message-add': 'message-square-plus',
    'bxs-door-open': 'log-in',
    'bxs-check-circle': 'check-circle',
    'bxs-copy': 'copy',
    'bxs-log-in-circle': 'log-in',
    'bxs-info-circle': 'info',
    'bxs-phone-call': 'phone-call',
    'bxs-terminal': 'terminal',
    'bxs-x-circle': 'x-circle',
    'bxs-star': 'star',
    'bxl-github': 'github',
    'bxs-heart': 'heart',
    'bxs-group': 'users',
    'bxs-volume-mute': 'volume-off',
    'bxs-volume-full': 'volume-high',
    'bxs-volume-low': 'volume-low',
    'bxs-microphone-off': 'mic-off',
    'bxs-phone-off': 'phone-off',
    'bxs-signal-5': 'activity',
    'bxs-message-dots': 'message-circle',
    'bxs-extension': 'puzzle',
    'bxs-smile': 'smile',
    'bxs-paperclip': 'paperclip',
    'bxs-send': 'send',
    'bxs-link': 'link',
    'bxs-chip': 'cpu',
    'bxs-broadcast': 'radio',
    'bxs-bolt': 'zap-on',
    'bxs-microphone-alt': 'mic',
    'bxs-layer': 'layers',
    'bxs-search': 'search',
    'bxs-search-alt-2': 'search',
    'bxs-plug': 'plug',
    'bxs-cog': 'settings',
    'bxs-wrench': 'tool',
    'bxs-list-ul': 'list',
    'bxs-rocket': 'rocket',
    'bxs-server': 'server',
    'bxs-code-block': 'code',
    'bxs-headphone': 'headphones',
    'bxs-network-chart': 'share-2',
    'bxs-hourglass': 'hourglass'
  };

  function mapIcon(el) {
    var cls = Array.from(el.classList).find(function (c) { return c.indexOf('bx') === 0 && c !== 'bx'; });
    if (!cls) return null;
    return MAP[cls] || null;
  }

  function replaceIn(root) {
    var icons = root.querySelectorAll ? root.querySelectorAll('i.bx') : [];
    icons.forEach(function (el) {
      var name = mapIcon(el);
      if (!name || !window.hicon.icons[name]) return;
      var size = window.getComputedStyle(el).fontSize || '1em';
      var extraClass = Array.from(el.classList).filter(function (c) { return c !== 'bx' && c.indexOf('bxs-') !== 0 && c.indexOf('bxl-') !== 0; }).join(' ');
      var svg = window.hicon.icons[name].toSvg({
        width: size,
        height: size,
        class: extraClass,
        style: el.getAttribute('style') || ''
      });
      var doc = new DOMParser().parseFromString(svg, 'image/svg+xml');
      var node = doc.querySelector('svg');
      if (!node) return;
      node.setAttribute('aria-hidden', 'true');
      el.replaceWith(node);
    });
  }

  document.addEventListener('DOMContentLoaded', function () {
    replaceIn(document);
    var obs = new MutationObserver(function (mutations) {
      mutations.forEach(function (m) {
        m.addedNodes.forEach(function (n) {
          if (n.nodeType === 1) {
            if (n.matches && n.matches('i.bx')) replaceIn(n.parentNode || document);
            else replaceIn(n);
          }
        });
      });
    });
    obs.observe(document.body, { childList: true, subtree: true });
  });
})();
