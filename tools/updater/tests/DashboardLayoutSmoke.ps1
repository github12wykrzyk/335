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
    if ($badges.Controls.Count -ne 5) { throw "Expected four main badges and the remaining-branches button." }
    $branches=New-Object 'System.Collections.Generic.List[string]'
    foreach ($name in @('work','main','feature/autoloot-12340',
        'feature/autopickpocket-12340','feature/autopickpocket-packets-12340',
        'feature/loader-12340','promote/stable-infrastructure-12340',
        'feature/new-a','feature/new-b','feature/new-c')) { $branches.Add($name) }
    $arrange=$type.GetMethod('Arrange335MonitorBadges',
        [Reflection.BindingFlags]::NonPublic -bor [Reflection.BindingFlags]::Instance)
    $set=$type.GetMethod('Set335Badge',
        [Reflection.BindingFlags]::NonPublic -bor [Reflection.BindingFlags]::Instance)
    $parameters=[object[]]::new(1)
    $parameters[0]=$branches.PSObject.BaseObject
    $null=$arrange.Invoke($form,$parameters)
    $form.PerformLayout()
    $badges.PerformLayout()
    if ($badges.Controls.Count -ne 5) { throw "Only four featured statuses and one menu should be on the bar." }
    $more=$badges.Controls[4]
    $menu=$more.ContextMenuStrip
    if ($more -isnot [System.Windows.Forms.Button] -or
        $null -eq $menu -or $menu.Items.Count -ne 6 -or $more.Text -notmatch '6') {
        throw "Six non-featured branches must be accessible in the expandable menu."
    }
    $menuNames=@($menu.Items | ForEach-Object { $_.Tag })
    if ($menuNames -notcontains 'feature/new-c' -or
        $menuNames -notcontains 'promote/stable-infrastructure-12340') {
        throw "A dynamically added or historical branch disappeared from the menu."
    }
    # Status priority must be determined from current data, not fixed branch order.
    $null=$set.Invoke($form,[object[]]@('work','UNKNOWN','','missing'))
    $null=$set.Invoke($form,[object[]]@('main','SUCCESS','1234567890','passed'))
    $null=$set.Invoke($form,[object[]]@('feature/autopickpocket-12340','RUNNING','1234567890','running'))
    $null=$set.Invoke($form,[object[]]@('feature/autopickpocket-packets-12340','FAIL','1234567890','failed'))
    $null=$set.Invoke($form,[object[]]@('feature/new-c','FAIL','1234567890','failed'))
    $null=$arrange.Invoke($form,$parameters)
    $badges.PerformLayout()
    $expected=@('PP pakiety','PP natywny','main','work')
    for ($i=0; $i -lt 4; $i++) {
        if ($badges.Controls[$i].Controls[0].Text -notlike "$($expected[$i])*") {
            throw "Incorrect CI status priority at $i: $($badges.Controls[$i].Controls[0].Text)"
        }
    }
    if ($menu.Items[0].Tag -ne 'feature/new-c') {
        throw "Expandable menu did not sort the failing branch first."
    }
    $null=$set.Invoke($form,[object[]]@('work','FAIL','1234567890','failed'))
    $null=$set.Invoke($form,[object[]]@('feature/autopickpocket-packets-12340','SUCCESS','1234567890','passed'))
    $null=$arrange.Invoke($form,$parameters)
    if ($badges.Controls[0].Controls[0].Text -notlike 'work*' -or
        $badges.Controls[3].Controls[0].Text -notlike 'PP pakiety*') {
        throw "Status changes did not reorder the main bar dynamically."
    }
    $top=$badges.Controls[0].Top
    foreach ($item in $badges.Controls) {
        if ($item.Top -ne $top) { throw "Featured branch badges wrapped onto another row." }
    }
    if ($badges.HorizontalScroll.Visible) {
        throw "Four featured badges and menu do not fit the compact dashboard."
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
