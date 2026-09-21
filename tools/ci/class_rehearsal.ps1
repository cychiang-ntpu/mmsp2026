# class_rehearsal.ps1 -- rehearse a whole class on a clean Windows PC, exactly the way a student does it:
#   1. paste the one-line installer from setup_first.md            (irm ... | iex)
#   2. type every Windows PowerShell command of commands.md, in order, in the same window
#   3. the two-window parts (file transfer, chat + chunk_send) are run with a background process
# Usage:  powershell -File tools\ci\class_rehearsal.ps1 <week folder name>      e.g. wk03_0921_team1-kickoff
# ASCII only on purpose: Windows PowerShell 5.1 reads a script without BOM in the ANSI code page.
param([string]$Week = 'wk03_0921_team1-kickoff')
$ErrorActionPreference = 'Continue'
$env:PYTHONUTF8 = '1'
$results = New-Object System.Collections.ArrayList
function Note($ok, $what, $detail) {
    [void]$results.Add([pscustomobject]@{ ok = $ok; what = $what; detail = $detail })
    $tag = '[FAIL]'; $col = 'Red'
    if ($ok) { $tag = '[PASS]'; $col = 'Green' }
    Write-Host "$tag $what  $detail" -ForegroundColor $col
}

# ------------------------------------------------------------ 1. install, as the student does
Write-Host '=== 1. one-line install (irm | iex) ===' -ForegroundColor Cyan
$t0 = Get-Date
Invoke-Expression (Invoke-RestMethod 'https://raw.githubusercontent.com/cychiang-ntpu/mmsp2026/main/tools/setup/lab_setup.ps1')
$mins = [math]::Round(((Get-Date) - $t0).TotalMinutes, 1)
foreach ($c in 'git', 'gcc', 'mingw32-make', 'make', 'python') {
    $src = (Get-Command $c -ErrorAction SilentlyContinue).Source
    # a PC that already has C:\msys64 (just not on PATH) is reused by the installer, so accept that location too
    Note ([bool]$src -and ($src -like '*mmsp-tools*' -or $src -like 'C:\msys64\*')) "installed: $c" $src
}
Note $true 'install time (minutes)' $mins
Note (-not $env:MSYSTEM) 'the installer leaves no MSYSTEM variable behind in this window' "MSYSTEM='$env:MSYSTEM'"

$repo = Join-Path ([Environment]::GetFolderPath('Desktop')) 'mmsp2026'
if ($env:MMSP_REPO_DIR) { $repo = $env:MMSP_REPO_DIR }
Note (Test-Path "$repo\lectures\$Week\commands.md") 'course repo is on the Desktop' $repo
if ($env:REHEARSAL_SOURCE) {
    # test the commit being pushed, not what is already on main: overlay the checked-out tree
    Copy-Item -Recurse -Force "$env:REHEARSAL_SOURCE\lectures" "$repo\"
    Copy-Item -Recurse -Force "$env:REHEARSAL_SOURCE\team_projects" "$repo\"
}

# ------------------------------------------------------------ 2. every PowerShell command of commands.md, in order
Write-Host '=== 2. commands.md, Windows PowerShell column, top to bottom ===' -ForegroundColor Cyan
$md = Get-Content -Encoding UTF8 "$repo\lectures\$Week\commands.md"
$cmds = New-Object System.Collections.ArrayList
$inTrouble = $false
foreach ($line in $md) {
    if ($line -match '^## ') { $inTrouble = ($line -notmatch '^## [0-9]') }      # only the numbered sections hold commands
    if ($inTrouble -or $line -notmatch '^\|' -or $line -match '^\|\s*-') { continue }
    $cells = $line.Trim().Trim('|').Split('|')
    $last = $cells[$cells.Length - 1]
    $m = [regex]::Match($last, '`([^`]+)`')
    if ($m.Success) { [void]$cmds.Add($m.Groups[1].Value.Trim()) }
}
Note ($cmds.Count -ge 30) 'commands found in commands.md' $cmds.Count

Set-Location (Split-Path $repo)
$skip = '--step|^start |chat server|chat client|^ipconfig|recv 5001|send 127\.0\.0\.1 5001|chunk_send|^--probe$|^ --probe$'
$expectNonZero = 'mingw32-make test|^\.\\textlink\.exe$'
foreach ($c in $cmds) {
    if ($c -match $skip) { Write-Host "  (later / interactive) $c" -ForegroundColor DarkGray; continue }
    Write-Host "PS> $c" -ForegroundColor Yellow
    $global:LASTEXITCODE = 0
    $before = (Get-Location).Path
    $out = ''
    try { $out = (Invoke-Expression "$c 2>&1" | Out-String) } catch { $out = "EXCEPTION: $($_.Exception.Message)"; $global:LASTEXITCODE = 99 }
    $code = $LASTEXITCODE
    $tail = (($out -split "`n") | Where-Object { $_.Trim() } | Select-Object -Last 2) -join ' // '
    if ($tail.Length -gt 200) { $tail = $tail.Substring(0, 200) }
    $ok = ($code -eq 0)
    if ($c -match $expectNonZero) { $ok = ($code -ne 0) -and ($code -ne 99) }
    if ($c -match '^cd ') { $ok = ((Get-Location).Path -ne $before) -or ($c -eq 'cd mmsp2026' -and (Test-Path .\lectures)) }
    if ($c -match 'mingw32-make test') { $ok = $ok -and ($out -match 'TODO 46') -and ($out -notmatch 'FAIL [1-9]') }
    if ($c -match 'mingw32-make check') { $ok = $ok -and ($out -match 'no differences') }
    if ($c -match 'MISSISSIPPI') { $ok = $ok -and ($out -match '46') }
    if ($c -match 'wavegen') { $ok = $ok -and ($out -match 'SQNR') -and (Test-Path .\sine16.wav) }
    if ($c -match 'inspect') { $ok = $ok -and ($out -match 's16') }
    Note $ok $c "exit=$code  $tail"
}

# ------------------------------------------------------------ 3. the two-window parts (we are now in python_ref)
Write-Host '=== 3. two-window parts ===' -ForegroundColor Cyan
Note ((Get-Location).Path -like '*python_ref') 'we ended up in python_ref, as commands.md says' (Get-Location).Path
$data = "..\..\..\lectures\$Week\data\speech_osr_8k.wav"

$out = (python textlink.py inspect $data --probe 2>&1 | Out-String)
Note ($LASTEXITCODE -eq 0 -and $out.Length -gt 500) 'inspect ... --probe' "exit=$LASTEXITCODE, $($out.Length) chars of probe output"

# window A: recv   window B: send
Remove-Item -Recurse -Force .\received -ErrorAction SilentlyContinue
$recv = Start-Process python -ArgumentList 'textlink.py', 'recv', '5001', 'received' -PassThru -WindowStyle Hidden -RedirectStandardOutput recv.log -RedirectStandardError recv.err
Start-Sleep -Seconds 3
$out = (python textlink.py send 127.0.0.1 5001 textlink.py --huff 2>&1 | Out-String)
$sendCode = $LASTEXITCODE
[void]$recv.WaitForExit(20000)
$same = (Test-Path .\received\textlink.py) -and ((Get-FileHash .\received\textlink.py).Hash -eq (Get-FileHash .\textlink.py).Hash)
Note ($sendCode -eq 0 -and $same -and $out -match 'STATS') 'window A recv + window B send --huff: received file is byte-identical, STATS printed' (($out -split "`n" | Where-Object { $_ -match 'STATS' }) -join '')

# window A: Python chat server (stdin kept open by a silent ping)   window B: chunk_send
$srv = Start-Process cmd -ArgumentList '/c', 'ping -n 14 127.0.0.1 >nul | python -u textlink.py chat server 5000 > chat.log 2>&1' -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 4
$out = (python "..\..\..\lectures\$Week\examples\chunk_send.py" 127.0.0.1 5000 2>&1 | Out-String)
$csCode = $LASTEXITCODE
[void]$srv.WaitForExit(40000)          # let the server end by itself (stdin closes when ping ends) so its output is complete
$log = ''
if (Test-Path chat.log) { $log = [IO.File]::ReadAllText((Resolve-Path chat.log), [Text.Encoding]::UTF8) }
$nRaw = ([regex]::Matches($log, 'RAW \d+ B')).Count
Note ($csCode -eq 0 -and $nRaw -eq 6) 'chat server + chunk_send: server shows exactly 6 separate messages (5 sticky + 1 dripped)' "chunk_send exit=$csCode, messages shown=$nRaw"
if (-not $srv.HasExited) { try { Stop-Process -Id $srv.Id -Force } catch {} }
Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# the C starter chat: start both sides for a few seconds, they must connect and must not crash
Set-Location ..\starter
$a = Start-Process cmd -ArgumentList '/c', 'ping -n 8 127.0.0.1 >nul | textlink.exe chat server 5002 > a.log 2>&1' -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2
$b = Start-Process cmd -ArgumentList '/c', 'ping -n 5 127.0.0.1 >nul | textlink.exe chat client 127.0.0.1 5002 > b.log 2>&1' -PassThru -WindowStyle Hidden
[void]$a.WaitForExit(30000); [void]$b.WaitForExit(30000)
$la = ''; $lb = ''
if (Test-Path a.log) { $la = [IO.File]::ReadAllText((Resolve-Path a.log), [Text.Encoding]::UTF8) }
if (Test-Path b.log) { $lb = [IO.File]::ReadAllText((Resolve-Path b.log), [Text.Encoding]::UTF8) }
Note ($la.Length -gt 50 -and $lb.Length -gt 50 -and $la -match 'TODO' ) 'C starter: chat server + client start, connect, list the unfinished TODOs, and exit cleanly' "server log $($la.Length) chars, client log $($lb.Length) chars"

# ------------------------------------------------------------ summary
$bad = @($results | Where-Object { -not $_.ok })
Write-Host ''
Write-Host ("=== REHEARSAL: {0} steps, {1} failed ===" -f $results.Count, $bad.Count) -ForegroundColor Cyan
if ($env:GITHUB_STEP_SUMMARY) {
    $lines = @("## Class rehearsal ($Week): $($results.Count) steps, $($bad.Count) failed", '', '| | step | detail |', '|---|---|---|')
    foreach ($r in $results) { $mark = 'FAIL'; if ($r.ok) { $mark = 'ok' }; $lines += "| $mark | ``$($r.what)`` | $(("$($r.detail)" -replace '\|', '/')) |" }
    [IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY, ($lines -join "`n"), (New-Object Text.UTF8Encoding $false))
}
foreach ($r in $bad) { Write-Host "FAILED: $($r.what)  $($r.detail)" -ForegroundColor Red }
if ($bad.Count) { exit 1 }
