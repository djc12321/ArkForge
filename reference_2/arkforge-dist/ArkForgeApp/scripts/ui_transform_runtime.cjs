// Official SDK ArkUI transformer orchestration, shared by host checks and device bundle.
const path = require('path');
const fs = require('fs');
const sdk = globalThis.__arkforgeSdk || 'C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader';
let session;

exports.transform = function(source, fileName = '/project/harmony_demo/src/main/ets/pages/Index.ets', settings = {}) {
  const moduleName = settings.moduleName || 'harmony_demo';
  const bundleName = settings.bundleName || 'com.example.harmonyplayground';
  const inputFiles = settings.files;
  const fresh = !session || !inputFiles || session.inputFiles !== inputFiles;
  if (fresh) {
    session = { inputFiles, files: Object.assign({}, inputFiles || {}, {[fileName]:source}) };
    // Resolve all Kit imports once before binding the shared program.
    for (const name of Object.keys(session.files)) session.files[name] = expandKits(session.files[name]);
  }
  const projectFiles = session.files;
  source = projectFiles[fileName];
  if (fresh && typeof __sdkFiles !== 'undefined') {
    Object.assign(__sdkFiles, projectFiles);
    for (const filename of Object.keys(projectFiles)) {
      let child = filename;
      while (child !== '/') {
        const parent = path.dirname(child);
        if (!__sdkDirectories[parent]) __sdkDirectories[parent] = [];
        const name = path.basename(child);
        if (!__sdkDirectories[parent].includes(name)) __sdkDirectories[parent].push(name);
        child = parent;
      }
    }
  }
  // Resolve Kit exports with the installed SDK's own symbol metadata.
  function expandKits(text) { return text.replace(/import\s+\{([^}]+)\}\s+from\s+['"](@kit\.[^'"]+)['"]\s*;?/g, function(original, names, kit) {
    const metadata = JSON.parse(fs.readFileSync(path.join(sdk, 'kit_configs', kit + '.json'), 'utf8'));
    return names.split(',').map(function(name) {
      const parts = name.trim().split(/\s+as\s+/), symbol = metadata.symbols[parts[0]];
      if (!symbol) throw new Error('SDK Kit export not found: ' + kit + '.' + parts[0]);
      const binding = parts[1] || parts[0], moduleName = symbol.source.replace(/\.d\.(ets|ts)$/, '');
      return symbol.bindings === 'default' ? 'import ' + binding + ' from "' + moduleName + '";' :
        'import { ' + symbol.bindings + (symbol.bindings === binding ? '' : ' as ' + binding) + ' } from "' + moduleName + '";';
    }).join(' ') + '\n'.repeat((original.match(/\n/g) || []).length);
  }); }
  const mainPath = path.join(sdk, 'main.js');
  let main = {
    ohosSystemModuleSubDirPaths: [], globalProgram: {}, resources: settings.resources || {},
    projectConfig: { projectPath: '/project/' + moduleName + '/src/main/ets', projectRootPath: '/project',
      compileMode: 'esmodule', moduleName, bundleName,
      minAPIVersion: 12, compileSdkVersion: 23, compatibleSdkVersion: 12, sdkInfo: '23',
      buildMode: 'debug', processTs: false, cachePath: '/cache',
      moduleJsonPath: '/project/' + moduleName + '/src/main/module.json5' },
    partialUpdateConfig: { partialUpdateMode: true }
  };
  if (require.cache[mainPath]) {
    Object.assign(require.cache[mainPath].exports, main);
    main = require.cache[mainPath].exports;
  } else require.cache[mainPath] = { id: mainPath, filename: mainPath, loaded: true, exports: main };
  process.env.compiler = 'on'; process.env.compileTool = 'rollup'; process.env.compileMode = 'moduleJson';
  const ts = require(path.join(sdk, 'node_modules/typescript'));
  const options = { target: ts.ScriptTarget.ES2021, module: ts.ModuleKind.ES2020,
    experimentalDecorators: true, allowNonTsExtensions: true, noLib: true, sourceMap: true, inlineSources: true };
  options.ets = ts.parseConfigFileTextToJson('tsconfig.json', fs.readFileSync(path.join(sdk, 'tsconfig.json'), 'utf8')).config.compilerOptions.ets;
  options.ets.components = Object.keys(require(path.join(sdk, 'lib/component_map.js')).COMPONENT_MAP);
  const host = ts.createCompilerHost(options);
  const read = name => name === fileName ? source : projectFiles[name];
  host.getSourceFile = (name, version) => read(name) !== undefined ?
    ts.createSourceFile(name, read(name), version, true, ts.ScriptKind.ETS, options) : undefined;
  host.fileExists = name => read(name) !== undefined;
  host.readFile = read;
  host.resolveModuleNames = (names, containingFile) => names.map(name => {
    if (!name.startsWith('.')) return undefined;
    const base = path.resolve(path.dirname(containingFile), name);
    const resolved = [base + '.ets', base + '.ts', base + '/index.ets', base + '/index.ts'].find(f => read(f) !== undefined);
    return resolved ? { resolvedFileName: resolved, extension: ts.Extension.Ts } : undefined;
  });
  if (!session.program) session.program = ts.createProgram(Object.keys(projectFiles), options, host);
  main.globalProgram.program = session.program;
  main.globalProgram.checker = main.globalProgram.program.getTypeChecker();
  const syntax = require(path.join(sdk, 'lib/process_ui_syntax.js'));
  syntax.resetProcessUiSyntax();
  const stored = require(path.join(sdk, 'lib/utils.js')).storedFileInfo;
  stored.setCurrentArkTsFile(); stored.wholeFileInfo[fileName] = {};
  const validation = require(path.join(sdk, 'lib/validate_ui_syntax.js'));
  validation.resetComponentCollection();
  const sourceFile = main.globalProgram.program.getSourceFile(fileName);
  const logs = validation.validateUISyntax(source, source, fileName, '', sourceFile);
  // Emit the program's bound AST so the SDK can resolve imported components
  // with the same checker. transpileModule creates a different, unbound AST.
  let outputText = '', sourceMap = '';
  const emitted = main.globalProgram.program.emit(sourceFile, (name, text) => {
    if (/\.js$/.test(name)) outputText = text;
    if (/\.js\.map$/.test(name)) sourceMap = text;
  }, undefined, false, { before: [syntax.processUISyntax(null)] });
  const result = { outputText, diagnostics: main.globalProgram.program.getSyntacticDiagnostics(sourceFile).concat(emitted.diagnostics) };
  const position = function(start) {
    const prefix = source.slice(0, start || 0), lines = prefix.split('\n');
    return { line: lines.length, column: lines[lines.length - 1].length + 1 };
  };
  const errors = result.diagnostics.map(d => Object.assign({ message: ts.flattenDiagnosticMessageText(d.messageText, '\n'), start: d.start }, position(d.start)));
  for (const item of logs.concat(syntax.transformLog.errors)) {
    if (item.type === 'ERROR') errors.push(Object.assign({message: item.message, start: item.pos}, position(item.pos)));
  }
  let output = validation.processSystemApi(result.outputText, true, fileName);
  if (settings.normalized) {
    // Match the SDK normalized OHM URL and filesInfo record-name contract.
    // Parse imports as JS; never rewrite strings inside user code or Python templates.
    const ast = ts.createSourceFile(fileName + '.js', output, ts.ScriptTarget.ES2021, true, ts.ScriptKind.JS);
    const replacements = [];
    const usedNames = new Set();
    function collectNames(node) {
      if (ts.isImportDeclaration(node)) return;
      if (ts.isIdentifier(node)) usedNames.add(node.text);
      ts.forEachChild(node, collectNames);
    }
    collectNames(ast);
    ast.forEachChild(node => {
      const specifier = node.moduleSpecifier;
      if ((!ts.isImportDeclaration(node) && !ts.isExportDeclaration(node)) || !specifier || !ts.isStringLiteral(specifier)) return;
      let name = specifier.text, target;
      if (name.startsWith('.')) {
        target = path.resolve(path.dirname(fileName), name).replace(/\.(ets|ts|js)$/, '');
        if (!projectFiles[target + '.ets'] && !projectFiles[target + '.ts'] && projectFiles[target + '/index.ets']) target += '/index';
        name = '@normalized:N&&&' + target.replace(/^\/project\//, '') + '&';
      } else if (/^lib[^/]+\.so$/.test(name)) name = '@normalized:Y&&&' + name + '&';
      else if (name.startsWith('@hms.')) name = '@hms:' + name.slice(5);
      let declaration = node;
      if (ts.isImportDeclaration(node) && node.importClause && node.importClause.namedBindings && ts.isNamedImports(node.importClause.namedBindings)) {
        const elements = node.importClause.namedBindings.elements.filter(element => usedNames.has(element.name.text));
        if (elements.length === 0 && !node.importClause.name) {
          replacements.push({start:node.getStart(ast),end:node.getEnd(),text:''});
          return;
        }
        declaration = ts.factory.updateImportDeclaration(node,node.modifiers,
          ts.factory.updateImportClause(node.importClause,false,node.importClause.name,
            ts.factory.updateNamedImports(node.importClause.namedBindings,elements)),
          ts.factory.createStringLiteral(name),node.assertClause);
        replacements.push({start:node.getStart(ast),end:node.getEnd(),text:ts.createPrinter().printNode(ts.EmitHint.Unspecified,declaration,ast)});
      } else if (name !== specifier.text) replacements.push({ start: specifier.getStart(ast), end: specifier.getEnd(), text: JSON.stringify(name) });
    });
    for (const replacement of replacements.reverse()) output = output.slice(0, replacement.start) + replacement.text + output.slice(replacement.end);
  }
  // Preserve the SDK map through its post-emit import rewriting. Body lines
  // are matched exactly; inserted/rewritten imports are deliberately unmapped.
  const before = outputText.split('\n'), after = output.split('\n');
  const generatedLineMap = new Array(after.length).fill(-1);
  // Myers edit path: map only lines unchanged by SDK postprocessing. Bound the
  // edit distance; an unmapped location is preferable to a guessed source line.
  const trace=[], v=new Map([[1,0]]);
  let complete=false;
  outer:for(let d=0;d<=Math.min(before.length+after.length,1024);d++) {
    trace.push(new Map(v));
    for(let k=-d;k<=d;k+=2) {
      const left=v.has(k-1)?v.get(k-1):-Infinity,right=v.has(k+1)?v.get(k+1):-Infinity;
      let x=k===-d || (k!==d && left<right)?right:left+1;
      if(!Number.isFinite(x)) x=0;
      let y=x-k;
      while(x<before.length && y<after.length && before[x]===after[y]) {x++;y++;}
      v.set(k,x);
      if(x>=before.length && y>=after.length) {complete=true;break outer;}
    }
  }
  if(complete) {
    let x=before.length,y=after.length;
    for(let d=trace.length-1;d>=0;d--) {
      const previous=trace[d],k=x-y,left=previous.has(k-1)?previous.get(k-1):-Infinity,right=previous.has(k+1)?previous.get(k+1):-Infinity;
      const previousK=k===-d || (k!==d && left<right)?k+1:k-1;
      const previousX=previous.get(previousK)||0,previousY=previousX-previousK;
      while(x>previousX && y>previousY) {generatedLineMap[--y]=--x;}
      x=previousX;y=previousY;
    }
  }
  return { ok: errors.length === 0, output, errors, sourceMap, generatedLineMap };
};
