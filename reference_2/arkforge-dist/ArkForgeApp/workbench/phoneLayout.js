// Adapt the existing Code-OSS workbench; all actions still use its command service.
export function installPhoneLayout(code, win = window, doc = document) {
  const query = win.matchMedia('(max-width: 600px)');
  let disposed = false, entered = false, scheduled = false, ready = false, updating = false;
  let saved = null, chatWasVisible = false, maximizeChat = false, showPrimary = false;
  const invoke = (id, ...args) => code.commands.executeCommand(id, ...args);
  const visible = selector => {
    const element = doc.querySelector(selector);
    return !!element && element.getBoundingClientRect().width > 0 && element.getBoundingClientRect().height > 0;
  };
  async function update() {
    if (disposed || !ready) return;
    const phone = query.matches;
    doc.documentElement.classList.toggle('arkforge-phone', phone);
    await invoke('_setContext', 'arkforge.compactLayout', phone);
    if (phone && !entered) {
      entered = true;
      saved = { sidebar: visible('.part.sidebar'), chat: visible('.part.auxiliarybar'), panel: visible('.part.panel') };
      await invoke('workbench.action.closeSidebar');
      await invoke('workbench.action.closeAuxiliaryBar');
      await invoke('workbench.action.closePanel');
    } else if (!phone && entered) {
      entered = false;
      await invoke('workbench.action.restoreAuxiliaryBar');
      if (saved?.sidebar && !visible('.part.sidebar')) await invoke('workbench.action.toggleSidebarVisibility');
      if (saved?.panel && !visible('.part.panel')) await invoke('workbench.action.togglePanel');
      if (saved?.chat && !visible('.part.auxiliarybar')) await invoke('workbench.action.toggleAuxiliaryBar');
      saved = null;
    }
    if (phone && showPrimary) {
      showPrimary = false; maximizeChat = false;
      await invoke('workbench.action.closeAuxiliaryBar');
      if (!visible('.part.sidebar')) await invoke('workbench.action.toggleSidebarVisibility');
    }
    if (phone && maximizeChat && visible('.part.auxiliarybar')) {
      maximizeChat = false;
      await invoke('workbench.action.maximizeAuxiliaryBar');
    }
    if (!phone) doc.documentElement.style.removeProperty('--arkforge-viewport-height');
  }
  function schedule() {
    if (scheduled || disposed) return;
    scheduled = true;
    win.requestAnimationFrame(async () => {
      if (updating) { scheduled = false; schedule(); return; }
      scheduled = false; updating = true;
      try { await update(); } catch (error) { console.error('[ArkForgePhone]', error); }
      finally { updating = false; }
    });
  }
  let previousHeight = 0;
  function viewport() {
    if (!query.matches) return;
    const height = Math.floor(Math.min(win.innerHeight, win.visualViewport?.height || win.innerHeight));
    // Ignore pinch zoom: it should pan the visual viewport, rather than relayout the IDE.
    if (win.visualViewport && Math.abs(win.visualViewport.scale - 1) > 0.01) return;
    if (height > 0 && Math.abs(height - previousHeight) > 1) {
      previousHeight = height;
      doc.documentElement.style.setProperty('--arkforge-viewport-height', height + 'px');
      win.dispatchEvent(new Event('resize'));
    }
  }
  function pickFile(event) {
    if (!query.matches) return;
    const row = event.target.closest?.('.part.sidebar .explorer-item');
    if (!row || row.classList.contains('folder-icon')) return;
    const treeRow = row.closest('.monaco-list-row');
    if (treeRow?.hasAttribute('aria-expanded')) return;
    // Code-OSS opens the selected file before its own sidebar is dismissed.
    win.setTimeout(() => { if (!disposed) invoke('workbench.action.closeSidebar').catch(console.error); }, 150);
  }
  const observer = new MutationObserver(() => {
    if (!query.matches || !ready) return;
    const chat = visible('.part.auxiliarybar');
    if (chat && !chatWasVisible) { maximizeChat = true; schedule(); }
    chatWasVisible = chat;
    if (chat && visible('.part.sidebar') && !maximizeChat && !updating) {
      showPrimary = true; schedule();
    }
  });
  query.addEventListener('change', schedule);
  win.visualViewport?.addEventListener('resize', viewport);
  doc.addEventListener('click', pickFile, true);
  invoke('workbench.action.closeQuickOpen').then(() => {
    if (disposed) return;
    const root = doc.querySelector('.monaco-workbench');
    if (root) observer.observe(root, { attributes: true, attributeFilter: ['class'] });
    ready = true; viewport(); schedule();
  }).catch(console.error);
  return {
    isPhone: () => query.matches,
    revealEditor: async () => {
      if (!query.matches) return;
      await invoke('workbench.action.closeAuxiliaryBar');
      await invoke('workbench.action.closeSidebar');
      await invoke('workbench.action.focusActiveEditorGroup');
    },
    dispose() {
      disposed = true; observer.disconnect();
      query.removeEventListener('change', schedule);
      win.visualViewport?.removeEventListener('resize', viewport);
      doc.removeEventListener('click', pickFile, true);
    }
  };
}
