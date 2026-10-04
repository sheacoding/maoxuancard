<#
[INPUT]: Bun、FFmpeg、GCC/G++ 与 PATH 或 PLATFORMIO 指定的 PlatformIO
[OUTPUT]: 检查退出码的测试、固件构建、指定串口烧录和监视入口
[POS]: 项目根目录的构建编排；首次运行安装固定依赖，不依赖其他项目目录
[PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
#>
param(
    [ValidateSet('test','build','upload','monitor')][string]$Action = 'build',
    [string]$Port
)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
Set-Location $PSScriptRoot

function Get-PlatformIO {
    if ($env:PLATFORMIO) { return $env:PLATFORMIO }
    return (Get-Command platformio -ErrorAction Stop).Source
}

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program 执行失败，退出码：$LASTEXITCODE" }
}

if ($Action -in 'test','build','upload') {
    if (-not (Test-Path '.pio/libdeps/esp32c3/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c') -or
        -not (Test-Path '.pio/libdeps/esp32c3/ArduinoJson/src/ArduinoJson.h')) {
        Invoke-Checked (Get-PlatformIO) @('pkg','install','-e','esp32c3')
    }
    Invoke-Checked bun @('tools/prepare.ts')
    Invoke-Checked bun @('tools/native.ts')
    Invoke-Checked bun @('tests/check.ts')
}
if ($Action -in 'build','upload') { Invoke-Checked (Get-PlatformIO) @('run','-e','esp32c3') }
if ($Action -eq 'upload') {
    if (-not $Port) { throw 'upload 必须显式指定 -Port，例如 COM5。' }
    Invoke-Checked (Get-PlatformIO) @('run','-e','esp32c3','-t','upload','--upload-port',$Port)
}
if ($Action -eq 'monitor') {
    if (-not $Port) { throw 'monitor 必须显式指定 -Port，例如 COM5。' }
    Invoke-Checked (Get-PlatformIO) @('device','monitor','--port',$Port,'--baud','115200')
}
