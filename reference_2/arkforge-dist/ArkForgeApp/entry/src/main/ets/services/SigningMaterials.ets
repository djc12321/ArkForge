import { asset } from '@kit.AssetStoreKit';
import { util } from '@kit.ArkTS';
import { fileIo } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { cert } from '@kit.DeviceCertificateKit';
import { zlib } from '@kit.BasicServicesKit';
import signer from 'libsigntool.so';
import resources from 'libarkforge_resources.so';
import { AtomicFiles } from './AtomicFiles';

interface ActiveSigning { set: string; }

export interface SigningCredentials {
  keyAlias: string;
  signAlg: string;
  keyPassword: string;
  storePassword: string;
}

/** Import passwords once into the OS encrypted asset store, then delete transport JSON. */
export class SigningMaterials {
  private static async saveCredentials(directory: string, credentials: SigningCredentials): Promise<void> {
    if (!credentials.keyAlias?.trim() || !credentials.storePassword ||
      !/^SHA(256|384)withECDSA$/.test(credentials.signAlg)) throw new Error('签名别名、密码或算法无效；当前官方原生签名器支持 SHA256withECDSA / SHA384withECDSA');
    const alias = new util.TextEncoder().encodeInto('ArkForgeDeviceSigning' +
      (directory.endsWith('/signing') ? '' : ':' + directory.substring(directory.lastIndexOf('/signing/') + '/signing/'.length)));
    const attributes: asset.AssetMap = new Map();
    attributes.set(asset.Tag.ALIAS, alias);
    attributes.set(asset.Tag.SECRET, new util.TextEncoder().encodeInto(JSON.stringify(credentials)));
    attributes.set(asset.Tag.CONFLICT_RESOLUTION, asset.ConflictResolution.OVERWRITE);
    await asset.add(attributes);
  }

  static async importFiles(context: common.UIAbilityContext, uris: string[], credentials: SigningCredentials, bundle: string): Promise<void> {
    if (!/^[A-Za-z0-9_.]+$/.test(bundle) || uris.length !== 3) throw new Error('签名文件或应用标识无效');
    const logical = context.filesDir + '/native-toolchain/signing' + (bundle==='com.example.harmonyplayground' ? '' : '/' + bundle);
    const set = String(Date.now()), directory = logical + '/.sets/' + set;
    fileIo.mkdirSync(directory, true);
    const names: string[] = ['key.p12','app.cer','profile.p7b'];
    for (let i = 0; i < names.length; i++) {
      const source = fileIo.openSync(uris[i], fileIo.OpenMode.READ_ONLY);
      try {
        const size = fileIo.statSync(source.fd).size;
        if (size < 1 || size > 16*1024*1024) throw new Error('签名文件为空或超过 16 MB');
        fileIo.copyFileSync(source.fd, directory + '/' + names[i]);
      } finally { fileIo.closeSync(source); }
    }
    // Passwords never pass through provision.json in the individual-file flow.
    await SigningMaterials.saveCredentials(directory, credentials);
    await SigningMaterials.validate(directory, bundle);
    await SigningMaterials.validateKey(directory);
    const active: ActiveSigning = {set:set};
    AtomicFiles.write(logical + '/active.json', JSON.stringify(active));
  }
  static directory(directory: string): string {
    if (!fileIo.accessSync(directory + '/active.json')) return directory;
    const active = JSON.parse(fileIo.readTextSync(directory + '/active.json')) as ActiveSigning;
    if (!/^\d+$/.test(active.set)) throw new Error('签名配置索引无效');
    return directory + '/.sets/' + active.set;
  }
  private static async validateKey(directory: string): Promise<void> {
    const credentials = await SigningMaterials.load(directory);
    const csr = directory + '/validation.csr';
    try {
      const code = await signer.runSigner(['generate-csr','-keyAlias',credentials.keyAlias,
        '-keyPwd',credentials.keyPassword,'-keystoreFile',directory+'/key.p12',
        '-keystorePwd',credentials.storePassword,'-signAlg',credentials.signAlg,
        '-subject','CN=ArkForge Signing Validation','-outFile',csr]);
      if (code !== 0) throw new Error('密钥库、密钥别名、密码或签名算法校验失败，原有签名配置已保留');
    } finally { if (fileIo.accessSync(csr)) fileIo.unlinkSync(csr); }
  }
  static async load(directory: string): Promise<SigningCredentials> {
    directory = SigningMaterials.directory(directory);
    const alias = new util.TextEncoder().encodeInto('ArkForgeDeviceSigning' +
      (directory.endsWith('/signing') ? '' : ':' + directory.substring(directory.lastIndexOf('/signing/') + '/signing/'.length)));
    const provision = directory + '/provision.json';
    if (fileIo.accessSync(provision)) {
      const text = fileIo.readTextSync(provision);
      const credentials = JSON.parse(text) as SigningCredentials;
      if (!credentials.keyAlias || !credentials.storePassword) throw new Error('签名材料不完整');
      const attributes: asset.AssetMap = new Map();
      attributes.set(asset.Tag.ALIAS, alias);
      attributes.set(asset.Tag.SECRET, new util.TextEncoder().encodeInto(text));
      attributes.set(asset.Tag.CONFLICT_RESOLUTION, asset.ConflictResolution.OVERWRITE);
      await asset.add(attributes);
      fileIo.unlinkSync(provision);
    }
    const query: asset.AssetMap = new Map();
    query.set(asset.Tag.ALIAS, alias);
    query.set(asset.Tag.RETURN_TYPE, asset.ReturnType.ALL);
    let records: asset.AssetMap[];
    try { records = await asset.query(query); }
    catch (_) { throw new Error('尚未导入当前应用的开发签名。请使用“构建 → 导入当前工程签名包”，导入一次后可自动复用。'); }
    if (records.length !== 1) throw new Error('尚未导入本设备的有效签名材料');
    const secret = records[0].get(asset.Tag.SECRET) as Uint8Array;
    return JSON.parse(new util.TextDecoder().decodeToString(secret)) as SigningCredentials;
  }

  private static time(text: string): number {
    const match = /^(\d{4}|\d{2})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})Z$/.exec(text);
    if (match) {
      const year = match[1].length === 4 ? Number(match[1]) : Number(match[1]) + (Number(match[1]) >= 50 ? 1900 : 2000);
      return Date.UTC(year, Number(match[2])-1, Number(match[3]), Number(match[4]), Number(match[5]), Number(match[6]));
    }
    return Date.parse(text);
  }
  static async validate(logical: string, bundle: string): Promise<string> {
    const directory = SigningMaterials.directory(logical);
    for (const name of ['key.p12','app.cer','profile.p7b']) if (!fileIo.accessSync(directory + '/' + name)) throw new Error('缺少开发签名文件 ' + name + '，请导入当前工程签名包');
    await SigningMaterials.load(logical);
    const handle = fileIo.openSync(directory + '/app.cer', fileIo.OpenMode.READ_ONLY);
    const bytes = new ArrayBuffer(fileIo.statSync(handle.fd).size);
    try { fileIo.readSync(handle.fd, bytes); } finally { fileIo.closeSync(handle); }
    const certificate = await cert.createX509Cert({data:new Uint8Array(bytes),encodingFormat:new Uint8Array(bytes)[0]===45 ? cert.EncodingFormat.FORMAT_PEM : cert.EncodingFormat.FORMAT_DER});
    const before = SigningMaterials.time(certificate.getNotBeforeTime()), after = SigningMaterials.time(certificate.getNotAfterTime());
    if (!Number.isFinite(before) || !Number.isFinite(after)) throw new Error('无法解析签名证书有效期：' + certificate.getNotBeforeTime() + ' / ' + certificate.getNotAfterTime());
    if (Date.now() < before || Date.now() >= after) throw new Error('开发证书尚未生效或已过期，请重新导入有效签名包');
    const profileOutput = directory + '/verified-profile.json';
    const args: string[] = ['verify-profile','-inFile',directory+'/profile.p7b','-outFile',profileOutput];
    if (await signer.runSigner(args) !== 0) throw new Error('开发 Profile 签名校验失败，请重新下载有效 Profile');
    const profile = JSON.parse(fileIo.readTextSync(profileOutput)) as Record<string, Object>;
    const bundleInfo = JSON.parse(JSON.stringify(profile['bundle-info'])) as Record<string, string>;
    const validity = JSON.parse(JSON.stringify(profile['validity'])) as Record<string, number>;
    if (bundleInfo['bundle-name'] !== bundle) throw new Error('签名 Profile 与当前 bundleName 不匹配：' + bundle);
    const validFrom = validity['not-before']*1000, validUntil = validity['not-after']*1000;
    if (!Number.isFinite(validFrom) || !Number.isFinite(validUntil)) throw new Error('开发 Profile 缺少有效期');
    if (Date.now() < validFrom || Date.now() >= validUntil) throw new Error('开发 Profile 已过期或尚未生效，请更新签名包');
    // The Profile can expire earlier than the certificate. Report the effective limit.
    return new Date(Math.min(after, validUntil)).toISOString();
  }
  static async importPackage(context: common.UIAbilityContext, uri: string, bundle: string): Promise<string> {
    if (!/^[A-Za-z0-9_.]+$/.test(bundle)) throw new Error('无效应用标识');
    const logical = context.filesDir + '/native-toolchain/signing' + (bundle==='com.example.harmonyplayground' ? '' : '/' + bundle);
    const set = String(Date.now()), directory = logical + '/.sets/' + set;
    fileIo.mkdirSync(directory, true);
    const archive = directory + '/input.zip';
    const source = fileIo.openSync(uri, fileIo.OpenMode.READ_ONLY);
    try {
      if (fileIo.statSync(source.fd).size > 16*1024*1024) throw new Error('签名包不能超过 16 MB');
      fileIo.copyFileSync(source.fd, archive);
    } finally { fileIo.closeSync(source); }
    try {
      resources.validateArchive(archive);
      await zlib.decompressFile(archive, directory);
      await SigningMaterials.validate(directory, bundle);
      await SigningMaterials.validateKey(directory);
      const active: ActiveSigning = {set:set};
      AtomicFiles.write(logical + '/active.json', JSON.stringify(active));
    } finally { if (fileIo.accessSync(archive)) fileIo.unlinkSync(archive); }
    return '当前应用的开发签名已导入并校验；后续构建自动复用，密码已存入系统加密资产库。';
  }
}
