param([Parameter(Mandatory=$true)][string]$UpdaterExe)
$ErrorActionPreference = 'Stop'
$assembly = [Reflection.Assembly]::LoadFile((Resolve-Path $UpdaterExe).Path)
$guard = $assembly.GetType('WoW335Updater.GameProcessGuard', $true)
$flags = [Reflection.BindingFlags]::Static -bor [Reflection.BindingFlags]::NonPublic
$running = $guard.GetMethod('IsRunning', $flags)
$stop = $guard.GetMethod('StopForUpdateAsync', $flags)
if ($null -eq $running -or $null -eq $stop) { throw 'Missing game process guard methods' }
$root = Join-Path $env:RUNNER_TEMP ('wow335-close-game-' + [guid]::NewGuid().ToString('N'))
$other = Join-Path $env:RUNNER_TEMP ('wow335-other-game-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
[IO.Directory]::CreateDirectory($other) | Out-Null
$cmd = Join-Path $env:windir 'SysWOW64\cmd.exe'
if (-not (Test-Path $cmd)) { throw 'x86 cmd.exe required for process smoke test' }
[IO.File]::Copy($cmd, (Join-Path $root 'WoW.exe'))
[IO.File]::Copy($cmd, (Join-Path $root 'CustomClient.exe'))
[IO.File]::Copy($cmd, (Join-Path $other 'WoW.exe'))
$processes = New-Object 'System.Collections.Generic.List[System.Diagnostics.Process]'
function Start-TestGame([string]$path) {
    $p = Start-Process -FilePath $path -ArgumentList '/c ping -n 60 127.0.0.1 >nul' -WorkingDirectory $env:RUNNER_TEMP -WindowStyle Hidden -PassThru
    $processes.Add($p)
    Start-Sleep -Milliseconds 250
    if ($p.HasExited) { throw "Test fixture exited prematurely: $path" }
    return $p
}
try {
    $game = Start-TestGame (Join-Path $root 'WoW.exe')
    $unrelated = Start-TestGame (Join-Path $other 'WoW.exe')
    if (-not $running.Invoke($null, [object[]]@($root, $null))) {
        throw 'Did not detect the running game in selected directory'
    }
    $task = $stop.Invoke($null, [object[]]@($root, $null, $null))
    $count = $task.GetAwaiter().GetResult()
    $game.Refresh()
    $unrelated.Refresh()
    if ($count -lt 1 -or -not $game.HasExited -or
        $running.Invoke($null, [object[]]@($root, $null))) {
        throw 'Updater did not close selected WoW.exe and wait for exit'
    }
    if ($unrelated.HasExited) { throw 'Unrelated WoW installation was terminated' }
    $custom = Start-TestGame (Join-Path $root 'CustomClient.exe')
    if (-not $running.Invoke($null, [object[]]@($root, 'CustomClient.exe'))) {
        throw 'Did not detect installed game with a custom executable name'
    }
    $task = $stop.Invoke($null, [object[]]@($root, 'CustomClient.exe', $null))
    $count = $task.GetAwaiter().GetResult()
    $custom.Refresh()
    $unrelated.Refresh()
    if ($count -lt 1 -or -not $custom.HasExited -or $unrelated.HasExited) {
        throw 'Custom game shutdown or other-installation isolation failed'
    }
    if ($stop.Invoke($null, [object[]]@($root, $null, $null)).GetAwaiter().GetResult() -ne 0) {
        throw 'Repeated shutdown should be a no-op'
    }
    Write-Host 'UPDATE_CLOSES_GAME: PASS x86 processes; WoW.exe, renamed exe, other installation isolated'
}
finally {
    foreach ($p in $processes) {
        try {
            $p.Refresh()
            if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit(5000) | Out-Null }
        } finally { $p.Dispose() }
    }
    foreach ($folder in @($root, $other)) {
        if (Test-Path $folder) {
            try { Remove-Item -LiteralPath $folder -Recurse -Force -ErrorAction Stop }
            catch { Write-Warning ("Fixture cleanup: " + $_.Exception.Message) }
        }
    }
}
