param(
    [Parameter(Mandatory=$true)][string]$UpdaterExe
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
$updater=(Resolve-Path $UpdaterExe).Path
$assembly=[Reflection.Assembly]::LoadFile($updater)
$type=$assembly.GetType('WoW335Updater.MainForm',$true)
$form=[Activator]::CreateInstance($type)
try {
    if ($form.Controls.Count -ne 1) { throw "Dashboard must have one root surface." }
    $root=$form.Controls[0]
    if ($root -isnot [System.Windows.Forms.TableLayoutPanel] -or
        $root.RowCount -ne 5 -or $root.ColumnCount -ne 1) {
        throw "Unexpected dashboard root layout."
    }
    $build=$root.GetControlFromPosition(0,2)
    $actions=$build.GetControlFromPosition(0,5)
    if ($actions.ColumnCount -ne 3) { throw "Expected exactly three primary actions." }
    $expected=@("Sprawd*","Aktualizuj","Aktualizuj i uruchom")
    for ($i=0; $i -lt 3; $i++) {
        $button=$actions.GetControlFromPosition($i,0)
        if ($button -isnot [System.Windows.Forms.Button] -or $button.Text -notlike $expected[$i]) {
            throw "Incorrect primary action at column $i; got [$($button.GetType().FullName)] [$($button.Text)]."
        }
    }
    # The available card interior is 166px minus its bottom margin (9)
    # and vertical padding (6). Its absolute rows must fit without clipping.
    $height=0
    foreach ($row in $build.RowStyles) {
        if ($row.SizeType -ne [System.Windows.Forms.SizeType]::Absolute) {
            throw "Updater card has an unexpected non-absolute row."
        }
        $height+=$row.Height
    }
    if ($height -gt 151) { throw "Updater card rows overflow: $height > 151." }
    $header=$root.GetControlFromPosition(0,0)
    $monitor=$header.GetControlFromPosition(1,0)
    $badges=$monitor.GetControlFromPosition(0,0)
    if ($badges.ColumnCount -ne 3 -or $badges.RowCount -ne 2) {
        throw "Branch status strips must be a compact 3x2 grid."
    }
    $tools=$root.GetControlFromPosition(0,3)
    if ($root.RowStyles[3].Height -ne 37) { throw "Advanced tools must start collapsed." }
    $toggle=$tools.GetControlFromPosition(0,0)
    $onClick=[System.Windows.Forms.Control].GetMethod("OnClick",
        [Reflection.BindingFlags]::Instance -bor [Reflection.BindingFlags]::NonPublic)
    $null=$onClick.Invoke($toggle,[object[]]@([System.EventArgs]::Empty))
    if ($root.RowStyles[3].Height -ne 145) { throw "Advanced tools did not expand." }
    $details=$tools.GetControlFromPosition(0,1)
    if ($details.Controls.Count -ne 3) { throw "Advanced controls were lost." }
    $null=$onClick.Invoke($toggle,[object[]]@([System.EventArgs]::Empty))
    if ($root.RowStyles[3].Height -ne 37) { throw "Advanced tools did not collapse." }
    Write-Host "DASHBOARD_LAYOUT: PASS; Windows WinForms x86, 3 actions, 3x2 status, no overflow, tools expand/collapse"
} finally {
    if ($null -ne $form) { $form.Dispose() }
}
