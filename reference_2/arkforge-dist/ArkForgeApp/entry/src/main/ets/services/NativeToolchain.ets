import signer from 'libsigntool.so';
import native from 'libarkforge_script.so';
import { common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { SigningMaterials } from './SigningMaterials';
import { BuildProgress } from './BuildConsole';

/** Runs entirely inside the application's sandbox. No host RPC or subprocess. */
export class NativeToolchain {
  static async signingFingerprint(context: common.UIAbilityContext, bundleName: string): Promise<string> {
    const logical = context.filesDir + '/native-toolchain/signing' +
      (bundleName === 'com.djc.arkforge' ? '' : '/' + bundleName);
    await SigningMaterials.validate(logical, bundleName);
    const directory = SigningMaterials.directory(logical);
    const credentials = await SigningMaterials.load(logical);
    const fingerprints: string[] = [];
    for (const name of ['key.p12', 'app.cer', 'profile.p7b']) {
      fingerprints.push(await hash.hash(directory + '/' + name, 'sha256'));
    }
    // Passwords never enter cache metadata or logs.
    return fingerprints.join(':') + ':' + credentials.keyAlias + ':' + credentials.signAlg;
  }
  static signedPath(context: common.UIAbilityContext, bundleName: string): string {
    return context.filesDir + '/native-toolchain/' + (bundleName === 'com.djc.arkforge' ? 'device-signed-demo' : bundleName) + '.hap';
  }
  static async signDemo(context: common.UIAbilityContext, report?: BuildProgress, builtInput?: string,
    bundleName: string = 'com.djc.arkforge'): Promise<string> {
    const root = context.filesDir + '/native-toolchain';
    const logical = root + '/signing' + (bundleName === 'com.djc.arkforge' ? '' : '/' + bundleName);
    if (!fileIo.accessSync(logical)) fileIo.mkdirSync(logical, true);
    const directory = SigningMaterials.directory(logical);
    report?.('> ArkForge :loadSigningMaterials');
    const credentials = await SigningMaterials.load(logical);
    const input = builtInput || root + '/unsigned-demo.hap';
    const destination = NativeToolchain.signedPath(context, bundleName);
    const output = destination + '.pending-' + Date.now() + '.hap';
    if (!builtInput) {
      const bytes = await context.resourceManager.getRawFileContent('backend/unsigned-demo.hap');
      const file = fileIo.openSync(input,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      try { fileIo.writeSync(file.fd, bytes.buffer); } finally { fileIo.closeSync(file); }
    }
    const args = new Array<string>();
    args.push('sign-app', '-mode', 'localSign', '-keyAlias', credentials.keyAlias,
      '-keyPwd', credentials.keyPassword, '-keystorePwd', credentials.storePassword,
      '-keystoreFile', directory + '/key.p12', '-appCertFile', directory + '/app.cer',
      '-profileFile', directory + '/profile.p7b', '-signAlg', credentials.signAlg,
      '-inFile', input, '-outFile', output, '-inForm', 'zip', '-compatibleVersion', '12');
    report?.('> hap-sign-tool sign-app -mode localSign -inFile ' + input + ' -outFile ' + output +
      ' -keyPwd ****** -keystorePwd ******');
    const code = await signer.runSigner(args);
    if (code !== 0 || !fileIo.accessSync(output)) throw new Error('本机签名失败，返回码 ' + code);
    const verify = new Array<string>();
    verify.push('verify-app', '-inFile', output, '-outCertChain', root + '/device-signed.cer',
      '-outProfile', root + '/device-signed.p7b');
    report?.('> hap-sign-tool verify-app -inFile ' + output);
    const verified = await signer.runSigner(verify);
    const ok = verified === 0;
    hilog.info(0x0000, 'ArkForgeNative', 'Device HAP sign and verify ok=%{public}s', String(ok));
    if (!ok) throw new Error('本机产物验签失败');
    fileIo.renameSync(output, destination);
    return '本机进程内签名及验签完成：' + destination +
      (builtInput ? '\n输入为本机源码编译打包生成的 HAP。' : '\n输入为开发机预构建的无签名 HAP；此测试尚未验证本机源码构建。');
  }

  static async verifySigner(context: common.UIAbilityContext, report?: BuildProgress): Promise<string> {
    const root = context.filesDir + '/native-toolchain';
    if (!fileIo.accessSync(root)) fileIo.mkdirSync(root, true);
    const input = root + '/verify-demo.hap';
    hilog.info(0x0000, 'ArkForgeNative', 'Preparing native HAP verification');
    const loader = native.inspectLibrary('libsigntool.so');
    hilog.info(0x0000, 'ArkForgeNative', 'Signer loader: %{public}s', loader);
    if (loader !== 'loaded') throw new Error(loader);
    const bytes = await context.resourceManager.getRawFileContent('backend/verify-demo.hap');
    const file = fileIo.openSync(input,
      fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, bytes.buffer); } finally { fileIo.closeSync(file); }
    // Build mutable argument lists explicitly. The connected firmware crashes
    // in Createarraywithbuffer when restoring this async frame's literal array.
    const versionArgs = new Array<string>();
    versionArgs.push('-v');
    report?.('> hap-sign-tool -v');
    const version = await signer.runSigner(versionArgs);
    hilog.info(0x0000, 'ArkForgeNative', 'Native signer version check: %{public}d', version);
    const verifyArgs = new Array<string>();
    verifyArgs.push('verify-app', '-inFile', input, '-outCertChain', root + '/verified.cer',
      '-outProfile', root + '/verified.p7b');
    report?.('> hap-sign-tool verify-app -inFile ' + input);
    const code = await signer.runSigner(verifyArgs);
    const cert = fileIo.accessSync(root + '/verified.cer');
    const profile = fileIo.accessSync(root + '/verified.p7b');
    const ok = code === 0 && cert && profile;
    const result = JSON.stringify({ stage: 'native-hap-verify', ok: ok, code: code,
      certificateWritten: cert, profileWritten: profile });
    hilog.info(0x0000, 'ArkForgeNative', '%{public}s', result);
    if (!ok) throw new Error(result);
    return '本机 ARM64 签名库已在 App 进程内完成 HAP 验签。\n' + result +
      '\n这项检查不代表源码构建、签名或本机安装闭环已经完成。';
  }
}
