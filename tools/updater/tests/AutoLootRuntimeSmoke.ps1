param(
    [Parameter(Mandatory=$true)][string]$UpdaterExe,
    [Parameter(Mandatory=$true)][string]$LoaderExe
)
$ErrorActionPreference='Stop'
$updater=(Resolve-Path $UpdaterExe).Path
$loader=(Resolve-Path $LoaderExe).Path
$assembly=[Reflection.Assembly]::LoadFile($updater)
$stream=$assembly.GetManifestResourceStream("AutoLootRuntime.Launcher.exe")
if ($null -eq $stream) { throw "Missing regular AutoLoot launcher resource" }
$m=[IO.MemoryStream]::new()
try { $stream.CopyTo($m) } finally { $stream.Dispose() }
$a=$m.ToArray()
$b=[IO.File]::ReadAllBytes($loader)
if ($a.Length -ne $b.Length -or $a.Length -lt 512) {throw "Launcher size mismatch"}
$sha=[Security.Cryptography.SHA256]::Create()
try {
    if ([BitConverter]::ToString($sha.ComputeHash($a)) -ne
        [BitConverter]::ToString($sha.ComputeHash($b))) {throw "Launcher SHA mismatch"}
} finally {$sha.Dispose()}
if ($a[0] -ne 77 -or $a[1] -ne 90) { throw "Launcher is not MZ" }
$offset=[BitConverter]::ToInt32($a,0x3c)
if ([BitConverter]::ToUInt16($a,$offset+4) -ne 0x014c -or
    [BitConverter]::ToUInt16($a,$offset+24) -ne 0x010b) {
    throw "Launcher is not PE32 x86"
}
$form=$assembly.GetType('WoW335Updater.MainForm',$true)
$method=$form.GetMethod('TryLaunchInstalledAutoLoot',
    [Reflection.BindingFlags]::NonPublic -bor [Reflection.BindingFlags]::Instance)
if ($null -eq $method) {throw "Missing updater normal-runtime launch integration"}
$code=[IO.File]::ReadAllText('tools/updater/WoW335Updater.cs')
$feature=[IO.File]::ReadAllText('tools/updater/UpdaterAutoLootRuntimeFeature.cs')
if (-not $code.Contains('TryLaunchInstalledAutoLoot(root, exe, state)') -or
    -not $code.Contains('state["managed_sha256"] = fileHashes;') -or
    -not $feature.Contains('Sha256File(dll)') -or
    -not $feature.Contains('Sha256File(order)')) {
    throw "Normal AutoLoot launch does not enforce installed manifest hashes"
}
Write-Host "AUTOLOOT_NORMAL_LOADER: PASS, embedded PE32 x86, manifest-gated launch"
