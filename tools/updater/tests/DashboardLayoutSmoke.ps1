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
    # New yellow ESP must follow red and displace green regardless of branch name.
    $esp='feature/player-esp-12340'
    $branches.Add($esp)
    $null=$arrange.Invoke($form,$parameters) # initialize newly discovered badge before assigning status
    $null=$set.Invoke($form,[object[]]@('work','SUCCESS','1234567890','passed'))
    $null=$set.Invoke($form,[object[]]@('main','SUCCESS','1234567890','passed'))
    $null=$set.Invoke($form,[object[]]@('feature/autopickpocket-12340','SUCCESS','1234567890','passed'))
    $null=$set.Invoke($form,[object[]]@('feature/autopickpocket-packets-12340','FAIL','1234567890','failed'))
    $null=$set.Invoke($form,[object[]]@($esp,'RUNNING','1234567890','running'))
    $null=$arrange.Invoke($form,$parameters)
    $badges.PerformLayout()
    $expected=@('PP pakiety','player-esp-12340','work','main')
    for ($i=0; $i -lt 4; $i++) {
        if ($badges.Controls[$i].Controls[0].Text -notlike "$($expected[$i])*") {
            throw "Global status priority failed at $($i): $($badges.Controls[$i].Controls[0].Text)"
        }
    }
    if (@($menu.Items | ForEach-Object { $_.Tag }) -contains $esp) {
        throw "Running ESP was hidden behind green branches in the overflow menu."
    }
    # Brand-new red branches also displace older green statuses.
    $newFail='feature/new-critical-12340'
    $branches.Add($newFail)
    $null=$arrange.Invoke($form,$parameters)
    $null=$set.Invoke($form,[object[]]@($newFail,'FAIL','1234567890','failed'))
    $null=$arrange.Invoke($form,$parameters)
    if ($badges.Controls[1].Controls[0].Text -notlike 'new-critical-12340*' -or
        $badges.Controls[2].Controls[0].Text -notlike 'player-esp-12340*') {
        throw "New red and yellow branches did not displace green statuses."
    }
    # More urgent branches than slots: menu retains overflow in the SAME order.
    foreach ($n in 1..5) { $branches.Add("feature/critical-$n") }
    $null=$arrange.Invoke($form,$parameters)
    foreach ($n in 1..5) {
        $name="feature/critical-$n"
        $null=$set.Invoke($form,[object[]]@($name,'FAIL','1234567890','failed'))
    }
    $null=$arrange.Invoke($form,$parameters)
    if ($badges.Controls[0].Controls[0].Text -notlike 'PP pakiety*' -or
        $menu.Items[0].Tag -notlike 'feature/critical-*') {
        throw "Overflowed failures must precede yellow and green menu items."
    }
    $overflowNames=@($menu.Items | ForEach-Object { $_.Tag })
    if ($overflowNames -notcontains $esp -or $overflowNames -notcontains 'work') {
        throw "Urgent overflow or green fallback was lost."
    }
    # Dynamic status change and removal of deleted branches on refresh.
    $null=$set.Invoke($form,[object[]]@('feature/autopickpocket-packets-12340','SUCCESS','1234567890','passed'))
    $fresh=New-Object 'System.Collections.Generic.List[string]'
    foreach ($name in @('work','main',$esp,'feature/autopickpocket-packets-12340')) {
        $fresh.Add($name)
    }
    $parameters[0]=$fresh.PSObject.BaseObject
    $null=$arrange.Invoke($form,$parameters)
    if ($badges.Controls[0].Controls[0].Text -notlike 'player-esp-12340*' -or
        $menu.Items.Count -ne 0 -or -not $badges.Controls[4] -or
        $badges.Controls[4].Enabled) {
        throw "Latest CI status/discovered branch set did not replace stale data."
    }
    $top=$badges.Controls[0].Top
    foreach ($item in $badges.Controls) {
        if ($item.Top -ne $top) { throw "Branch badges wrapped onto another row." }
    }
    if ($badges.HorizontalScroll.Visible) {
        throw "Four priority badges and menu do not fit the compact dashboard."
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
