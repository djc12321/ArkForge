const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

(async () => {
  const source = fs.readFileSync(path.join(__dirname, '../workbench/phoneLayout.js'), 'utf8');
  const { installPhoneLayout } = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const settle = () => new Promise(resolve => setTimeout(resolve, 30));
  let watch;
  global.MutationObserver = class {
    constructor(callback) { watch = callback; }
    observe() {} disconnect() {}
  };
  function setup(phone) {
    const commands = [], listeners = {}, properties = new Map(), classes = new Set();
    const state = { sidebar: true, chat: false, panel: true };
    const root = { classList: { contains: key => key === 'noauxiliarybar' ? !state.chat : classes.has(key) } };
    const query = { matches: phone, addEventListener: (_, fn) => { listeners.change = fn; }, removeEventListener() {} };
    const doc = {
      documentElement: { classList: { toggle: (key, value) => value ? classes.add(key) : classes.delete(key) }, style: {
        setProperty: (key, value) => properties.set(key, value), removeProperty: key => properties.delete(key)
      } },
      querySelector: selector => selector === '.monaco-workbench' ? root : ({
        getBoundingClientRect: () => ({ width: state[selector.split('.')[2] === 'auxiliarybar' ? 'chat' : selector.split('.')[2]] ? 300 : 0, height: 600 })
      }),
      addEventListener: (name, fn) => { listeners[name] = fn; }, removeEventListener() {}
    };
    const win = { matchMedia: () => query, requestAnimationFrame: fn => setTimeout(fn, 0), setTimeout,
      innerHeight: 800, visualViewport: { height: 400, scale: 1, addEventListener: (_, fn) => { listeners.viewport = fn; }, removeEventListener() {} },
      dispatchEvent() {}
    };
    const code = { commands: { async executeCommand(id, ...args) {
      commands.push([id, ...args]);
      if (id.endsWith('closeSidebar')) state.sidebar = false;
      if (id.endsWith('closeAuxiliaryBar')) state.chat = false;
      if (id.endsWith('closePanel')) state.panel = false;
      if (id.endsWith('toggleSidebarVisibility')) state.sidebar = !state.sidebar;
      if (id.endsWith('togglePanel')) state.panel = !state.panel;
      if (id.endsWith('maximizeAuxiliaryBar')) classes.add('auxiliarybarmaximized');
    } } };
    return { controller: installPhoneLayout(code, win, doc), commands, state, query, listeners, properties, classes, win };
  }
  const wide = setup(false); await settle();
  assert(!wide.commands.some(([id]) => /closeSidebar|closePanel|closeAuxiliaryBar/.test(id)), 'wide layout must retain existing panes');
  wide.controller.dispose();
  const phone = setup(true); await settle();
  assert(!phone.state.sidebar && !phone.state.panel);
  assert.equal(phone.properties.get('--arkforge-viewport-height'), '400px', 'software keyboard must reduce available height');
  phone.win.visualViewport.scale = 2; phone.win.visualViewport.height = 200; phone.listeners.viewport();
  assert.equal(phone.properties.get('--arkforge-viewport-height'), '400px', 'pinch zoom must not reflow editor');
  phone.state.chat = true; watch(); await settle();
  assert(phone.commands.some(([id]) => id === 'workbench.action.maximizeAuxiliaryBar'));
  phone.state.sidebar = true; watch(); await settle();
  assert(!phone.state.chat && phone.state.sidebar, 'opening primary sidebar from maximized chat must keep the requested sidebar visible');
  await phone.controller.revealEditor(); assert(!phone.state.chat);
  phone.query.matches = false; phone.listeners.change(); await settle();
  assert(phone.state.sidebar && phone.state.panel, 'wide-screen panes must be restored');
  assert(!phone.properties.has('--arkforge-viewport-height'));
  const count = phone.commands.length; phone.controller.dispose(); phone.listeners.change(); await settle();
  assert.equal(phone.commands.length, count);
  console.log('PASS: native compact layout, keyboard resize, zoom preservation, chat maximize, wide-layout restoration and disposal.');
})().catch(error => { console.error(error); process.exitCode = 1; });
