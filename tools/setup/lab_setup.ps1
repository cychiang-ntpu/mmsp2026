# lab_setup.ps1 -- MMSP 2026: set up a classroom (shared) Windows PC in one step. No administrator rights needed.
#
# Run in PowerShell:
#   irm https://raw.githubusercontent.com/cychiang-ntpu/mmsp2026/main/tools/setup/lab_setup.ps1 | iex
#
# What it does (anything already on the PC is kept and skipped):
#   1. Git      -> PortableGit            (git-for-windows official release)
#   2. GCC+make -> MSYS2 base + pacman    (msys2.org official self-extracting archive; ucrt64 gcc and make)
#   3. Python 3 -> official "python" NuGet package from python.org, plus numpy
#   4. adds them to the user PATH (and to this window), clones / updates the course repo on the Desktop,
#      builds this week's examples and runs one of them as a check.
# Everything goes under one folder (default C:\mmsp-tools, or %USERPROFILE%\mmsp-tools if C:\ is not writable),
# so removing that folder removes everything. Messages are in plain English on purpose (avoids code page problems).
#
# Optional environment variables:  MMSP_ROOT = install folder;  MMSP_FORCE = 1 ignores tools already on the PC
#                                  MMSP_REPO_DIR = where to clone the course repo (default: Desktop\mmsp2026)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
$t0 = Get-Date
$force = ($env:MMSP_FORCE -eq '1')

function Say($msg)  { Write-Host "[mmsp] $msg" -ForegroundColor Cyan }
function Good($msg) { Write-Host "[ ok ] $msg" -ForegroundColor Green }
function Bad($msg)  { Write-Host "[FAIL] $msg" -ForegroundColor Red }
# Windows PowerShell 5.1 turns whatever a program writes to stderr into an error when it is redirected;
# run external programs through this so that harmless progress messages do not stop the script.
function Native([scriptblock]$sb) { $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'; try { & $sb } finally { $ErrorActionPreference = $old } }
function Have($cmd) { if ($force) { return $false }; return [bool](Get-Command $cmd -ErrorAction SilentlyContinue) }

function Download($url, $dest) {
    Say "downloading $url"
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl) {
        & $curl.Source -L --fail --silent --show-error --retry 3 -o $dest $url
        if ($LASTEXITCODE -ne 0) { throw "download failed: $url" }
    } else {
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
    }
    if (-not (Test-Path $dest) -or (Get-Item $dest).Length -lt 1MB) { throw "download looks broken: $url" }
}

function Add-ToPath($dir) {
    if (-not (Test-Path $dir)) { return }
    if (($env:Path -split ';') -notcontains $dir) { $env:Path = "$dir;$env:Path" }
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    if (-not $user) { $user = '' }
    if (($user -split ';') -notcontains $dir) {
        [Environment]::SetEnvironmentVariable('Path', ($dir + ';' + $user).TrimEnd(';'), 'User')
    }
}

# ---------------------------------------------------------------- where to install
$root = $env:MMSP_ROOT
if (-not $root) {
    $root = 'C:\mmsp-tools'
    try { New-Item -ItemType Directory -Force -Path $root | Out-Null; [IO.File]::WriteAllText("$root\.w", 'x'); Remove-Item "$root\.w" -Force }
    catch { $root = Join-Path $env:USERPROFILE 'mmsp-tools' }
}
if ($root -match '\s') { throw "The install folder must not contain spaces (MSYS2 does not like them): $root  -- set MMSP_ROOT to another folder." }
New-Item -ItemType Directory -Force -Path $root | Out-Null
$dl = Join-Path $root '_downloads'
New-Item -ItemType Directory -Force -Path $dl | Out-Null
Say "install folder: $root"

# ---------------------------------------------------------------- 1. Git
if (Have git) { Good "git already here: $((Get-Command git).Source)" }
else {
    $gitDir = Join-Path $root 'git'
    if (-not (Test-Path "$gitDir\cmd\git.exe")) {
        Say 'Git: looking up the latest PortableGit release'
        $rel = Invoke-RestMethod 'https://api.github.com/repos/git-for-windows/git/releases/latest' -UseBasicParsing
        $asset = $rel.assets | Where-Object { $_.name -match '^PortableGit-.*-64-bit\.7z\.exe$' } | Select-Object -First 1
        if (-not $asset) { throw 'cannot find the PortableGit download' }
        $exe = Join-Path $dl $asset.name
        Download $asset.browser_download_url $exe
        Say 'Git: unpacking (about 1 minute)'
        $p = Start-Process -FilePath $exe -ArgumentList '-y', "-o$gitDir" -Wait -PassThru
        if ($p.ExitCode -ne 0 -or -not (Test-Path "$gitDir\cmd\git.exe")) { throw 'PortableGit did not unpack' }
    }
    Add-ToPath "$gitDir\cmd"
    Good 'git installed'
}

# ---------------------------------------------------------------- 2. GCC + make (MSYS2, ucrt64)
if ((Have gcc) -and (Have mingw32-make)) { Good "gcc already here: $((Get-Command gcc).Source)" }
else {
    $msys = Join-Path $root 'msys64'
    if (-not $force -and (Test-Path 'C:\msys64\usr\bin\bash.exe')) { $msys = 'C:\msys64' }
    $bash = Join-Path $msys 'usr\bin\bash.exe'
    if (-not (Test-Path $bash)) {
        $sfx = Join-Path $dl 'msys2-base-x86_64-latest.sfx.exe'
        Download 'https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-base-x86_64-latest.sfx.exe' $sfx
        Say 'MSYS2: unpacking (about 1 minute)'
        $p = Start-Process -FilePath $sfx -ArgumentList '-y', "-o$root\" -Wait -PassThru
        if ($p.ExitCode -ne 0 -or -not (Test-Path $bash)) { throw 'MSYS2 did not unpack' }
    }
    $env:MSYSTEM = 'UCRT64'
    $env:CHERE_INVOKING = '1'
    Say 'MSYS2: first start (creates its settings)'
    Native { & $bash -lc ' ' 2>&1 | Out-Null }
    if (-not (Test-Path "$msys\ucrt64\bin\gcc.exe") -or -not (Test-Path "$msys\ucrt64\bin\mingw32-make.exe")) {
        Say 'MSYS2: installing gcc and make with pacman (downloads about 80 MB, 2-5 minutes)'
        Native { & $bash -lc 'pacman -Sy --noconfirm --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make' 2>&1 |
            ForEach-Object { if ("$_" -match 'error|installing mingw-w64-ucrt-x86_64-(gcc|make)\b|Total') { Write-Host "       $_" } } }
        if (-not (Test-Path "$msys\ucrt64\bin\gcc.exe")) { throw 'pacman could not install gcc (network problem?) -- run the script again' }
    }
    # IMPORTANT: do not leave these in the student's window. The course Makefiles look at MSYSTEM to tell
    # "inside an MSYS2 shell" from "PowerShell / cmd"; a leftover value makes 'mingw32-make check' and 'clean' fail.
    Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
    Remove-Item Env:CHERE_INVOKING -ErrorAction SilentlyContinue
    # so that plain "make" also works, like on macOS / Linux
    if (-not (Test-Path "$msys\ucrt64\bin\make.exe")) { Copy-Item "$msys\ucrt64\bin\mingw32-make.exe" "$msys\ucrt64\bin\make.exe" }
    Add-ToPath "$msys\ucrt64\bin"
    Good 'gcc and make installed'
}

# ---------------------------------------------------------------- 3. Python 3 + numpy
$pyOk = $false
if (Have python) { try { Native { & python -c "import sys; assert sys.version_info >= (3, 8)" 2>&1 | Out-Null }; $pyOk = ($LASTEXITCODE -eq 0) } catch { $pyOk = $false } }
if ($pyOk) { Good "python already here: $((Get-Command python).Source)" }
else {
    $pyDir = Join-Path $root 'python'
    if (-not (Test-Path "$pyDir\tools\python.exe")) {
        $zip = Join-Path $dl 'python-nuget.zip'
        Download 'https://www.nuget.org/api/v2/package/python/3.13.7' $zip
        Say 'Python: unpacking'
        if (Test-Path $pyDir) { Remove-Item -Recurse -Force $pyDir }
        Expand-Archive -Path $zip -DestinationPath $pyDir -Force
        if (-not (Test-Path "$pyDir\tools\python.exe")) { throw 'Python did not unpack' }
    }
    Add-ToPath "$pyDir\tools\Scripts"
    Add-ToPath "$pyDir\tools"
    Good 'python installed'
}
$ErrorActionPreference = 'Continue'          # from here on only external programs run; their stderr chatter is not an error
try {
    & python -c "import numpy" 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Say 'Python: installing numpy (needed only by the MP2 reference program)'
        & python -m ensurepip --upgrade 2>&1 | Out-Null
        & python -m pip install --quiet --disable-pip-version-check --no-warn-script-location numpy 2>&1 | Out-Null
    }
    & python -c "import numpy" 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { Good 'numpy ready' } else { Bad 'numpy is not installed (only wavegen.py needs it; everything else works)' }
} catch { Bad "numpy step skipped: $($_.Exception.Message)" }

# ---------------------------------------------------------------- 4. course repo
$repo = $env:MMSP_REPO_DIR
if (-not $repo) { $repo = Join-Path ([Environment]::GetFolderPath('Desktop')) 'mmsp2026' }
$ErrorActionPreference = 'Continue'          # git writes progress to stderr; do not treat that as an error
if (Test-Path (Join-Path $repo '.git')) {
    Say "course repo: updating $repo"
    & git -C $repo pull --ff-only 2>&1 | Out-Null
} else {
    Say "course repo: cloning to $repo"
    & git clone --depth 1 https://github.com/cychiang-ntpu/mmsp2026.git $repo 2>&1 | Out-Null
}
if (-not (Test-Path (Join-Path $repo 'lectures'))) { Bad 'could not get the course repo (network?)' } else { Good "course repo ready: $repo" }

# ---------------------------------------------------------------- 5. check everything by really using it
Write-Host ''
Say 'checking...'
$fail = 0
foreach ($c in @(@('git', '--version'), @('gcc', '--version'), @('mingw32-make', '--version'), @('python', '--version'))) {
    try { $all = @(& $c[0] $c[1] 2>&1); if ($LASTEXITCODE -ne 0 -or $all.Count -eq 0) { throw 'x' }; Good ("{0,-13} {1}" -f $c[0], $all[0]) }
    catch { Bad "$($c[0]) does not run"; $fail++ }
}
$ex = Join-Path $repo 'lectures\wk03_0921_team1-kickoff\examples'
if (Test-Path $ex) {
    Push-Location $ex
    & mingw32-make 2>&1 | Out-Null
    $out = (& .\huffman_trace.exe 2>&1 | Out-String)
    Pop-Location
    if ($out -match '23') { Good 'built and ran this week''s C example (huffman_trace)' } else { Bad 'could not build / run huffman_trace'; $fail++ }
}
$mins = [math]::Round(((Get-Date) - $t0).TotalMinutes, 1)
Write-Host ''
if ($fail -eq 0) {
    Write-Host "ALL SET ($mins min).  Next:" -ForegroundColor Green
    Write-Host "    cd `"$ex`""
    Write-Host '    then follow commands.md (one folder up)'
    Write-Host 'VS Code or any NEW terminal window opened from now on will also find git / gcc / make / python.'
} else {
    Write-Host "$fail check(s) failed ($mins min). Run the same command again; if it still fails, show this window to the teacher / TA." -ForegroundColor Red
}
