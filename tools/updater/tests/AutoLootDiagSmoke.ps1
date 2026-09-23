param(
    [Parameter(Mandatory=$true)][string]$UpdaterExe,
    [Parameter(Mandatory=$true)][string]$ExpectedSha
)
$ErrorActionPreference = 'Stop'
$assembly = [Reflection.Assembly]::LoadFile((Resolve-Path $UpdaterExe).Path)
$names = $assembly.GetManifestResourceNames()
$source = (Resolve-Path 'src\AutoLoot\diagnostics\WoW335AutoLootDiag').Path
foreach ($leaf in @('WoW335AutoLootDiag.lua','WoW335AutoLootDiag.toc')) {
    $name = 'AutoLootDiag.' + $leaf
    if ($names -notcontains $name) { throw "Missing embedded addon resource $name" }
    $stream = $assembly.GetManifestResourceStream($name)
    try {
        $bytes = New-Object byte[] $stream.Length
        if ($stream.Read($bytes, 0, $bytes.Length) -ne $bytes.Length) { throw "Truncated resource: $name" }
        $original = [IO.File]::ReadAllBytes((Join-Path $source $leaf))
        if ($bytes.Length -ne $original.Length) { throw "Embedded byte count changed: $name" }
        $hash = [Security.Cryptography.SHA256]::Create()
        try {
            $a = [BitConverter]::ToString($hash.ComputeHash($bytes))
            $b = [BitConverter]::ToString($hash.ComputeHash($original))
            if ($a -ne $b) { throw "Embedded addon bytes changed: $name" }
        } finally { $hash.Dispose() }
    } finally { $stream.Dispose() }
}
$stream = $assembly.GetManifestResourceStream('AutoLootDiag.SourceCommit.txt')
if ($null -eq $stream) { throw "No addon SHA evidence" }
$reader = [IO.StreamReader]::new($stream)
try { $sha = $reader.ReadToEnd().Trim() } finally { $reader.Dispose() }
if ($sha -ne $ExpectedSha) { throw "Embedded addon source SHA differs from build SHA" }

$type = $assembly.GetType('WoW335Updater.AutoLootDiagSupport', $true)
$flags = [Reflection.BindingFlags]::NonPublic -bor [Reflection.BindingFlags]::Static
$collect = $type.GetMethod('CollectLog', $flags)
$sanitize = $type.GetMethod('SanitizeLog', $flags)
$install = $type.GetMethod('Install', $flags)
if ($null -eq $collect -or $null -eq $sanitize -or $null -eq $install) {
    throw "Updater diagnostic integration missing methods"
}
$folder = Join-Path $env:RUNNER_TEMP ('wow335-al-test-' + [guid]::NewGuid().ToString('N'))
$save = Join-Path $folder 'WTF\Account\DO_NOT_UPLOAD_ME\SavedVariables'
[IO.Directory]::CreateDirectory($save) | Out-Null
$savedFile = Join-Path $save 'WoW335AutoLootDiag.lua'
$fixture = @'
WoW335AutoLootDiagLog = {
    ["entries"] = {
        "2026-09-23 10:00:00 LOOT_OPENED num=2",
        "2026-09-23 10:00:01 UI_ERROR contact@example.com 192.168.1.2",
        "2026-09-23 10:00:02 LOOT_CLOSED attempts=1",
        "IGNORED_SECRET password=NEVER_INCLUDE_ME",
    },
    ["client"] = "REALM_PRIVATE",
}
'@
[IO.File]::WriteAllText($savedFile, $fixture)
try {
    $single = New-Object 'object[]' 1
    $single[0] = [string]$folder
    $events = [string]$collect.Invoke($null, $single)
    if ($events -notmatch 'LOOT_OPENED' -or $events -notmatch 'LOOT_CLOSED') { throw "Failed to collect expected diagnostic lines" }
    if ($events -match 'DO_NOT_UPLOAD_ME|NEVER_INCLUDE_ME|REALM_PRIVATE') { throw "Raw account or SavedVariables data leaked" }
    $single[0] = [string]$events
    $safe = [string]$sanitize.Invoke($null, $single)
    if ($safe -notmatch '<EMAIL>' -or $safe -notmatch '<IP>' -or $safe -match 'contact@example.com|192.168.1.2') {
        throw "Diagnostic privacy redaction failed"
    }
    $rejected = $false
    try {
        $triple = New-Object 'object[]' 3
        $triple[0] = [string]$folder
        $triple[1] = $assembly
        $triple[2] = $false
        $install.Invoke($null, $triple) | Out-Null
    } catch {
        $rejected = $_.Exception.ToString().Contains('Wow.exe')
    }
    if (-not $rejected -or (Test-Path (Join-Path $folder 'Interface'))) {
        throw "Addon installer failed to reject missing exact pinned Wow.exe"
    }
    [IO.File]::Copy((Resolve-Path 'Wow.exe').Path, (Join-Path $folder 'Wow.exe'))
    $triple[2] = $false
    $first = [string]$install.Invoke($null, $triple)
    $addonDir = Join-Path $folder 'Interface\AddOns\WoW335AutoLootDiag'
    $scriptPath = Join-Path $addonDir 'WoW335AutoLootDiag.lua'
    $tocPath = Join-Path $addonDir 'WoW335AutoLootDiag.toc'
    if (-not (Test-Path $scriptPath) -or -not (Test-Path $tocPath) -or
        -not (Test-Path (Join-Path $addonDir '.wow335_diag_managed.txt'))) {
        throw "Addon installation missing managed file set"
    }
    $scriptText = [IO.File]::ReadAllText($scriptPath)
    if (-not $scriptText.Contains('local enabled = false')) { throw "Addon not disabled by default" }
    $sourceScript = (Resolve-Path 'src\AutoLoot\diagnostics\WoW335AutoLootDiag\WoW335AutoLootDiag.lua').Path
    if ([IO.File]::ReadAllText($sourceScript) -ne $scriptText) { throw "Installer wrote different script bytes" }
    $idempotent = [string]$install.Invoke($null, $triple)
    if ($idempotent -notmatch 'zainstalowany' -or
        (Test-Path (Join-Path $folder '.wow335_updater\autoloot_diag_backups'))) {
        throw "Repeat install made an unnecessary backup or overwrote matching files"
    }
    [IO.File]::AppendAllText($scriptPath, [Environment]::NewLine + '-- existing content to preserve in backup')
    [IO.File]::WriteAllText((Join-Path $addonDir 'unrelated.txt'), 'do not discard')
    $blockedReplacement = $false
    try { $install.Invoke($null, $triple) | Out-Null }
    catch { $blockedReplacement = $_.Exception.ToString() -match 'Wymagana jawna zgoda' }
    if (-not $blockedReplacement) { throw "Existing modified addon overwritten without consent" }
    $triple[2] = $true
    $second = [string]$install.Invoke($null, $triple)
    if (-not (Test-Path $scriptPath) -or (Test-Path (Join-Path $addonDir 'unrelated.txt')) -or
        [IO.File]::ReadAllText($scriptPath) -ne [IO.File]::ReadAllText($sourceScript)) {
        throw "Addon replacement did not restore exact staged content"
    }
    $backupRoot = Join-Path $folder '.wow335_updater\autoloot_diag_backups'
    $backups = @(Get-ChildItem -LiteralPath $backupRoot -Directory)
    if ($backups.Count -ne 1 -or
        -not (Test-Path (Join-Path $backups[0].FullName 'unrelated.txt'))) {
        throw "Previous addon directory was not preserved in backup"
    }
    if ([IO.File]::ReadAllText((Join-Path $backups[0].FullName 'unrelated.txt')) -ne 'do not discard') {
        throw "Existing user content lost during replacement"
    }
    Write-Host "AUTOLOOT_UPDATER_SMOKE: PASS x86 resources, exact SHA, log whitelist/privacy, wrong EXE rejection, addon install and backup/restore"
} finally {
    if (Test-Path $folder) { Remove-Item -LiteralPath $folder -Recurse -Force }
}
