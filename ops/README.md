# Ops — daily EOD pipeline (Windows)

## CSV format

```text
symbol,date,open,high,low,close,volume
TCS,2026-09-24,3750.00,3780.50,3720.00,3765.25,2100000
```

Save today's file as `ops/data/eod_today.csv` (or set `STOCKSENSE_EOD_CSV`).

## Run pipeline (API must be up)

```powershell
powershell -ExecutionPolicy Bypass -File ops\scripts\daily_pipeline.ps1
```

## Schedule Mon–Fri 18:30 (Administrator PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File ops\scripts\register_scheduled_task.ps1
```

## Start API helper

Edit `ops/scripts/start_api.ps1` paths and Telegram env, then run it.
