param(
    [string]$IdfPath = "",
    [string]$IdfToolsPath = ""
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$IdfPath) { $IdfPath = Join-Path $repoRoot '.tools/esp-idf' }
if (!$IdfToolsPath) { $IdfToolsPath = Join-Path $repoRoot '.tools/idf-tools' }
$IdfPath = (Resolve-Path -LiteralPath $IdfPath).Path
$IdfToolsPath = (Resolve-Path -LiteralPath $IdfToolsPath).Path
$idfVersion = & git -C $IdfPath describe --tags --exact-match
if ($LASTEXITCODE -ne 0 -or $idfVersion -ne 'v5.5.5') {
    throw 'This comparison build requires ESP-IDF v5.5.5.'
}
$pythonCandidates = @(Get-ChildItem (Join-Path $IdfToolsPath 'python_env') -Directory |
    Where-Object Name -Like 'idf5.5_py*_env')
if ($pythonCandidates.Count -ne 1) {
    throw 'Install one ESP-IDF 5.5 Python environment with idf_tools.py install-python-env.'
}
$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $IdfToolsPath
$env:IDF_PYTHON_ENV_PATH = $pythonCandidates[0].FullName
$idfPython = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
# Resolve build tools from IDF's pinned manifest without probing optional GDB
# executables, whose --version can hang on this Windows host.
$toolManifest = Get-Content (Join-Path $IdfPath 'tools/tools.json') -Raw | ConvertFrom-Json
$buildPaths = @((Split-Path $idfPython), (Join-Path $IdfPath 'tools'))
foreach ($toolName in @('xtensa-esp-elf', 'esp32ulp-elf', 'cmake', 'ninja')) {
    $tool = $toolManifest.tools | Where-Object name -EQ $toolName
    $version = $tool.versions | Where-Object status -EQ 'recommended'
    $toolRoot = Join-Path $IdfToolsPath "tools/$toolName/$($version.name)"
    foreach ($parts in $tool.export_paths) {
        $exportPath = Join-Path $toolRoot ($parts -join '/')
        if (!(Test-Path -LiteralPath $exportPath)) { throw "Missing build tool: $exportPath" }
        $buildPaths += $exportPath
    }
}
$env:PATH = ($buildPaths -join ';') + ';' + $env:PATH
$env:ESP_IDF_VERSION = '5.5'
$env:ESP_ROM_ELF_DIR = Join-Path $IdfToolsPath 'tools/esp-rom-elfs/20241011'

# Use the already pinned Arduino installation, without changing its files.
# Its broad component manifest pulls unrelated Matter/Zigbee dependencies;
# the project enables the libraries required by this application directly.
$arduinoSource = Join-Path $repoRoot '.arduino/data/packages/esp32/hardware/esp32/3.3.11'
$arduinoComponent = Join-Path $repoRoot 'build/idf-components/arduino'
if (!(Test-Path (Join-Path $arduinoSource 'CMakeLists.txt'))) {
    throw 'Run the normal project bootstrap first to install Arduino ESP32 3.3.11 and pinned libraries.'
}
New-Item -ItemType Directory -Force -Path $arduinoComponent | Out-Null
foreach ($entry in @('cores', 'libraries', 'variants', 'CMakeLists.txt', 'Kconfig.projbuild')) {
    Copy-Item -LiteralPath (Join-Path $arduinoSource $entry) -Destination $arduinoComponent -Recurse -Force
}
# ESP-IDF caches the generated sdkconfig and it takes precedence over
# sdkconfig.defaults, so editing the defaults alone silently keeps the previous
# memory profile. Drop the cache whenever the defaults are newer than it; the
# static_asserts in main/sketch.cpp are the backstop if this ever misses.
$sdkDefaults = Join-Path $repoRoot 'firmware/idf/sdkconfig.defaults'
$sdkCache = Join-Path $repoRoot 'build/idf-sdkconfig'
if ((Test-Path -LiteralPath $sdkCache) -and
    (Get-Item -LiteralPath $sdkDefaults).LastWriteTimeUtc -gt
    (Get-Item -LiteralPath $sdkCache).LastWriteTimeUtc) {
    Write-Host 'sdkconfig.defaults changed; regenerating the cached sdkconfig.'
    Remove-Item -LiteralPath $sdkCache -Force
}

& $idfPython (Join-Path $IdfPath 'tools/idf.py') -C (Join-Path $repoRoot 'firmware/idf') `
    -B (Join-Path $repoRoot 'build/firmware-idf') build
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF build failed; no firmware was flashed.' }
