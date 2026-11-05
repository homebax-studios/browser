// Built-in icon set (24x24, stroke based). Everything is inline so the browser works fully offline.
(function () {
  const dots = '<g fill="currentColor" stroke="none"><circle cx="12" cy="5.5" r="1.6"/><circle cx="12" cy="12" r="1.6"/><circle cx="12" cy="18.5" r="1.6"/></g>';
  const P = {
    back: '<path d="M15 5l-7 7 7 7"/>',
    forward: '<path d="M9 5l7 7-7 7"/>',
    reload: '<path d="M20 11a8 8 0 1 0-2.3 5.7"/><path d="M20 4v7h-7"/>',
    close: '<path d="M6 6l12 12M18 6L6 18"/>',
    home: '<path d="M4 11l8-7 8 7"/><path d="M6 10v9h12v-9"/>',
    star: '<path d="M12 4l2.4 5 5.4.7-4 3.8 1 5.4-4.8-2.6-4.8 2.6 1-5.4-4-3.8 5.4-.7z"/>',
    download: '<path d="M12 4v11M7 11l5 5 5-5"/><path d="M5 20h14"/>',
    menu: dots,
    plus: '<path d="M12 5v14M5 12h14"/>',
    minus: '<path d="M5 12h14"/>',
    minimize: '<path d="M5 12h14"/>',
    maximize: '<rect x="5" y="5" width="14" height="14" rx="1.5"/>',
    restore: '<rect x="8" y="5" width="11" height="11" rx="1.5"/><path d="M5 8v10a1.5 1.5 0 0 0 1.5 1.5H16"/>',
    lock: '<rect x="5" y="11" width="14" height="9" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>',
    alert: '<path d="M12 4l9 16H3z"/><path d="M12 10v5M12 17.5v.5"/>',
    info: '<circle cx="12" cy="12" r="8"/><path d="M12 11v5M12 8v.5"/>',
    search: '<circle cx="11" cy="11" r="6"/><path d="M16 16l4 4"/>',
    history: '<circle cx="12" cy="12" r="8"/><path d="M12 8v4l3 2"/>',
    bookmark: '<path d="M7 4h10v16l-5-4-5 4z"/>',
    folder: '<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
    globe: '<circle cx="12" cy="12" r="8"/><path d="M4 12h16M12 4c3 3 3 13 0 16M12 4c-3 3-3 13 0 16"/>',
    sliders: '<path d="M4 7h10M18 7h2M4 17h2M10 17h10"/><circle cx="16" cy="7" r="2"/><circle cx="8" cy="17" r="2"/>',
    trash: '<path d="M5 7h14M9 7V5h6v2M7 7l1 12h8l1-12"/>',
    external: '<path d="M14 5h5v5M19 5l-8 8"/><path d="M17 14v4a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1V8a1 1 0 0 1 1-1h4"/>',
    volume: '<path d="M5 10v4h3l5 4V6L8 10z"/><path d="M16 9a4 4 0 0 1 0 6"/>',
    'volume-off': '<path d="M5 10v4h3l5 4V6L8 10z"/><path d="M16 9l5 6M21 9l-5 6"/>',
    pin: '<path d="M9 4h6l-1 6 3 3H7l3-3z"/><path d="M12 13v7"/>',
    sun: '<circle cx="12" cy="12" r="4"/><path d="M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6L7 7M17 17l1.4 1.4M5.6 18.4L7 17M17 7l1.4-1.4"/>',
    moon: '<path d="M19 14.5A7.5 7.5 0 0 1 9.5 5 7.5 7.5 0 1 0 19 14.5z"/>',
    shield: '<path d="M12 4l7 3v5c0 4-3 7-7 8-4-1-7-4-7-8V7z"/>',
    check: '<path d="M5 12l5 5 9-10"/>',
    chevron: '<path d="M7 10l5 5 5-5"/>',
    file: '<path d="M7 4h7l4 4v12H7z"/><path d="M14 4v4h4"/>',
    eye: '<path d="M3 12s3.5-6 9-6 9 6 9 6-3.5 6-9 6-9-6-9-6z"/><circle cx="12" cy="12" r="2.5"/>',
    fullscreen: '<path d="M5 9V5h4M19 9V5h-4M5 15v4h4M19 15v4h-4"/>',
    print: '<rect x="6" y="3" width="12" height="6"/><rect x="4" y="9" width="16" height="8" rx="2"/><rect x="7" y="14" width="10" height="6"/>',
    code: '<path d="M9 8l-4 4 4 4M15 8l4 4-4 4"/>',
    power: '<path d="M12 4v8"/><path d="M7 7a7 7 0 1 0 10 0"/>',
    window: '<rect x="4" y="5" width="16" height="14" rx="2"/><path d="M4 9h16"/>',
    case: '<path d="M4 18l4-11 4 11M5.5 14h5M14 18v-6a3 3 0 0 1 6 0v6M14 15h6"/>',
    pause: '<path d="M9 5v14M15 5v14"/>',
    play: '<path d="M8 5l11 7-11 7z"/>',
    edit: '<path d="M5 19l1-4L16 5l3 3L9 18z"/>',
    upload: '<path d="M12 16V5M7 9l5-5 5 5"/><path d="M5 20h14"/>',
    zzz: '<path d="M5 6h6l-6 7h6M14 12h5l-5 6h5"/>',
  };
  window.HB_ICONS = P;
  window.icon = (name, size) => {
    const s = size || 18;
    return '<svg class="ic" width="' + s + '" height="' + s + '" viewBox="0 0 24 24" fill="none" stroke="currentColor" ' +
      'stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + (P[name] || '') + '</svg>';
  };
  window.hydrateIcons = (root) => {
    (root || document).querySelectorAll('[data-icon]').forEach((el) => {
      el.innerHTML = icon(el.dataset.icon, Number(el.dataset.size) || 18);
    });
  };
})();
