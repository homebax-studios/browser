// HB Browser shell: tab strip, address bar, menus. Talks to the native side with flat {cmd, ...} messages.
(() => {
  'use strict';
  const $ = (s) => document.querySelector(s);
  const tr = (k, v) => window.t(k, v);
  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  const post = (cmd, extra) => hb.post(Object.assign({ cmd }, extra || {}));

  const el = {
    tabs: $('#tabs'), url: $('#url'), omni: $('#omni'), sec: $('#sec'), star: $('#star'), back: $('#back'), forward: $('#forward'),
    reload: $('#reload'), home: $('#home'), bmbar: $('#bmbar'), menu: $('#menu'), ctx: $('#ctx'), folder: $('#folder'),
    suggest: $('#suggest'), dlpop: $('#dlpop'), find: $('#find'), findInput: $('#findInput'), findCount: $('#findCount'),
    toast: $('#toast'), wmax: $('#wmax'), badge: $('#privateBadge'), shell: $('#shell'), dlbtn: $('#dlbtn'), menubtn: $('#menubtn'),
  };
  let state = { tabs: [], activeId: -1, nav: null, bookmarks: [], settings: {}, dark: false, window: {} };
  let lang = '';
  let dragging = false, pendingState = null, drag = null;
  let suggestItems = [], suggestSel = 0, suggestSeq = 0, typedValue = '', suggestTimer = 0;
  let downloads = [], findCase = false, toastTimer = 0;

  // ------------------------------------------------------------------ helpers
  const human = (b) => {
    const u = ['B', 'KB', 'MB', 'GB', 'TB']; let v = Math.max(0, b || 0), i = 0;
    while (v >= 1024 && i < 4) { v /= 1024; i++; }
    return (i === 0 ? v.toFixed(0) : v.toFixed(1)) + ' ' + u[i];
  };
  const colorFor = (s) => { let h = 0; for (const c of String(s)) h = (h * 31 + c.charCodeAt(0)) % 360; return 'hsl(' + h + ' 55% 48%)'; };
  const hostOf = (u) => { try { return new URL(u).hostname.replace(/^www\./, ''); } catch (e) { return u; } };
  const isNewTab = (u) => !u || u === 'hb://newtab';
  const pretty = (u) => {
    if (isNewTab(u)) return '';
    if (u.startsWith('hb://')) return u;
    try {
      const x = new URL(u);
      if (x.protocol === 'http:' || x.protocol === 'https:')
        return x.host.replace(/^www\./, '') + (x.pathname === '/' && !x.search && !x.hash ? '' : x.pathname + x.search + x.hash);
    } catch (e) { /* fall through */ }
    return u;
  };
  const internalIcon = (u) => ({ 'hb://settings': 'sliders', 'hb://bookmarks': 'bookmark', 'hb://history': 'history', 'hb://downloads': 'download' }[u.split(/[?#]/)[0]] || 'globe');

  // ------------------------------------------------------------------ popups
  const reportOverlay = () => requestAnimationFrame(() => {
    const rects = [...document.querySelectorAll('.popup:not([hidden])')].map((p) => {
      const r = p.getBoundingClientRect();
      return [Math.floor(r.left), Math.floor(r.top), Math.ceil(r.width), Math.ceil(r.height)];
    });
    post('overlay', { rects });
  });
  const closePopups = (except, keepFind) => {
    document.querySelectorAll('.popup').forEach((p) => { if (p !== except && !(keepFind && p === el.find) && p !== el.toast) p.hidden = true; });
    reportOverlay();
  };
  const place = (p, x, y, alignRight) => {
    p.hidden = false;
    const w = p.offsetWidth, h = p.offsetHeight;
    let left = alignRight ? x - w : x, top = y;
    left = Math.max(6, Math.min(left, innerWidth - w - 6));
    top = Math.max(2, Math.min(top, innerHeight - h - 6));
    p.style.left = left + 'px'; p.style.top = top + 'px';
    reportOverlay();
  };
  const below = (btn) => { const r = btn.getBoundingClientRect(); return [r.left, r.bottom + 4, r.right]; };

  const showContext = (items, x, y) => {
    closePopups(el.ctx);
    el.ctx.innerHTML = items.map((it, i) => it === '-' ? '<div class="sep"></div>'
      : '<div class="mi' + (it.disabled ? ' disabled' : '') + '" data-i="' + i + '">' + (it.icon ? icon(it.icon, 16) : '<span style="width:16px"></span>') + '<span>' + esc(it.label) + '</span></div>').join('');
    el.ctx.onclick = (e) => {
      const m = e.target.closest('.mi'); if (!m || m.classList.contains('disabled')) return;
      const it = items[+m.dataset.i]; closePopups(); post(it.cmd, it.args);
    };
    place(el.ctx, x, y);
  };

  // ------------------------------------------------------------------ tabs
  const tabTitle = (tb) => tb.title || (isNewTab(tb.url) ? tr('tabNew') : tb.url);
  const favHtml = (tb) => {
    if (tb.loading) return '<i class="spin"></i>';
    if (tb.url.startsWith('hb://')) return icon(internalIcon(tb.url), 16);
    if (tb.favicon && /^https?:/i.test(tb.favicon)) return '<img alt="" src="' + esc(tb.favicon) + '" onerror="this.replaceWith(Object.assign(document.createElement(\'span\'),{innerHTML:window.icon(\'globe\',16)}))">';
    return icon('globe', 16);
  };
  const renderTabs = () => {
    if (dragging) { pendingState = true; return; }
    el.tabs.innerHTML = state.tabs.map((tb) => {
      const cls = 'tab' + (tb.id === state.activeId ? ' active' : '') + (tb.pinned ? ' pinned' : '') + (tb.sleeping ? ' sleeping' : '');
      const title = tabTitle(tb) + (tb.sleeping ? ' — ' + tr('sleeping') : '');
      return '<div class="' + cls + '" role="tab" data-id="' + tb.id + '" title="' + esc(title) + '">' +
        '<span class="fav">' + favHtml(tb) + '</span><span class="title">' + esc(tabTitle(tb)) + '</span>' +
        (tb.private ? '<span class="priv">' + icon('eye', 14) + '</span>' : '') +
        (tb.audible || tb.muted ? '<button class="tbtn audio" data-act="mute">' + icon(tb.muted ? 'volume-off' : 'volume', 14) + '</button>' : '') +
        '<button class="tbtn close" data-act="close">' + icon('close', 13) + '</button></div>';
    }).join('');
  };

  el.tabs.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    const tabEl = e.target.closest('.tab');
    if (!tabEl || e.target.closest('[data-act]')) return;
    const id = +tabEl.dataset.id;
    closePopups(null, true);
    if (id !== state.activeId) post('activateTab', { id });
    drag = { id, el: tabEl, startX: e.clientX, moved: false, rects: [...el.tabs.children].map((c) => c.getBoundingClientRect()) };
    tabEl.setPointerCapture(e.pointerId);
  });
  el.tabs.addEventListener('pointermove', (e) => {
    if (!drag) return;
    const dx = e.clientX - drag.startX;
    if (!drag.moved && Math.abs(dx) < 6) return;
    drag.moved = true; dragging = true;
    drag.el.style.transform = 'translateX(' + dx + 'px)'; drag.el.style.zIndex = 5;
  });
  el.tabs.addEventListener('pointerup', (e) => {
    if (!drag) return;
    const d = drag; drag = null;
    if (d.moved) {
      const dx = e.clientX - d.startX;
      const myIndex = [...el.tabs.children].indexOf(d.el);
      const center = d.rects[myIndex].left + d.rects[myIndex].width / 2 + dx;
      let index = 0;
      d.rects.forEach((r, i) => { if (i !== myIndex && r.left + r.width / 2 < center) index++; });
      post('moveTab', { id: d.id, index });
    }
    d.el.style.transform = ''; d.el.style.zIndex = '';
    dragging = false;
    if (pendingState) { pendingState = null; renderTabs(); }
  });
  el.tabs.addEventListener('click', (e) => {
    const act = e.target.closest('[data-act]'); if (!act) return;
    const id = +act.closest('.tab').dataset.id;
    if (act.dataset.act === 'close') post('closeTab', { id }); else post('muteTab', { id });
  });
  el.tabs.addEventListener('auxclick', (e) => {
    const tabEl = e.target.closest('.tab');
    if (e.button === 1 && tabEl) { e.preventDefault(); post('closeTab', { id: +tabEl.dataset.id }); }
  });
  el.tabs.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const tabEl = e.target.closest('.tab'); if (!tabEl) return;
    const id = +tabEl.dataset.id, tb = state.tabs.find((x) => x.id === id); if (!tb) return;
    showContext([
      { icon: 'reload', label: tr('t_reload'), cmd: 'reloadTab', args: { id } },
      { icon: 'window', label: tr('t_duplicate'), cmd: 'duplicateTab', args: { id } },
      { icon: 'pin', label: tb.pinned ? tr('t_unpin') : tr('t_pin'), cmd: 'pinTab', args: { id, pinned: !tb.pinned } },
      { icon: tb.muted ? 'volume' : 'volume-off', label: tb.muted ? tr('t_unmute') : tr('t_mute'), cmd: 'muteTab', args: { id } },
      '-',
      { icon: 'close', label: tr('t_close'), cmd: 'closeTab', args: { id } },
      { label: tr('t_closeOthers'), cmd: 'closeOthers', args: { id } },
      { label: tr('t_closeRight'), cmd: 'closeRight', args: { id } },
      '-',
      { icon: 'history', label: tr('t_reopen'), cmd: 'reopenClosed' },
    ], e.clientX, e.clientY);
  });
  $('#newtab').addEventListener('click', () => post('newTab'));

  // ------------------------------------------------------------------ window controls
  $('#drag').addEventListener('pointerdown', (e) => { if (e.button === 0 && e.detail < 2) post('drag'); });
  $('#drag').addEventListener('dblclick', () => post('maximize'));
  $('#wmin').addEventListener('click', () => post('minimize'));
  el.wmax.addEventListener('click', () => post('maximize'));
  $('#wclose').addEventListener('click', () => post('close'));

  // ------------------------------------------------------------------ toolbar + omnibox
  el.back.addEventListener('click', () => post('back'));
  el.forward.addEventListener('click', () => post('forward'));
  el.reload.addEventListener('click', () => post(state.nav && state.nav.loading ? 'stop' : 'reload'));
  el.home.addEventListener('click', () => post('home'));
  el.star.addEventListener('click', () => post('bookmark'));

  const realUrl = () => (state.nav && !isNewTab(state.nav.url) ? state.nav.url : '');
  const renderNav = () => {
    const n = state.nav;
    el.back.disabled = !n || !n.canBack; el.forward.disabled = !n || !n.canForward;
    el.reload.innerHTML = icon(n && n.loading ? 'close' : 'reload', 18);
    el.reload.title = tr(n && n.loading ? 'stop' : 'reload');
    el.home.hidden = state.settings.showHomeButton === false;
    el.star.classList.toggle('on', !!(n && n.bookmarked));
    el.star.title = tr(n && n.bookmarked ? 'bookmarkRemove' : 'bookmarkThis');
    const sec = n ? n.security : 'none';
    const map = { secure: ['lock', ''], insecure: ['alert', tr('insecure')], internal: ['shield', ''], file: ['file', tr('localFile')], none: ['info', ''] };
    const [ic, label] = map[sec] || map.none;
    el.sec.className = 'icon-btn' + (sec === 'insecure' ? ' insecure' : '');
    el.sec.innerHTML = icon(ic, 16) + (label ? '<span>' + esc(label) + '</span>' : '');
    el.sec.title = tr('sec_' + sec);
    if (document.activeElement !== el.url) el.url.value = n ? pretty(n.url) : '';
    el.badge.hidden = !(n && n.private);
    el.badge.innerHTML = icon('eye', 14) + '<span>' + esc(tr('privateBadge')) + '</span>';
    el.wmax.innerHTML = icon(state.window.maximized ? 'restore' : 'maximize', 15);
    el.wmax.title = tr(state.window.maximized ? 'restore' : 'maximize');
  };

  el.url.addEventListener('focus', () => { el.url.value = realUrl(); requestAnimationFrame(() => el.url.select()); });
  el.url.addEventListener('blur', () => { setTimeout(() => { if (document.activeElement !== el.url) { el.suggest.hidden = true; reportOverlay(); renderNav(); } }, 120); });
  el.url.addEventListener('input', () => {
    typedValue = el.url.value; clearTimeout(suggestTimer);
    if (!typedValue.trim()) { el.suggest.hidden = true; reportOverlay(); return; }
    suggestTimer = setTimeout(() => post('suggest', { q: typedValue.trim(), seq: ++suggestSeq }), 70);
    renderSuggest(true);
  });
  el.url.addEventListener('keydown', (e) => {
    const open = !el.suggest.hidden;
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      if (!open) return;
      e.preventDefault();
      const n = suggestItems.length;
      suggestSel = (suggestSel + (e.key === 'ArrowDown' ? 1 : -1) + n) % n;
      const it = suggestItems[suggestSel];
      el.url.value = suggestSel === 0 ? typedValue : it.url;
      renderSuggest();
    } else if (e.key === 'Enter') {
      e.preventDefault();
      el.suggest.hidden = true; reportOverlay();
      post('navigate', { text: el.url.value, newTab: e.altKey });
    } else if (e.key === 'Escape') {
      if (open) { el.suggest.hidden = true; reportOverlay(); }
      else { el.url.value = realUrl(); post('focusContent'); }
    }
  });
  const looksLikeUrl = (s) => !/\s/.test(s) && (/^[a-z][a-z0-9+.-]*:\/\//i.test(s) || /\./.test(s) || /^localhost/i.test(s));
  const renderSuggest = (fromTyping) => {
    const q = typedValue.trim();
    if (!q) { el.suggest.hidden = true; return; }
    if (fromTyping) { suggestSel = 0; }
    const head = { type: looksLikeUrl(q) ? 'go' : 'search', url: q, title: q };
    const rest = suggestItems.filter((x) => x.url !== q).slice(0, 6);
    suggestItems = [head].concat(rest.length && fromTyping !== true ? rest : suggestItems.slice(1));
    el.suggest.innerHTML = suggestItems.map((it, i) => {
      const ic = it.type === 'go' ? 'globe' : it.type === 'search' ? 'search' : it.type === 'bookmark' ? 'star' : 'history';
      const main = it.type === 'go' ? tr('goTo', { q: it.url }) : it.type === 'search' ? tr('searchFor', { q: it.url }) : (it.title || it.url);
      const sub = it.type === 'bookmark' || it.type === 'history' ? '<div class="su">' + esc(it.url) + '</div>' : '';
      return '<div class="sg' + (i === suggestSel ? ' sel' : '') + '" data-i="' + i + '">' + icon(ic, 17) + '<div class="tt"><div class="st">' + esc(main) + '</div>' + sub + '</div></div>';
    }).join('');
    const r = el.omni.getBoundingClientRect();
    el.suggest.style.width = r.width + 'px';
    place(el.suggest, r.left, r.bottom + 4);
  };
  el.suggest.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    const row = e.target.closest('.sg'); if (!row) return;
    const it = suggestItems[+row.dataset.i];
    el.suggest.hidden = true; reportOverlay();
    post('navigate', { text: it.url, newTab: e.button === 1 });
  });

  // ------------------------------------------------------------------ bookmarks bar
  const bookmarkMenu = (b, x, y) => showContext([
    { icon: 'external', label: tr('openNewTab'), cmd: 'newTab', args: { url: b.url } },
    { icon: 'trash', label: tr('delete'), cmd: 'bookmarkRemove', args: { url: b.url } },
  ], x, y);
  const renderBookmarks = () => {
    const show = state.settings.showBookmarksBar !== false;
    el.bmbar.hidden = !show;
    if (!show) return;
    const loose = state.bookmarks.filter((b) => !b.folder);
    const folders = {};
    state.bookmarks.filter((b) => b.folder).forEach((b) => { (folders[b.folder] = folders[b.folder] || []).push(b); });
    let html = Object.keys(folders).map((f) => '<button class="bm" data-folder="' + esc(f) + '" data-opens>' + icon('folder', 15) + '<span class="bt">' + esc(f) + '</span></button>').join('');
    html += loose.map((b) => '<button class="bm" data-url="' + esc(b.url) + '" title="' + esc(b.url) + '"><span class="dot" style="background:' + colorFor(hostOf(b.url)) + '">' +
      esc((b.title || hostOf(b.url)).trim().charAt(0).toUpperCase()) + '</span><span class="bt">' + esc(b.title || hostOf(b.url)) + '</span></button>').join('');
    el.bmbar.innerHTML = html || '<span class="hintline">' + esc(tr('noBookmarksHint')) + '</span>';
    el.bmbar._folders = folders;
  };
  el.bmbar.addEventListener('click', (e) => {
    const b = e.target.closest('.bm'); if (!b) return;
    if (b.dataset.folder) {
      const items = el.bmbar._folders[b.dataset.folder] || [];
      closePopups(el.folder);
      el.folder.innerHTML = items.map((x, i) => '<div class="mi" data-i="' + i + '"><span class="dot" style="width:16px;height:16px;border-radius:5px;background:' + colorFor(hostOf(x.url)) + '"></span><span>' + esc(x.title || hostOf(x.url)) + '</span></div>').join('');
      el.folder.onclick = (ev) => { const m = ev.target.closest('.mi'); if (m) { closePopups(); post('navigate', { text: items[+m.dataset.i].url }); } };
      const r = b.getBoundingClientRect(); place(el.folder, r.left, r.bottom + 4);
    } else post('navigate', { text: b.dataset.url });
  });
  el.bmbar.addEventListener('auxclick', (e) => { const b = e.target.closest('.bm[data-url]'); if (b && e.button === 1) post('navigate', { text: b.dataset.url, newTab: true }); });
  el.bmbar.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const b = e.target.closest('.bm[data-url]'); if (b) bookmarkMenu({ url: b.dataset.url }, e.clientX, e.clientY);
  });

  // ------------------------------------------------------------------ main menu
  const menuRows = () => [
    ['plus', 'm_newTab', 'Ctrl+T', 'newTab'], ['window', 'm_newWindow', 'Ctrl+N', 'newWindow'],
    ['eye', 'm_privateTab', '', 'newPrivateTab'], ['eye', 'm_privateWindow', 'Ctrl+Shift+N', 'newPrivateWindow'], '-',
    ['bookmark', 'm_bookmarks', '', 'openPage', { name: 'bookmarks' }], ['history', 'm_history', 'Ctrl+H', 'openPage', { name: 'history' }],
    ['download', 'm_downloads', 'Ctrl+J', 'openPage', { name: 'downloads' }], '-', 'ZOOM',
    ['search', 'm_find', 'Ctrl+F', 'openFind'], ['print', 'm_print', 'Ctrl+P', 'print'], ['file', 'm_savePdf', '', 'savePdf'], ['window', 'm_screenshot', '', 'screenshot'],
    ['file', 'm_reader', '', 'reader'], ['sun', 'm_night', '', 'pageNight'], ['code', 'm_devtools', 'F12', 'devtools'],
    ['file', 'm_viewSource', 'Ctrl+U', 'viewSource'], '-',
    ['sliders', 'm_settings', 'Ctrl+,', 'openPage', { name: 'settings' }], ['info', 'm_about', '', 'openPage', { name: 'about' }], ['power', 'm_exit', '', 'exit'],
  ];
  const openMenu = () => {
    closePopups(el.menu);
    const z = state.nav ? state.nav.zoom : 100;
    el.menu.innerHTML = menuRows().map((r, i) => r === '-' ? '<div class="sep"></div>' : r === 'ZOOM'
      ? '<div class="zoomrow"><span class="grow">' + esc(tr('m_zoom')) + '</span><button class="icon-btn" data-z="zoomOut">' + icon('minus', 16) + '</button><button class="zv" data-z="zoomReset" style="border:0;background:none">' + z + ' %</button><button class="icon-btn" data-z="zoomIn">' + icon('plus', 16) + '</button><button class="icon-btn" data-z="fullscreen" title="F11">' + icon('fullscreen', 16) + '</button></div>'
      : '<div class="mi" data-i="' + i + '">' + icon(r[0], 17) + '<span>' + esc(tr(r[1])) + '</span><span class="kbd">' + r[2] + '</span></div>').join('');
    el.menu.onclick = (e) => {
      const zb = e.target.closest('[data-z]');
      if (zb) { post(zb.dataset.z); if (zb.dataset.z === 'fullscreen') closePopups(); return; }
      const m = e.target.closest('.mi'); if (!m) return;
      const r = menuRows()[+m.dataset.i]; closePopups();
      if (r[3] === 'openFind') openFind(); else post(r[3], r[4]);
    };
    const [x, y, right] = below(el.menubtn); place(el.menu, x, y, true); el.menu.style.left = Math.min(right - el.menu.offsetWidth, innerWidth - el.menu.offsetWidth - 6) + 'px';
    reportOverlay();
  };
  el.menubtn.addEventListener('click', () => { if (!el.menu.hidden) closePopups(); else openMenu(); });

  // ------------------------------------------------------------------ find in page
  const doFind = (backwards) => post('find', { text: el.findInput.value, backwards: !!backwards, caseSensitive: findCase });
  const openFind = () => {
    closePopups(el.find);
    el.find.style.top = (el.shell.getBoundingClientRect().height + 6) + 'px';
    el.find.style.right = '14px'; el.find.style.left = 'auto';
    el.find.hidden = false; reportOverlay();
    post('focusUi'); // gives the UI view keyboard focus, then we move it to the find box
    setTimeout(() => { el.findInput.focus(); el.findInput.select(); }, 60);
  };
  const closeFind = () => { el.find.hidden = true; el.findCount.textContent = ''; reportOverlay(); post('findClose'); };
  el.findInput.addEventListener('input', () => doFind(false));
  el.findInput.addEventListener('keydown', (e) => { if (e.key === 'Enter') doFind(e.shiftKey); else if (e.key === 'Escape') closeFind(); });
  $('#findNext').addEventListener('click', () => doFind(false));
  $('#findPrev').addEventListener('click', () => doFind(true));
  $('#findClose').addEventListener('click', closeFind);
  $('#findCase').addEventListener('click', (e) => { findCase = !findCase; e.currentTarget.classList.toggle('on', findCase); doFind(false); });

  // ------------------------------------------------------------------ downloads popover
  const renderDownloads = () => {
    if (el.dlpop.hidden) return;
    const rows = downloads.map((d) => {
      const active = d.state === 'progress' || d.state === 'paused';
      const pct = d.total > 0 ? Math.min(100, Math.round(d.received * 100 / d.total)) : 0;
      const sub = active ? (d.state === 'paused' ? tr('dlPaused') + ' · ' : '') + (d.total > 0 ? pct + ' % · ' + human(d.received) + ' / ' + human(d.total) : human(d.received))
        : tr('dl_' + d.state) + (d.state === 'done' ? ' · ' + human(d.total) : '');
      const btns = active
        ? '<button class="icon-btn" data-dl="' + (d.state === 'paused' ? 'Resume' : 'Pause') + '" data-id="' + d.id + '">' + icon(d.state === 'paused' ? 'play' : 'pause', 15) + '</button><button class="icon-btn" data-dl="Cancel" data-id="' + d.id + '">' + icon('close', 15) + '</button>'
        : (d.state === 'done' ? '<button class="icon-btn" data-dl="Show" data-id="' + d.id + '" title="' + esc(tr('showInFolder')) + '">' + icon('folder', 15) + '</button>' : '') + '<button class="icon-btn" data-dl="Remove" data-id="' + d.id + '">' + icon('trash', 15) + '</button>';
      return '<div class="dl"><span class="badge-ic">' + icon('file', 20) + '</span><div class="meta" data-dl="' + (d.state === 'done' ? 'Open' : '') + '" data-id="' + d.id + '"><div class="nm">' + esc(d.name || d.url) + '</div>' +
        (active ? '<div class="bar"><i style="width:' + pct + '%"></i></div>' : '') + '<div class="sub">' + esc(sub) + '</div></div>' + btns + '</div>';
    }).join('');
    el.dlpop.innerHTML = (rows || '<div class="pophint">' + esc(tr('noDownloads')) + '</div>') +
      '<div class="sep"></div><div class="mi" data-all>' + icon('external', 16) + '<span>' + esc(tr('showAll')) + '</span></div>';
    reportOverlay();
  };
  const openDownloads = () => {
    closePopups(el.dlpop);
    el.dlpop.hidden = false; renderDownloads(); post('downloadsRefresh');
    const [x, y, right] = below(el.dlbtn); place(el.dlpop, x, y, true); el.dlpop.style.left = Math.min(right - el.dlpop.offsetWidth, innerWidth - el.dlpop.offsetWidth - 6) + 'px';
  };
  el.dlbtn.addEventListener('click', () => { if (!el.dlpop.hidden) closePopups(); else openDownloads(); });
  el.dlpop.addEventListener('click', (e) => {
    if (e.target.closest('[data-all]')) { closePopups(); post('openPage', { name: 'downloads' }); return; }
    const b = e.target.closest('[data-dl]'); if (!b || !b.dataset.dl) return;
    post('download' + b.dataset.dl, { id: +b.dataset.id });
  });

  // ------------------------------------------------------------------ toast
  const toast = (key) => {
    el.toast.textContent = tr('toast_' + key); el.toast.hidden = false; reportOverlay();
    clearTimeout(toastTimer); toastTimer = setTimeout(() => { el.toast.hidden = true; reportOverlay(); }, 2600);
  };

  // ------------------------------------------------------------------ global events
  document.addEventListener('pointerdown', (e) => {
    if (e.target.closest('.popup') || e.target.closest('[data-opens]') || e.target.closest('#omni') || e.target.closest('#tabs')) return;
    closePopups(null, true);
  }, true);
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape' && !el.menu.hidden) closePopups(); });
  document.addEventListener('contextmenu', (e) => e.preventDefault());
  window.addEventListener('focus', () => post('uiFocus', { focused: true }));
  window.addEventListener('blur', () => post('uiFocus', { focused: false }));
  new ResizeObserver(() => post('layout', { height: Math.ceil(el.shell.getBoundingClientRect().height) })).observe(el.shell);
  window.addEventListener('resize', reportOverlay);

  // ------------------------------------------------------------------ messages from native
  const renderAll = () => { renderTabs(); renderNav(); renderBookmarks(); };
  hb.on('state', async (m) => {
    const bookmarks = m.bookmarks || state.bookmarks;
    state = Object.assign({}, m, { bookmarks });
    applyTheme(m.dark, m.settings.accent);
    if (m.settings.language !== lang) { lang = m.settings.language; await loadLang(lang); hydrateIcons(); }
    renderAll();
  });
  hb.on('suggestions', (m) => {
    if (m.seq !== suggestSeq || document.activeElement !== el.url) return;
    const head = suggestItems[0] || { type: 'search', url: typedValue, title: typedValue };
    suggestItems = [head].concat(m.items.filter((x) => x.url !== head.url));
    renderSuggest();
  });
  hb.on('focusOmnibox', () => { closePopups(); el.url.focus(); el.url.select(); });
  hb.on('findResult', (m) => { el.findCount.textContent = el.findInput.value ? String(m.count) : ''; });
  hb.on('downloads', (m) => { downloads = m.items; renderDownloads(); });
  hb.on('openDownloads', () => openDownloads());
  hb.on('openFind', openFind);
  hb.on('toast', (m) => toast(m.key));
  hb.on('dismiss', () => closePopups(null, true));

  (async () => {
    await loadLang('auto'); hydrateIcons();
    post('ready');
  })();
})();
