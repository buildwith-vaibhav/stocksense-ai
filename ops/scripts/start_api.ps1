# Start StockSense API - set your local paths and secrets

$Root = "D:\stocksense-ai"
$Exe = Join-Path $Root "build\Release\stocksense_api.exe"

if (-not (Test-Path $Exe)) {
  Write-Error "API not found: $Exe - build the project first."
  exit 1
}

$env:STOCKSENSE_DB_URL = "postgresql://stocksense:stocksense@127.0.0.1:2812/stocksense"
$env:STOCKSENSE_MODEL_PATH = Join-Path $Root "models\stocksense_lr.json"

# Set real values locally; never commit secrets
$env:STOCKSENSE_TELEGRAM_BOT_TOKEN = "YOUR_BOT_TOKEN"
$env:STOCKSENSE_TELEGRAM_CHAT_ID = "YOUR_CHAT_ID"

Set-Location $Root
Write-Host "Starting $Exe"
Write-Host "Dashboard http://localhost:8080/dashboard"
& $Exe
