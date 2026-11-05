# Installe un thème à la place du thème « Sombre » d'Audacity (menus et barre de titre sombres).
# Usage : Installer-en-Sombre.ps1 -Theme Graphite   (ou -Theme Origine pour remettre l'officiel)
param([string]$Theme = 'Graphite')

$ici = Split-Path -Parent $MyInvocation.MyCommand.Path
$cible = 'C:\Program Files\Audacity\lib-theme-resources.dll'
$origine = Join-Path $ici 'dll\lib-theme-resources.original.dll'
$patch = Join-Path $ici 'dll\lib-theme-resources.patched.dll'

if (Get-Process audacity -ErrorAction SilentlyContinue) { Write-Host 'Fermer Audacity d''abord.'; exit 1 }
if (-not (Test-Path $origine)) { New-Item -ItemType Directory -Force (Split-Path $origine) | Out-Null; Copy-Item $cible $origine }

python -I (Join-Path $ici 'patcher.py') $Theme $origine $patch
if ($LASTEXITCODE -ne 0) { exit 1 }

# Copie dans Program Files : demande l'élévation (fenêtre UAC)
$ps = if (Get-Command pwsh -ErrorAction SilentlyContinue) { 'pwsh' } else { 'powershell' }
Start-Process $ps -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList '-NoProfile', '-Command', "Copy-Item -LiteralPath '$patch' -Destination '$cible' -Force"
if ((Get-FileHash $patch).Hash -ne (Get-FileHash $cible).Hash) { Write-Host 'Copie refusée ou échouée.'; exit 1 }

$cfg = Join-Path $env:APPDATA 'audacity\audacity.cfg'
$lignes = (Get-Content $cfg) -replace '^Theme=.*$', 'Theme=dark'
[IO.File]::WriteAllLines($cfg, $lignes, (New-Object Text.UTF8Encoding $false))
Write-Host "Thème $Theme installé à la place de Sombre."
