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
    $events = [string]$collect.Invoke($null, [object[]]@($folder))
    if ($events -notmatch 'LOOT_OPENED' -or $events -notmatch 'LOOT_CLOSED') { throw "Failed to collect expected diagnostic lines" }
    if ($events -match 'DO_NOT_UPLOAD_ME|NEVER_INCLUDE_ME|REALM_PRIVATE') { throw "Raw account or SavedVariables data leaked" }
    $safe = [string]$sanitize.Invoke($null, [object[]]@($events))
    if ($safe -notmatch '<EMAIL>' -or $safe -notmatch '<IP>' -or $safe -match 'contact@example.com|192.168.1.2') {
        throw "Diagnostic privacy redaction failed"
    }
    $rejected = $false
    try {
        $install.Invoke($null, [object[]]@($folder, $assembly, $false)) | Out-Null
    } catch [Reflection.TargetInvocationException] {
        $rejected = $_.Exception.InnerException.Message -match 'Wow.exe'
    }
    if (-not $rejected -or (Test-Path (Join-Path $folder 'Interface'))) {
        throw "Addon installer failed to reject missing exact pinned Wow.exe"
    }
    Write-Host "AUTOLOOT_UPDATER_SMOKE: PASS embedded exact-SHA addon, saved-variable whitelist, privacy redaction, missing-client fail-closed"
} finally {
    if (Test-Path $folder) { Remove-Item -LiteralPath $folder -Recurse -Force }
}
