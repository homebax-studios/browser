// Message bridge to the native side (WebView2 postMessage). Pages use hb.call(); the browser UI uses hb.post().
(function () {
  if (window.top !== window.self) { document.documentElement.remove(); throw new Error("framing is not allowed"); }
  const wv = window.chrome && window.chrome.webview;
  const pending = new Map();
  const handlers = {};
  let counter = 0;

  const hb = {
    available: !!wv,
    post(message) { if (wv) wv.postMessage(message); },
    call(cmd, args) {
      return new Promise((resolve, reject) => {
        if (!wv) { reject(new Error('not running inside HB Browser')); return; }
        const id = ++counter;
        pending.set(id, { resolve, reject });
        wv.postMessage({ id, cmd, args: args || {} });
      });
    },
    on(type, fn) { (handlers[type] = handlers[type] || []).push(fn); },
  };

  if (wv) {
    wv.addEventListener('message', (e) => {
      const m = e.data;
      if (!m || typeof m !== 'object') return;
      if (m.type === 'reply') {
        const p = pending.get(m.id);
        if (!p) return;
        pending.delete(m.id);
        if (m.ok) p.resolve(m.result); else p.reject(new Error(String(m.result)));
        return;
      }
      (handlers[m.type] || []).forEach((fn) => fn(m));
    });
  }
  window.hb = hb;
})();
