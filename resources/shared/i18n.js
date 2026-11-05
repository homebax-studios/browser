// Tiny translation helper. Dictionaries live in resources/translations/<lang>.json (cs, en).
(function () {
  let dict = {};
  window.t = (key, vars) => {
    let s = Object.prototype.hasOwnProperty.call(dict, key) ? dict[key] : key;
    if (vars) for (const k in vars) s = s.split('{' + k + '}').join(vars[k]);
    return s;
  };
  window.applyI18n = (root) => {
    const r = root || document;
    r.querySelectorAll('[data-i18n]').forEach((e) => { e.textContent = t(e.dataset.i18n); });
    r.querySelectorAll('[data-i18n-ph]').forEach((e) => { e.placeholder = t(e.dataset.i18nPh); });
    r.querySelectorAll('[data-i18n-title]').forEach((e) => { e.title = t(e.dataset.i18nTitle); e.setAttribute('aria-label', e.title); });
  };
  window.loadLang = async (pref) => {
    const lang = pref === 'cs' || pref === 'en' ? pref : ((navigator.language || 'en').toLowerCase().startsWith('cs') ? 'cs' : 'en');
    try {
      const res = await fetch('/translations/' + lang + '.json');
      dict = await res.json();
    } catch (e) { dict = {}; }
    document.documentElement.lang = lang;
    applyI18n();
    return lang;
  };
  window.applyTheme = (dark, accent) => {
    const root = document.documentElement;
    root.dataset.theme = dark ? 'dark' : 'light';
    if (accent && /^#[0-9a-f]{6}$/i.test(accent)) {
      root.style.setProperty('--accent', accent);
      const n = parseInt(accent.slice(1), 16);
      const lum = (0.299 * (n >> 16) + 0.587 * ((n >> 8) & 255) + 0.114 * (n & 255)) / 255;
      root.style.setProperty('--accent-fg', lum > 0.62 ? '#111111' : '#ffffff');
    }
  };
  // Common bootstrap for hb:// pages.
  window.initPage = async () => {
    const load = async () => {
      const [settings, info] = await Promise.all([hb.call('settings.get'), hb.call('app.info')]);
      await loadLang(settings.language);
      applyTheme(info.dark, settings.accent);
      hydrateIcons();
      return { settings, info };
    };
    const first = await load();
    hb.on('event', async () => {
      const state = await load();
      window.dispatchEvent(new CustomEvent('hb-settings', { detail: state }));
    });
    return first;
  };
})();
