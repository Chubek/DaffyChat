(function (global) {
  'use strict';

  function getTheme() {
    return localStorage.getItem('daffy-theme') || 'light';
  }

  function setTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('daffy-theme', theme);
  }

  function toggleTheme() {
    var current = getTheme();
    var next = current === 'dark' ? 'light' : 'dark';
    setTheme(next);
    return next;
  }

  function initTheme() {
    setTheme(getTheme());
  }

  global.DaffyTheme = {
    get: getTheme,
    set: setTheme,
    toggle: toggleTheme,
    init: initTheme
  };
})(window);
