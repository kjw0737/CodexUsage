param(
    [switch]$SkipBuild,
    [string]$RedistPath,
    [string]$InnoSetupPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $projectRoot 'CodexUsage.sln'
$appPath = Join-Path $projectRoot 'installer\build\CodexUsage.exe'
$scriptPath = Join-Path $projectRoot 'installer\CodexUsage.iss'

if (-not $InnoSetupPath) {
    $InnoSetupPath = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
}
if (-not (Test-Path -LiteralPath $InnoSetupPath -PathType Leaf)) {
    throw "Inno Setup 6 compiler not found: $InnoSetupPath"
}

$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
    throw "Visual Studio vswhere not found: $vswherePath"
}
$vsPath = (& $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $vsPath) { throw 'Visual Studio 2022 C++ tools are required.' }

if (-not $SkipBuild) {
    $msbuildPath = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path -LiteralPath $msbuildPath -PathType Leaf)) { throw "MSBuild not found: $msbuildPath" }
    $outputDir = Join-Path $projectRoot 'installer\build'
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    & $msbuildPath $solutionPath /m /t:CodexUsage /p:Configuration=Release /p:Platform=x64 "/p:OutDir=$outputDir\" /nologo
    if ($LASTEXITCODE -ne 0) { throw "Release x64 build failed: $LASTEXITCODE" }
}
if (-not (Test-Path -LiteralPath $appPath -PathType Leaf)) { throw "Release executable not found: $appPath" }

if (-not $RedistPath) {
    $RedistPath = Join-Path $vsPath 'VC\Redist\MSVC\v143\vc_redist.x64.exe'
}
if (-not (Test-Path -LiteralPath $RedistPath -PathType Leaf)) { throw "Official Visual C++ x64 redistributable not found: $RedistPath" }

$appVersion = (Get-Item -LiteralPath $appPath).VersionInfo.ProductVersion
if (-not $appVersion) { throw "Product version missing from $appPath" }

& $InnoSetupPath "/DAppVersion=$appVersion" "/DRedistPath=$RedistPath" "/DAppExePath=$appPath" $scriptPath
if ($LASTEXITCODE -ne 0) { throw "Installer build failed: $LASTEXITCODE" }

$setupPath = Join-Path $projectRoot "dist\CodexUsage-Setup-$appVersion-x64.exe"
if (-not (Test-Path -LiteralPath $setupPath -PathType Leaf)) { throw "Installer output not found: $setupPath" }
Get-Item -LiteralPath $setupPath | Select-Object FullName, Length, LastWriteTime
