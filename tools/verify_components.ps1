param(
    [switch]$RestoreManaged
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lockPath = Join-Path $root 'dependencies/components.lock.json'

if (-not (Test-Path -LiteralPath $lockPath)) {
    throw "Missing dependency lock: $lockPath"
}

$lock = Get-Content -Raw -LiteralPath $lockPath | ConvertFrom-Json
$checks = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]

function Require-Text([string]$Path, [string]$Pattern, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "[$Label] missing: $Path"
    }
    $text = Get-Content -Raw -LiteralPath $Path
    if ($text -notmatch $Pattern) {
        throw "[$Label] marker not found in $Path"
    }
    $checks.Add($Label)
}

$managed = Join-Path $root 'managed_components/espressif__esp_lcd_ili9341'
if (-not (Test-Path -LiteralPath $managed)) {
    if ($RestoreManaged) {
        Push-Location $root
        try {
            & pio run -e playground
            if ($LASTEXITCODE -ne 0) {
                throw "PlatformIO could not restore managed components"
            }
        } finally {
            Pop-Location
        }
    } else {
        $warnings.Add('managed_components/esp_lcd_ili9341 is absent; run with -RestoreManaged or pio run -e playground')
    }
}

if (Test-Path -LiteralPath $managed) {
    Require-Text (Join-Path $managed 'idf_component.yml') 'version:\s*2\.0\.2' 'esp_lcd_ili9341 2.0.2'
}

Require-Text (Join-Path $root 'components/arduino/cores/esp32/esp_arduino_version.h') 'ESP_ARDUINO_VERSION_PATCH\s+9' 'Arduino-ESP32 3.3.9'
Require-Text (Join-Path $root 'components/bluepad32/include/uni_version.h') 'UNI_VERSION_MAJOR\s+4[\s\S]*UNI_VERSION_MINOR\s+2[\s\S]*UNI_VERSION_PATCH\s+0' 'Bluepad32 4.2.0'
Require-Text (Join-Path $root 'components/lvgl/lv_version.h') 'LVGL_VERSION_MAJOR\s+9[\s\S]*LVGL_VERSION_MINOR\s+5[\s\S]*LVGL_VERSION_PATCH\s+0' 'LVGL 9.5.0'

Write-Output ("Verified components: " + ($checks -join ', '))
foreach ($warning in $warnings) {
    Write-Warning $warning
}
