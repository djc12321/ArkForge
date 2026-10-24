const defaults = { contextWindow: 64000, maxOutputTokens: 8192, compressionThreshold: 0.75 };
function settings(vscode) {
  const config = vscode.workspace.getConfiguration?.('arkforge.agent');
  const contextWindow = Math.max(16384, Math.min(1048576, Number(config?.get('contextWindow', defaults.contextWindow)) || defaults.contextWindow));
  return { contextWindow,
    maxOutputTokens: Math.max(1024, Math.min(32768, Math.floor(contextWindow / 4), Number(config?.get('maxOutputTokens', defaults.maxOutputTokens)) || defaults.maxOutputTokens)),
    compressionThreshold: Math.max(0.5, Math.min(0.9, Number(config?.get('compressionThreshold', defaults.compressionThreshold)) || defaults.compressionThreshold)) };
}
function historyLimit(context = defaults) {
  // Leave space for tool definitions, system instructions and the next answer.
  // A conservative character estimate also covers Chinese prose and dense code.
  return Math.max(6000, Math.min(140000, Math.floor((context.contextWindow - context.maxOutputTokens - 4000) * context.compressionThreshold)));
}
module.exports = { settings, historyLimit, defaults };
