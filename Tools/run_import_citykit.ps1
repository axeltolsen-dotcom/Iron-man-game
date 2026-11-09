$ErrorActionPreference = "Continue"
$ProjRoot = "D:\IronManGame\IronManArena"
$UProject = Join-Path $ProjRoot "IronManArena.uproject"
$EngineCmd = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Script = Join-Path $ProjRoot "Content\Python\import_citykit.py"
$Fetch = Join-Path $ProjRoot "Tools\fetch_citykit_assets.ps1"
$Log = Join-Path $ProjRoot "Saved\CityKit\cmdlet.log"

New-Item -ItemType Directory -Force -Path (Join-Path $ProjRoot "Saved\CityKit") | Out-Null

# Ensure source assets
$Src = Join-Path $ProjRoot "SourceAssets\CityKit\Textures"
if (-not (Test-Path $Src)) {
  Write-Host "Fetching CityKit assets..."
  & powershell -NoProfile -ExecutionPolicy Bypass -File $Fetch
}

if (-not (Test-Path $EngineCmd)) { throw "Missing UnrealEditor-Cmd: $EngineCmd" }
if (-not (Test-Path $UProject)) { throw "Missing uproject: $UProject" }
if (-not (Test-Path $Script)) { throw "Missing import script: $Script" }

# Check for editor lock (report, don't kill)
$editors = Get-Process -Name "UnrealEditor","UnrealEditor-Cmd" -ErrorAction SilentlyContinue
if ($editors) {
  Write-Host ("NOTE: UnrealEditor running PIDs={0}; attempting commandlet anyway" -f ($editors.Id -join ","))
}

Write-Host "Running CityKit import commandlet..."
$args = @(
  $UProject,
  "-run=pythonscript",
  ("-script={0}" -f $Script),
  "-unattended",
  "-nullrhi",
  "-NoLogTimes"
)
$p = Start-Process -FilePath $EngineCmd -ArgumentList $args -Wait -PassThru -NoNewWindow `
  -RedirectStandardOutput $Log -RedirectStandardError (Join-Path $ProjRoot "Saved\CityKit\cmdlet.err.log")
Write-Host ("ExitCode={0}" -f $p.ExitCode)
if (Test-Path (Join-Path $ProjRoot "Content\CityKit\citykit_manifest.json")) {
  Write-Host "MANIFEST_OK"
  Get-Content (Join-Path $ProjRoot "Content\CityKit\citykit_manifest.json") -TotalCount 80
} else {
  Write-Host "MANIFEST_MISSING"
}
if (Test-Path (Join-Path $ProjRoot "Saved\CityKit\import_report.txt")) {
  Write-Host "==== import_report ===="
  Get-Content (Join-Path $ProjRoot "Saved\CityKit\import_report.txt")
}
