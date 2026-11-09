# Fetch CC0 CityKit into SourceAssets/CityKit (ambientCG + Poly Haven).
# Run from project root or anywhere; resolves relative to this script's ../../ 
$ErrorActionPreference = "Stop"
$UA = "IronManArenaCityKit/1.0 (Axel Olsen)"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Dest = Join-Path $Root "SourceAssets\CityKit"
$Tex = Join-Path $Dest "Textures"
$Hdri = Join-Path $Dest "HDRI"
$Props = Join-Path $Dest "Props"
New-Item -ItemType Directory -Force -Path $Tex,$Hdri,$Props | Out-Null

function Get-File($Url, $Out) {
  if ((Test-Path $Out) -and ((Get-Item $Out).Length -gt 0)) {
    Write-Host "SKIP $Out"
    return
  }
  New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
  Write-Host "GET $Url"
  Invoke-WebRequest -Uri $Url -OutFile $Out -UserAgent $UA -UseBasicParsing
  Write-Host (" OK {0} {1}" -f $Out, (Get-Item $Out).Length)
}

# ambientCG 2K-JPG packs -> Textures/<Role>/
$packs = @(
  @{ Id="Asphalt025C"; Role="Asphalt_Dry" },
  @{ Id="Asphalt031"; Role="Asphalt_Wet" },
  @{ Id="Concrete034"; Role="Concrete_Sidewalk" },
  @{ Id="Concrete033"; Role="Concrete_Curb" },
  @{ Id="Bricks092"; Role="Brick" },
  @{ Id="MetalPlates006"; Role="Metal_Panels" },
  @{ Id="Gravel023"; Role="Roof_Gravel" },
  @{ Id="Facade001"; Role="Glass_Facade" },
  @{ Id="Facade006"; Role="Facade_Alt" }
)
$Keep = @("Color","NormalGL","Roughness","AmbientOcclusion","Metalness","Displacement")
foreach ($p in $packs) {
  $zip = Join-Path $Tex ("{0}_2K-JPG.zip" -f $p.Id)
  Get-File ("https://ambientCG.com/get?file={0}_2K-JPG.zip" -f $p.Id) $zip
  $roleDir = Join-Path $Tex $p.Role
  New-Item -ItemType Directory -Force -Path $roleDir | Out-Null
  $tmp = Join-Path $Tex ("_tmp_" + $p.Id)
  if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
  Expand-Archive -Path $zip -DestinationPath $tmp -Force
  Get-ChildItem -Path $tmp -Recurse -File | ForEach-Object {
    foreach ($k in $Keep) {
      if ($_.Name -like "*_$k.jpg") {
        Copy-Item $_.FullName (Join-Path $roleDir ("{0}_{1}.jpg" -f $p.Role, $k)) -Force
      }
    }
  }
  Remove-Item -Recurse -Force $tmp
  Remove-Item -Force $zip
}

# Poly Haven HDRIs 2k
Get-File "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/2k/urban_street_01_2k.hdr" `
  (Join-Path $Hdri "City_Day_urban_street_01_2k.hdr")
Get-File "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/2k/modern_evening_street_2k.hdr" `
  (Join-Path $Hdri "City_Dusk_modern_evening_street_2k.hdr")

# Poly Haven models (gltf 2k + includes)
$models = @("street_lamp_01","painted_wooden_bench","metal_trash_can","concrete_road_barrier_02","tree_small_02")
foreach ($slug in $models) {
  $api = Invoke-RestMethod -Uri ("https://api.polyhaven.com/files/{0}" -f $slug) -Headers @{ "User-Agent" = $UA }
  $g = $api.gltf.'2k'.gltf
  $outdir = Join-Path $Props $slug
  New-Item -ItemType Directory -Force -Path $outdir | Out-Null
  Get-File $g.url (Join-Path $outdir ("{0}_2k.gltf" -f $slug))
  if ($g.include) {
    $g.include.PSObject.Properties | ForEach-Object {
      $rel = $_.Name
      $info = $_.Value
      $out = Join-Path $outdir ($rel -replace '/','\')
      Get-File $info.url $out
    }
  }
}

# manifest stub
$manifest = @{
  fetched = (Get-Date).ToString("o")
  dest = $Dest
  license = "CC0 (ambientCG + Poly Haven)"
}
$manifest | ConvertTo-Json | Set-Content (Join-Path $Dest "download_manifest.json")
Write-Host "DONE CityKit fetch -> $Dest"
Get-ChildItem -Path $Dest -Recurse -File | Measure-Object -Property Length -Sum | ForEach-Object {
  Write-Host ("Total bytes: {0} ({1:N1} MB)" -f $_.Sum, ($_.Sum/1MB))
}
