$ErrorActionPreference = "Continue"
$ProjRoot = "D:\IronManGame\IronManArena"
$UProject = Join-Path $ProjRoot "IronManArena.uproject"
$EngineCmd = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Script = Join-Path $ProjRoot "Content\Python\import_suit_models.py"
$LogDir = Join-Path $ProjRoot "Saved\Suits"
$Log = Join-Path $LogDir "cmdlet.log"
$Err = Join-Path $LogDir "cmdlet.err.log"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

if (-not (Test-Path $EngineCmd)) { throw "Missing UnrealEditor-Cmd: $EngineCmd" }
if (-not (Test-Path $UProject)) { throw "Missing uproject: $UProject" }
if (-not (Test-Path $Script)) { throw "Missing import script: $Script" }
$Src = Join-Path $ProjRoot "SourceAssets\Suits"
if (-not (Test-Path $Src)) { throw "Missing SourceAssets\Suits: $Src" }

$editors = Get-Process -Name "UnrealEditor","UnrealEditor-Cmd" -ErrorAction SilentlyContinue
if ($editors) {
  Write-Host ("EDITOR_RUNNING PIDs={0}; refusing to kill - wait or close editor, then re-run")
  Write-Host "ABORT_EDITOR_LOCK"
  exit 2
}

Write-Host "Running suit models import commandlet..."
$argList = @(
  $UProject,
  "-run=pythonscript",
  ("-script={0}" -f $Script),
  "-unattended",
  "-nullrhi",
  "-nosplash",
  "-nosound",
  "-NoLogTimes",
  "-stdout"
)
$p = Start-Process -FilePath $EngineCmd -ArgumentList $argList -Wait -PassThru -NoNewWindow `
  -RedirectStandardOutput $Log -RedirectStandardError $Err
Write-Host ("ExitCode={0}" -f $p.ExitCode)
if (Test-Path (Join-Path $ProjRoot "Content\Models\suit_models_manifest.json")) {
  Write-Host "MANIFEST_OK"
  Get-Content (Join-Path $ProjRoot "Content\Models\suit_models_manifest.json") -TotalCount 60
} else {
  Write-Host "MANIFEST_MISSING"
}
if (Test-Path (Join-Path $LogDir "import_report.txt")) {
  Write-Host "==== import_report ===="
  Get-Content (Join-Path $LogDir "import_report.txt")
}
exit $p.ExitCode
