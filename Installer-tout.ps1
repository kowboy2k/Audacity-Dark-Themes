# Installe d'un coup le thème (à la place de Sombre) et le module mod-sombre, avec une seule élévation.
# Usage : Installer-tout.ps1 [-Theme Graphite]
#         Installer-tout.ps1 -Retirer   (DLL d'origine remise, module retiré, Theme=light)
# -Audacity et -Config servent aux essais sur des copies.
# Compatible Windows PowerShell 5 et PowerShell 7.
param(
    [string]$Theme = 'Graphite',
    [switch]$Retirer,
    [string]$Audacity = 'C:\Program Files\Audacity',
    [string]$Config = (Join-Path $env:APPDATA 'audacity\audacity.cfg')
)

$VERSION_ATTENDUE = '3.5.1'  # le module et le patch ne valent que pour cette version
$ici = Split-Path -Parent $MyInvocation.MyCommand.Path
$origine = Join-Path $ici 'dll\lib-theme-resources.original.dll'
$p1 = Join-Path $ici 'dll\lib-theme-resources.patched.dll'
$p2 = Join-Path $ici 'module\mod-sombre.dll'
$c1 = Join-Path $Audacity 'lib-theme-resources.dll'
$c2 = Join-Path $Audacity 'modules\mod-sombre.dll'
$exe = Join-Path $Audacity 'Audacity.exe'
$ps = if (Get-Command pwsh -ErrorAction SilentlyContinue) { 'pwsh' } else { 'powershell' }

function Stop-Erreur($message) { Write-Host $message; exit 1 }

# Exécute des commandes de copie ; élévation (fenêtre UAC) seulement si le dossier n'est pas inscriptible
function Invoke-Copie([string]$commandes, [string[]]$dossiers) {
    $libre = $true
    foreach ($d in $dossiers) {
        $essai = Join-Path $d ([IO.Path]::GetRandomFileName())
        try { [IO.File]::WriteAllText($essai, ''); Remove-Item -LiteralPath $essai -Force }
        catch { $libre = $false }
    }
    if ($libre) { Invoke-Expression $commandes }
    else { Start-Process $ps -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList '-NoProfile', '-Command', $commandes }
}

function Get-Lignes { [Collections.Generic.List[string]](Get-Content -LiteralPath $Config | Where-Object { $_ -notmatch '^mod-sombre=' }) }
function Set-Lignes($lignes) { [IO.File]::WriteAllLines($Config, [string[]]$lignes, (New-Object Text.UTF8Encoding $false)) }

# Vérifications communes
if (-not (Test-Path -LiteralPath $exe)) { Stop-Erreur "Audacity introuvable : $exe" }
$vi = (Get-Item -LiteralPath $exe).VersionInfo
$version = '{0}.{1}.{2}' -f $vi.FileMajorPart, $vi.FileMinorPart, $vi.FileBuildPart
# Chemin illisible (autre compte) : considéré comme ouvert, par prudence
$ouvert = Get-Process audacity -ErrorAction SilentlyContinue | Where-Object { -not $_.Path -or $_.Path.StartsWith($Audacity, 'OrdinalIgnoreCase') }
if ($ouvert) { Stop-Erreur 'Fermer Audacity d''abord.' }
if (-not (Test-Path -LiteralPath $Config)) { Stop-Erreur "Configuration d'Audacity introuvable : $Config (lancer Audacity une fois)" }

if ($Retirer) {
    # La DLL d'origine sauvegardée ne vaut que pour 3.5.1 : sur une autre version, la mise à jour a déjà remis la sienne
    $commandes = @()
    if (Test-Path -LiteralPath $c2) { $commandes += "Remove-Item -LiteralPath '$c2' -Force" }
    if ($version -eq $VERSION_ATTENDUE -and (Test-Path -LiteralPath $origine) -and
        (Get-FileHash -LiteralPath $origine).Hash -ne (Get-FileHash -LiteralPath $c1).Hash) {
        $commandes += "Copy-Item -LiteralPath '$origine' -Destination '$c1' -Force"
    }
    elseif ($version -ne $VERSION_ATTENDUE) { Write-Host "Audacity $version : DLL laissée telle quelle (la sauvegarde vaut pour $VERSION_ATTENDUE)." }
    elseif (-not (Test-Path -LiteralPath $origine)) { Write-Host "Pas de sauvegarde de la DLL d'origine : si le thème Sombre reste modifié, réinstaller Audacity $VERSION_ATTENDUE." }
    if ($commandes) { Invoke-Copie ($commandes -join '; ') @($Audacity, (Split-Path $c2)) }
    if (Test-Path -LiteralPath $c2) { Stop-Erreur "Suppression refusée ou échouée : $c2" }
    if ($version -eq $VERSION_ATTENDUE -and (Test-Path -LiteralPath $origine) -and
        (Get-FileHash -LiteralPath $origine).Hash -ne (Get-FileHash -LiteralPath $c1).Hash) {
        Stop-Erreur "Copie refusée ou échouée : $c1"
    }
    Set-Lignes (Get-Lignes | ForEach-Object { if ($_ -match '^Theme=') { 'Theme=light' } else { $_ } })
    Write-Host 'Thème et module retirés ; Audacity repasse en thème Clair.'
    exit 0
}

if ($version -ne $VERSION_ATTENDUE) {
    Stop-Erreur "Audacity $version installé : ce thème et son module ne valent que pour Audacity $VERSION_ATTENDUE. Rien n'a été modifié."
}
if (-not (Get-Command python -ErrorAction SilentlyContinue)) { Stop-Erreur 'Python est nécessaire (python introuvable).' }

# Sauvegarde de la DLL d'origine (PC neuf) ; jamais une DLL déjà patchée
if (-not (Test-Path -LiteralPath $origine)) {
    python -I (Join-Path $ici 'patcher.py') --origine $c1
    if ($LASTEXITCODE -ne 0) { Stop-Erreur "La DLL installée n'est pas l'originale et aucune sauvegarde n'existe : réinstaller Audacity $VERSION_ATTENDUE." }
    New-Item -ItemType Directory -Force (Split-Path $origine) | Out-Null
    Copy-Item -LiteralPath $c1 -Destination $origine
}

python -I (Join-Path $ici 'patcher.py') $Theme $origine $p1
if ($LASTEXITCODE -ne 0) { exit 1 }
Invoke-Copie "Copy-Item -LiteralPath '$p1' -Destination '$c1' -Force; Copy-Item -LiteralPath '$p2' -Destination '$c2' -Force" @($Audacity, (Split-Path $c2))
foreach ($c in @(@($p1, $c1), @($p2, $c2))) {
    if (-not (Test-Path -LiteralPath $c[1]) -or (Get-FileHash -LiteralPath $c[0]).Hash -ne (Get-FileHash -LiteralPath $c[1]).Hash) {
        Stop-Erreur "Copie refusée ou échouée : $($c[1])"
    }
}

# Config : thème Sombre, module activé avec la date du fichier (sinon Audacity le croit nouveau)
$date = (Get-Item -LiteralPath $c2).LastWriteTime.ToString('yyyy-MM-ddTHH:mm:ss')
$lignes = [Collections.Generic.List[string]](Get-Lignes | ForEach-Object { if ($_ -match '^Theme=') { 'Theme=dark' } else { $_ } })
$valeurs = @{ 'Module' = '1'; 'ModulePath' = $c2.Replace('\', '\\'); 'ModuleDateTime' = $date }
foreach ($section in 'Module', 'ModulePath', 'ModuleDateTime') {
    $i = $lignes.IndexOf("[$section]")
    if ($i -lt 0) { $lignes.Add("[$section]"); $i = $lignes.Count - 1 }
    $lignes.Insert($i + 1, "mod-sombre=$($valeurs[$section])")
}
Set-Lignes $lignes
Write-Host "Thème $Theme et module installés."
