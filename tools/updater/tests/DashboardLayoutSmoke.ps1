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
    $badges=$header.GetControlFromPosition(0,1)
    if ($badges -isnot [System.Windows.Forms.FlowLayoutPanel] -or
        $badges.WrapContents -or
        $badges.FlowDirection -ne [System.Windows.Forms.FlowDirection]::LeftToRight -or
        -not $badges.AutoScroll) {
        throw "Branch statuses must occupy one scrollable horizontal row."
    }
    if ($badges.Controls.Count -ne 2) { throw "Expected initial work/main badges." }
    # Simulate GitHub returning nine branches: none may be dropped or wrapped.
    $branches=New-Object 'System.Collections.Generic.List[string]'
    foreach ($name in @('work','main','feature/autoloot-12340',
        'feature/autopickpocket-12340','feature/loader-12340',
        'promote/stable-infrastructure-12340','feature/new-a',
        'feature/new-b','feature/new-c')) { $branches.Add($name) }
    $arrange=$type.GetMethod('Arrange335MonitorBadges',
        [Reflection.BindingFlags]::NonPublic -bor [Reflection.BindingFlags]::Instance)
    $parameters=[object[]]::new(1)
    $parameters[0]=$branches.PSObject.BaseObject
    $null=$arrange.Invoke($form,$parameters)
    $form.PerformLayout()
    $badges.PerformLayout()
    if ($badges.Controls.Count -ne $branches.Count) {
        throw "A branch was omitted from the single status bar."
    }
    $top=$badges.Controls[0].Top
    foreach ($item in $badges.Controls) {
        if ($item.Top -ne $top) { throw "Branch badges wrapped into a second row." }
    }
    $fitWidth=($badges.ClientSize.Width - 6 * 6 - 4) / 6
    $six=New-Object 'System.Collections.Generic.List[string]'
    foreach ($name in @('work','main','feature/autoloot-12340',
        'feature/autopickpocket-12340','feature/loader-12340',
        'promote/stable-infrastructure-12340')) { $six.Add($name) }
    $parameters[0]=$six.PSObject.BaseObject
    $null=$arrange.Invoke($form,$parameters)
    $badges.PerformLayout()
    if ($badges.Controls.Count -ne 6 -or $badges.Controls[0].Width -lt 112) {
        throw "Six existing branches did not fit the compact bar."
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
    Write-Host "DASHBOARD_LAYOUT: PASS; Windows WinForms x86, 3 actions, one-row dynamic status, no overflow, tools expand/collapse"
} finally {
    if ($null -ne $form) { $form.Dispose() }
}
