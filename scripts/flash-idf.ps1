param([string]$Port = 'COM4')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $repoRoot 'build/firmware-idf'
$backupDir = Join-Path $repoRoot 'build/device-backup'
$backup = Join-Path $backupDir 'pre-vendor-demo-full-flash.bin'
$manifest = Get-Content (Join-Path $backupDir 'backup-manifest.json') -Raw | ConvertFrom-Json
if ((Get-Item $backup).Length -ne $manifest.bytes -or
    (Get-FileHash $backup -Algorithm SHA256).Hash -ne $manifest.sha256) {
    throw 'Verified recovery backup is required before this comparison flash.'
}
$flash = Get-Content (Join-Path $buildDir 'flasher_args.json') -Raw | ConvertFrom-Json
if ($flash.app.offset -ne '0x10000' -or $flash.bootloader.offset -ne '0x0' -or
    $flash.'partition-table'.offset -ne '0x8000' -or $flash.flash_settings.flash_size -ne '16MB') {
    throw 'Unexpected flash layout; comparison flash cancelled.'
}
$partitionFile = Join-Path $buildDir $flash.'partition-table'.file
$partitionBytes = [IO.File]::ReadAllBytes($partitionFile)
$backupBytes = [IO.File]::ReadAllBytes($backup)
for ($index = 0; $index -lt $partitionBytes.Length; $index++) {
    if ($partitionBytes[$index] -ne $backupBytes[0x8000 + $index]) {
        throw 'Partition table differs from the backed-up device.'
    }
}
$bootloader = Join-Path $buildDir $flash.bootloader.file
$app = Join-Path $buildDir $flash.app.file
if ((Get-Item $app).Length -gt 0x300000) { throw 'Application exceeds the existing OTA slot.' }
$esptool = Join-Path $repoRoot '.arduino/data/packages/esp32/tools/esptool_py/5.3.1/esptool.exe'
# Deliberately preserve NVS (0x9000) and OTA selection (0xe000). This device's
# verified backup selects app0. The new bootloader and app are built together.
& $esptool --chip esp32s3 --port $Port --baud 921600 write-flash `
    --flash-mode $flash.flash_settings.flash_mode --flash-freq $flash.flash_settings.flash_freq `
    --flash-size 16MB 0x0 $bootloader 0x8000 $partitionFile 0x10000 $app
if ($LASTEXITCODE -ne 0) { throw 'Comparison flash failed.' }
& $esptool --chip esp32s3 --port $Port --baud 921600 verify-flash `
    0x0 $bootloader 0x8000 $partitionFile 0x10000 $app
if ($LASTEXITCODE -ne 0) { throw 'Comparison flash verification failed.' }
Write-Output 'Source-built firmware written and verified; disconnect all power for ten seconds, then reconnect UART1.'
