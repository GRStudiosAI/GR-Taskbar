# Temporary opt-in diagnostic recorder for the native GR Taskbar Start button.
[CmdletBinding()]
param([ValidateSet('Start','Stop','Status')][string]$Action = 'Status')
$ErrorActionPreference = 'Stop'
$settings = 'HKCU:\Software\GR-Taskbar'
if ($Action -ne 'Status') {
    if (-not (Test-Path -LiteralPath $settings)) { New-Item -Path $settings | Out-Null }
    New-ItemProperty -Path $settings -Name StartIconDiagnostics -PropertyType DWord -Value ([int]($Action -eq 'Start')) -Force | Out-Null
    # The existing settings watcher reloads this value. No shell restart required.
}
$properties = Get-ItemProperty -Path $settings -ErrorAction SilentlyContinue
$enabled = $properties.StartIconDiagnostics
Write-Output ('Start icon recording: ' + $(if ($enabled) { 'enabled (bounded memory buffer)' } else { 'disabled' }))
if ($Action -eq 'Start') {
    Write-Output 'Move over Start, move away, press and release Start, then run this script with -Action Stop.'
} else {
    Get-ChildItem -LiteralPath (Join-Path $env:LOCALAPPDATA 'GR-Taskbar\Logs') -Filter 'StartIcon-*.log' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 3 FullName,LastWriteTime,Length
}
