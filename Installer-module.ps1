# Installe (ou retire avec -Retirer) le module mod-sombre : barre de titre, menus et barre d'état sombres.
param([switch]$Retirer)

$ici = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $ici 'module\mod-sombre.dll'
$cible = 'C:\Program Files\Audacity\modules\mod-sombre.dll'
$cfg = Join-Path $env:APPDATA 'audacity\audacity.cfg'

if (Get-Process audacity -ErrorAction SilentlyContinue) { Write-Host 'Fermer Audacity d''abord.'; exit 1 }

$cmd = if ($Retirer) { "Remove-Item -LiteralPath '$cible' -Force -ErrorAction SilentlyContinue" }
       else { "Copy-Item -LiteralPath '$src' -Destination '$cible' -Force" }
$ps = if (Get-Command pwsh -ErrorAction SilentlyContinue) { 'pwsh' } else { 'powershell' }
Start-Process $ps -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList '-NoProfile', '-Command', $cmd

# Entrées de config : statut 1 = activé ; la date doit être celle du fichier, sinon Audacity le traite comme nouveau
$lignes = [Collections.Generic.List[string]](Get-Content $cfg | ? { $_ -notmatch '^mod-sombre=' })
if (-not $Retirer) {
    if (-not (Test-Path $cible) -or (Get-FileHash $src).Hash -ne (Get-FileHash $cible).Hash) { Write-Host 'Copie refusée ou échouée.'; exit 1 }
    $date = (Get-Item $cible).LastWriteTime.ToString('yyyy-MM-ddTHH:mm:ss')
    $valeurs = @{ 'Module' = '1'; 'ModulePath' = $cible.Replace('\', '\\'); 'ModuleDateTime' = $date }
    foreach ($section in 'Module', 'ModulePath', 'ModuleDateTime') {
        $i = $lignes.IndexOf("[$section]")
        if ($i -lt 0) { $lignes.Add("[$section]"); $i = $lignes.Count - 1 }
        $lignes.Insert($i + 1, "mod-sombre=$($valeurs[$section])")
    }
}
[IO.File]::WriteAllLines($cfg, $lignes, (New-Object Text.UTF8Encoding $false))
Write-Host $(if ($Retirer) { 'Module retiré.' } else { 'Module installé.' })
