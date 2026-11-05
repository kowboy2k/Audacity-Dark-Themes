# Installe un des thèmes sombres comme thème « Personnalisé » d'Audacity.
param([string]$Theme)

$ici = Split-Path -Parent $MyInvocation.MyCommand.Path
$themes = Get-ChildItem (Join-Path $ici 'themes') -Directory | Sort-Object Name
if (-not $Theme) {
    Write-Host "Thèmes disponibles :"
    for ($i = 0; $i -lt $themes.Count; $i++) { Write-Host ("  {0}. {1}" -f ($i + 1), $themes[$i].Name) }
    $n = Read-Host "Numéro"
    $Theme = $themes[[int]$n - 1].Name
}
$src = Join-Path $ici "themes\$Theme\ImageCache.png"
if (-not (Test-Path $src)) { Write-Host "Thème introuvable : $Theme"; exit 1 }

$dest = Join-Path $env:APPDATA 'audacity\Theme\custom'
New-Item -ItemType Directory -Force $dest | Out-Null
Copy-Item $src (Join-Path $dest 'ImageCache.png') -Force
Write-Host "Thème $Theme installé."

if (Get-Process audacity -ErrorAction SilentlyContinue) {
    Write-Host "Audacity est ouvert : Édition > Préférences > Interface > Thème > Personnalisé (re-choisir pour recharger)."
} else {
    $cfg = Join-Path $env:APPDATA 'audacity\audacity.cfg'
    $lignes = (Get-Content $cfg) -replace '^Theme=.*$', 'Theme=custom'
    [IO.File]::WriteAllLines($cfg, $lignes, (New-Object Text.UTF8Encoding $false))
    Write-Host "Audacity utilisera ce thème au prochain lancement."
}
