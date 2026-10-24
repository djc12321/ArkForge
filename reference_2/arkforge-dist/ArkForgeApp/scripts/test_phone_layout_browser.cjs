const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
// Standalone checkouts can install playwright normally, or set ARKFORGE_PLAYWRIGHT_PATH.
const bundled = path.resolve(__dirname, '../../../../_vendor/vscode/node_modules/playwright');
const { chromium } = require(process.env.ARKFORGE_PLAYWRIGHT_PATH || (fs.existsSync(bundled) ? bundled : 'playwright'));

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  try {
    const page = await browser.newPage();
    const css = fs.readFileSync(path.join(__dirname, '../workbench/phone.css'), 'utf8');
    for (const width of [320, 390, 540, 840, 1280]) {
      await page.setViewportSize({ width, height: 800 });
      await page.setContent(`<style>
        body { margin:0 } .monaco-workbench { position:relative; width:100%; height:800px }
        .quick-input-widget { position:absolute; width:600px; left:50%; margin-left:-300px }
        .monaco-dialog-box { width:500px; min-width:500px; padding:20px }
        ${css}</style><div class="monaco-workbench"><div class="quick-input-widget">Quick Input</div>
        <div class="monaco-dialog-box"><div class="dialog-message-container">${'long-project-name/'.repeat(40)}</div></div></div>`);
      const bounds = await page.evaluate(() => Object.fromEntries(['.quick-input-widget', '.monaco-dialog-box'].map(selector => {
        const r = document.querySelector(selector).getBoundingClientRect(); return [selector, { left:r.left, right:r.right, width:r.width }];
      })));
      if (width <= 600) {
        assert(bounds['.quick-input-widget'].left >= 0 && bounds['.quick-input-widget'].right <= width);
        assert(bounds['.monaco-dialog-box'].width <= width - 32);
      } else {
        assert.equal(bounds['.quick-input-widget'].width, 600, 'wide Quick Input keeps native dimensions');
        assert.equal(bounds['.monaco-dialog-box'].width, 540, 'wide dialog keeps native dimensions');
      }
    }
    console.log('PASS: real Chromium layout at 320/390/540/840/1280px; narrow dialogs and Quick Input fit, wide styles remain unchanged.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
