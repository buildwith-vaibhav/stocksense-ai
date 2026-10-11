# StockSense AI - daily pipeline
# Runs: optional CSV ingest, synthetic fallback, indicators, alert evaluate
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File D:\stocksense-ai-ops\scripts\daily_pipeline.ps1

$ErrorActionPreference = "Stop"
$BaseUrl = if ($env:STOCKSENSE_BASE_URL) { $env:STOCKSENSE_BASE_URL } else { "http://127.0.0.1:8080" }
$CsvPath = if ($env:STOCKSENSE_EOD_CSV) { $env:STOCKSENSE_EOD_CSV } else {
  Join-Path $PSScriptRoot "..\data\eod_today.csv"
}
$CsvPath = [System.IO.Path]::GetFullPath($CsvPath)
$LogDir = Join-Path $PSScriptRoot "..\logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$LogFile = Join-Path $LogDir ("pipeline_{0:yyyyMMdd_HHmmss}.log" -f (Get-Date))

function Write-Log($msg) {
  $line = "[{0:yyyy-MM-dd HH:mm:ss}] {1}" -f (Get-Date), $msg
  Write-Host $line
  Add-Content -Path $LogFile -Value $line
}

function Invoke-Ss($method, $path) {
  $url = "$BaseUrl$path"
  Write-Log "$method $url"
  try {
    if ($method -eq "GET") {
      return Invoke-RestMethod -Uri $url -Method GET -TimeoutSec 120
    }
    return Invoke-RestMethod -Uri $url -Method POST -TimeoutSec 120
  } catch {
    Write-Log ("ERROR: " + $_.Exception.Message)
    throw
  }
}

Write-Log "=== StockSense daily pipeline start ==="
Write-Log ("BaseUrl=" + $BaseUrl)

try {
  $root = Invoke-Ss "GET" "/"
  Write-Log ("API version=" + $root.version + " status=" + $root.status)
} catch {
  Write-Log "API not reachable. Start stocksense_api.exe first."
  exit 1
}

if (Test-Path $CsvPath) {
  Write-Log ("CSV found: " + $CsvPath)
  $enc = [System.Uri]::EscapeDataString($CsvPath)
  $ing = Invoke-Ss "GET" ("/api/admin/ingest/csv?path=" + $enc)
  Write-Log ("CSV ingest: " + ($ing | ConvertTo-Json -Compress))
} else {
  Write-Log ("No CSV at " + $CsvPath + " - synthetic 5-day top-up")
  $ing = Invoke-Ss "GET" "/api/admin/ingest/synthetic?days=5"
  Write-Log ("Synthetic: " + ($ing | ConvertTo-Json -Compress))
}

$ind = Invoke-Ss "GET" "/api/admin/indicators/compute"
Write-Log ("Indicators: " + ($ind | ConvertTo-Json -Compress))

$ev = Invoke-Ss "GET" "/api/admin/alerts/evaluate"
Write-Log ("Evaluate: checked=" + $ev.checked + " triggered=" + $ev.triggered + " notified=" + $ev.notified)
if ($ev.details) {
  foreach ($d in $ev.details) {
    Write-Log ("  detail: " + $d)
  }
}

Write-Log "=== pipeline done ==="
Write-Log ("Log file: " + $LogFile)
exit 0
