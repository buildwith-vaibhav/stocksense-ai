# StockSense AI

Self-hosted stock monitoring and alert system for NSE-style symbols.

**Stack:** C++20 · Crow HTTP · PostgreSQL · technical indicators · logistic ML score · Telegram alerts · embedded web dashboard · Windows scheduled pipeline.

> **Not investment advice.** Automated rules and a simple model only. No guaranteed returns.

## Features (Stage 8)

| Area | What you get |
|------|----------------|
| API | REST health, stocks, analysis, ingest, indicators, alerts |
| Data | Synthetic bars or CSV EOD ingest |
| Indicators | SMA, RSI, MACD, ATR + rule signal |
| ML | `probability_up` from JSON logistic weights (`models/stocksense_lr.json`) |
| Alerts | `price_above` / `price_below` / `rsi_overbought` / `rsi_oversold` + cooldown |
| Notify | Telegram (env token + chat id) |
| Explain | Rule-based summary; optional OpenAI-compatible LLM |
| UI | `http://localhost:8080/dashboard` |
| Ops | PowerShell daily pipeline + Task Scheduler scripts (see `ops/`) |

## Quick start (Windows)

### Prerequisites

- Visual Studio 2022/2026 (C++ desktop)
- CMake 3.16+
- PostgreSQL (any port; Windows installs sometimes use **2812**)
- Git

### Database

```sql
CREATE USER stocksense WITH PASSWORD 'stocksense';
CREATE DATABASE stocksense OWNER stocksense;
```

Run `sql/001_schema.sql` and `sql/002_seed.sql` as that user.

### Build

```powershell
cd stocksense-ai
$env:PostgreSQL_ROOT = "C:\Program Files\PostgreSQL\18"
cmake -B build -G "Visual Studio 18 2026" -A x64 -DPostgreSQL_ROOT="$env:PostgreSQL_ROOT"
cmake --build build --config Release --target stocksense_api
```

### Run

```powershell
$env:STOCKSENSE_DB_URL = "postgresql://stocksense:stocksense@127.0.0.1:2812/stocksense"
$env:STOCKSENSE_MODEL_PATH = "$PWD\models\stocksense_lr.json"
$env:STOCKSENSE_TELEGRAM_BOT_TOKEN = "YOUR_BOT_TOKEN"
$env:STOCKSENSE_TELEGRAM_CHAT_ID = "YOUR_CHAT_ID"

.\build\Release\stocksense_api.exe
```

Open: http://localhost:8080/ and http://localhost:8080/dashboard

### Smoke test

```powershell
Invoke-RestMethod "http://localhost:8080/"
Invoke-RestMethod "http://localhost:8080/api/admin/ingest/synthetic?days=30"
Invoke-RestMethod "http://localhost:8080/api/admin/indicators/compute"
Invoke-RestMethod "http://localhost:8080/api/stocks/TCS/analysis"
Invoke-RestMethod "http://localhost:8080/api/alerts/create?symbol=TCS&alert_type=price_below&threshold=999999&cooldown_minutes=1"
Invoke-RestMethod "http://localhost:8080/api/admin/alerts/evaluate"
```

## Environment

| Variable | Purpose |
|----------|---------|
| `STOCKSENSE_DB_URL` | PostgreSQL connection string |
| `STOCKSENSE_MODEL_PATH` | Path to `stocksense_lr.json` |
| `STOCKSENSE_TELEGRAM_BOT_TOKEN` | BotFather token |
| `STOCKSENSE_TELEGRAM_CHAT_ID` | Numeric chat id |
| `STOCKSENSE_LLM_API_KEY` | Optional LLM explanations |

Never commit real tokens. Use `.env.example` as a template only.

## Retrain ML (optional)

```powershell
pip install scikit-learn numpy
python ml\train_model.py
```

## Ops

See `ops/README.md` for CSV EOD format and Windows Task Scheduler.

## License

MIT — see [LICENSE](LICENSE).

## Author

Portfolio / personal-alert MVP. Suitable as a learning project or paid pilot base—not a production brokerage platform.
