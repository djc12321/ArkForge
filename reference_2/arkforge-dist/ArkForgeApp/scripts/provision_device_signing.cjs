/* One-time provisioning to the debug app's private sandbox. Never packages keys in a HAP. */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const cp = require('child_process');
const vm = require('vm');
const plugin = 'C:/Program Files/Huawei/DevEco Studio/tools/hvigor/hvigor-ohos-plugin';
const json5 = require(path.join(plugin, 'node_modules/json5'));
// Use the installed SDK implementation, without initializing the Hvigor build
// engine or allowing its error logger to print signing inputs.
const sdkExports = {};
const decipherPath = path.join(plugin, 'src/utils/decipher-util.js');
vm.runInNewContext(fs.readFileSync(decipherPath, 'utf8'), {
  exports: sdkExports, __dirname: path.dirname(decipherPath),
  require: name => name === './log/ohos-logger.js' ? {
    OhosLogger: { getLogger: () => ({ printErrorExit: () => {
      throw new Error('SDK could not decrypt the existing signing configuration.');
    } }) }
  } : require(name)
}, { filename: decipherPath, timeout: 10000 });
const { DecipherUtil } = sdkExports;
const profilePath = process.argv[2];
if (!profilePath) throw new Error('Provide the previously authorized DevEco build-profile.json5 path.');
const profile = json5.parse(fs.readFileSync(profilePath, 'utf8'));
const product = profile.app.products.find(p => p.name === 'default');
const signing = profile.app.signingConfigs.find(s => s.name === product.signingConfig);
if (!signing) throw new Error('Default product has no signing configuration');
const material = signing.material;
const absolute = p => path.resolve(path.dirname(profilePath), p);
const store = absolute(material.storeFile);
const credentials = {
  keyAlias: material.keyAlias, signAlg: material.signAlg,
  storePassword: DecipherUtil.decryptPwd(path.dirname(store), material.storePassword, 'storePassword'),
  keyPassword: DecipherUtil.decryptPwd(path.dirname(store), material.keyPassword, 'keyPassword')
};
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'arkforge-provision-'));
const bundle = 'com.djc.arkforge';
const targetBundle = process.argv[3] || '';
if (targetBundle && !/^[A-Za-z0-9_.]+$/.test(targetBundle)) throw new Error('Invalid target bundle');
const remote = '/data/storage/el2/base/haps/entry/files/native-toolchain/signing/' + (targetBundle ? targetBundle + '/' : '');
function hdc(args) {
  const result = cp.spawnSync('hdc', args, { encoding: 'utf8', windowsHide: true });
  if (result.status !== 0 || /\[Fail\]|Permission denied|not found|Error:/i.test(result.stdout + result.stderr)) {
    throw new Error('Device provisioning failed; check debug authorization and the private signing directory.');
  }
}
try {
  for (const [name, source] of [['key.p12', store], ['app.cer', absolute(material.certpath)],
      ['profile.p7b', absolute(material.profile)]]) {
    const local = path.join(temporary, name);
    fs.copyFileSync(source, local);
    hdc(['file', 'send', '-b', bundle, local, remote + name]);
  }
  const manifest = path.join(temporary, 'provision.json');
  fs.writeFileSync(manifest, JSON.stringify(credentials), { mode: 0o600 });
  hdc(['file', 'send', '-b', bundle, manifest, remote + 'provision.json']);
  console.log('Provisioned signing materials into the app sandbox. No credentials were logged or added to the project.');
} finally {
  // Only remove the exact temporary directory created by mkdtemp above.
  fs.rmSync(temporary, { recursive: true, force: true });
}
