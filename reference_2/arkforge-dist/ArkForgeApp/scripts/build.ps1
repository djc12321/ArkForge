param(
  [string]$DevEcoHome = 'C:/Program Files/Huawei/DevEco Studio',
  [string]$SigningProfileFrom = ''
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$profilePath = Join-Path $project 'build-profile.json5'
$original = [IO.File]::ReadAllText($profilePath)
$utf8 = New-Object System.Text.UTF8Encoding($false)
$env:DEVECO_SDK_HOME = "$DevEcoHome/sdk"
Write-Host "Building DevEco project: $project"
Push-Location $project
try {
  & "$DevEcoHome/tools/node/node.exe" "$PSScriptRoot/resource_tool_fingerprint.cjs"
  if ($LASTEXITCODE -ne 0) { throw 'Resource compiler fingerprint failed' }
  & "$DevEcoHome/tools/node/node.exe" "$PSScriptRoot/git_tool_fingerprint.cjs"
  if ($LASTEXITCODE -ne 0) { throw 'Git compiler fingerprint failed' }
  & "$DevEcoHome/tools/node/node.exe" "$PSScriptRoot/verify_runtime_assets.cjs"
  if ($LASTEXITCODE -ne 0) { throw 'Runtime asset verification failed' }
  if ($SigningProfileFrom) {
    $sourceProfile = Get-Content -Raw -LiteralPath $SigningProfileFrom | ConvertFrom-Json
    if (-not $sourceProfile.app.signingConfigs) { throw 'Source project has no signing config' }
    $current = $original | ConvertFrom-Json
    $current.app.signingConfigs = $sourceProfile.app.signingConfigs
    $current.app.products[0] | Add-Member -NotePropertyName signingConfig -NotePropertyValue $sourceProfile.app.signingConfigs[0].name -Force
    [IO.File]::WriteAllText($profilePath, ($current | ConvertTo-Json -Depth 30), $utf8)
  }
  & "$DevEcoHome/tools/ohpm/bin/ohpm.bat" install
  if ($LASTEXITCODE -ne 0) { throw 'OHPM failed' }
  # Windows PowerShell wraps native stderr warnings as ErrorRecords. Judge the exit code.
  $ErrorActionPreference = 'Continue'
  & "$DevEcoHome/tools/node/node.exe" "$DevEcoHome/tools/hvigor/bin/hvigorw.js" `
    --mode module -p product=default assembleHap --no-daemon
  $buildExit = $LASTEXITCODE
  $ErrorActionPreference = 'Stop'
  if ($buildExit -ne 0) { throw 'Build failed' }
} finally {
  if ($SigningProfileFrom) { [IO.File]::WriteAllText($profilePath, $original, $utf8) }
  Pop-Location
}
