# Registers a Windows Scheduled Task for the daily StockSense pipeline.
# Run PowerShell AS ADMINISTRATOR once.

$ErrorActionPreference = "Stop"

$ScriptPath = Join-Path $PSScriptRoot "daily_pipeline.ps1"
$ScriptPath = [System.IO.Path]::GetFullPath($ScriptPath)
if (-not (Test-Path $ScriptPath)) {
  throw ("Missing " + $ScriptPath)
}

$TaskName = "StockSenseDailyPipeline"
$Arg = '-NoProfile -ExecutionPolicy Bypass -File "' + $ScriptPath + '"'
$Action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument $Arg

# Mon-Fri 18:30
$Trigger = New-ScheduledTaskTrigger -Weekly -DaysOfWeek Monday,Tuesday,Wednesday,Thursday,Friday -At 6:30PM

$Settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Hours 1)

Register-ScheduledTask -TaskName $TaskName -Action $Action -Trigger $Trigger -Settings $Settings -Description "StockSense daily ingest indicators alerts" -Force | Out-Null

Write-Host ("Registered task: " + $TaskName)
Write-Host ("Script: " + $ScriptPath)
Write-Host "Schedule: Mon-Fri 18:30"
Write-Host ""
Write-Host "Test now:"
Write-Host ("  Start-ScheduledTask -TaskName " + $TaskName)
Write-Host ("  Get-ScheduledTaskInfo -TaskName " + $TaskName)
