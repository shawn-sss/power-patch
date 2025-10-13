$ErrorActionPreference = 'Continue'

function Test-HasFunc { param([Parameter(Mandatory)][string]$Name) Test-Path ("function:\{0}" -f $Name) }

if (-not (Test-HasFunc 'Should-ShowDetails')) {
    function Should-ShowDetails {
        return ($env:POWERPATCH_VERBOSE -match '^(1|true|yes|on)$')
    }
}
if (-not (Test-HasFunc 'Write-Info')) { function Write-Info { param([Parameter(Mandatory)][string]$Message) Write-Host  "[INFO ] $Message" } }
if (-not (Test-HasFunc 'Write-Detail')) { function Write-Detail { param([Parameter(Mandatory)][string]$Message) if (Should-ShowDetails) { Write-Host "[DETAIL] $Message" } } }
if (-not (Test-HasFunc 'Write-Warn')) { function Write-Warn { param([Parameter(Mandatory)][string]$Message) Write-Warning "[WARN ] $Message" } }
if (-not (Test-HasFunc 'Write-Success')) { function Write-Success { param([Parameter(Mandatory)][string]$Message) Write-Host  "[ OK  ] $Message" } }
if (-not (Test-HasFunc 'Write-ErrorUI')) { function Write-ErrorUI { param([Parameter(Mandatory)][string]$Message) Write-Error  "[ERROR] $Message" } }

function Run-Command {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$Arguments
    )
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $FilePath
        $psi.Arguments = ($Arguments -join ' ')
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true

        $p = New-Object System.Diagnostics.Process
        $p.StartInfo = $psi
        [void]$p.Start()

        $show = Should-ShowDetails
        while (-not $p.HasExited) {
            if ($show) {
                while (-not $p.StandardOutput.EndOfStream) { Write-Detail ($p.StandardOutput.ReadLine()) }
                while (-not $p.StandardError.EndOfStream) { Write-Detail ("stderr: " + $p.StandardError.ReadLine()) }
            }
            else {
                Start-Sleep -Milliseconds 120
            }
        }

        if ($show) {
            while (-not $p.StandardOutput.EndOfStream) { Write-Detail ($p.StandardOutput.ReadLine()) }
            while (-not $p.StandardError.EndOfStream) { Write-Detail ("stderr: " + $p.StandardError.ReadLine()) }
        }

        return $p.ExitCode
    }
    catch {
        Write-ErrorUI $_.Exception.Message
        return -1
    }
}

function Ensure-ServiceRunning {
    param([Parameter(Mandatory)][string]$Name)
    try {
        $svc = Get-Service -Name $Name -ErrorAction Stop
        if ($svc.Status -ne 'Running') {
            Write-Info "Starting service '$Name'…"
            Start-Service -Name $Name -ErrorAction Stop
            $svc.WaitForStatus('Running', '00:00:20')
        }
        return $true
    }
    catch {
        Write-ErrorUI "Service '$Name' failed to start: $($_.Exception.Message)"
        return $false
    }
}

function Is-PendingReboot {
    $paths = @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired',
        'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\PendingFileRenameOperations'
    )
    foreach ($p in $paths) {
        if (Test-Path $p) { return $true }
    }
    return $false
}

function Start-SafeRestart {
    try {
        Write-Info 'Initiating restart…'
        shutdown.exe /r /t 0 /d p:2:4
    }
    catch {
        Write-ErrorUI "Could not initiate restart: $($_.Exception.Message)"
    }
}

function Do-WindowsUpdate {
    param([switch]$IncludeDrivers)
    Write-Info 'Windows Update starting…'
    $reboot = $false
    try {
        $okWU = Ensure-ServiceRunning -Name 'wuauserv'
        $okBITS = Ensure-ServiceRunning -Name 'BITS'
        if (-not ($okWU -and $okBITS)) { throw 'Windows Update prerequisites not available.' }

        $session = New-Object -ComObject Microsoft.Update.Session
        $searcher = $session.CreateUpdateSearcher()
        $searcher.Online = $true

        $criteria = 'IsInstalled=0 and IsHidden=0'
        Write-Info 'Scanning for updates…'
        $searchResult = $searcher.Search($criteria)
        $count = [int]$searchResult.Updates.Count
        Write-Info ("Found {0} applicable update(s)." -f $count)
        if ($count -eq 0) { Write-Success 'No updates available'; return $false }

        $toDownload = New-Object -ComObject Microsoft.Update.UpdateColl
        $toInstall = New-Object -ComObject Microsoft.Update.UpdateColl

        for ($i = 0; $i -lt $count; $i++) {
            $u = $searchResult.Updates.Item($i)
            if (-not $IncludeDrivers.IsPresent -and $u.Type -eq 2) {
                Write-Detail ("Skipping driver: {0}" -f $u.Title)
                continue
            }
            if (-not $u.EulaAccepted) { [void]$u.AcceptEula() }
            $kb = ($u.KBArticleIDs -join ', ')
            if ([string]::IsNullOrWhiteSpace($kb)) { $kb = '-' }
            Write-Detail ("Update: {0} (KB: {1})" -f $u.Title, $kb)
            if (-not $u.IsDownloaded) { [void]$toDownload.Add($u) }
            [void]$toInstall.Add($u)
        }

        if ($toInstall.Count -eq 0) { Write-Success 'No applicable updates after filters'; return $false }

        if ($toDownload.Count -gt 0) {
            Write-Info 'Downloading updates…'
            $downloader = $session.CreateUpdateDownloader()
            $downloader.Updates = $toDownload
            $dResult = $downloader.Download()
            Write-Info ("Download result: {0}" -f $dResult.ResultCode)
        }
        else {
            Write-Info 'All updates already downloaded.'
        }

        Write-Info 'Installing updates…'
        $installer = $session.CreateUpdateInstaller()
        $installer.ForceQuiet = $true
        $installer.Updates = $toInstall
        $iResult = $installer.Install()
        Write-Info ("Installation result: {0} (Reboot required: {1})" -f $iResult.ResultCode, $iResult.RebootRequired)

        for ($j = 0; $j -lt $toInstall.Count; $j++) {
            $res = $iResult.GetUpdateResult($j)
            $ut = $toInstall.Item($j).Title
            Write-Detail ("{0}: {1}" -f $ut, $res.ResultCode)
        }

        if ($iResult.RebootRequired -or (Is-PendingReboot)) {
            Write-Warn 'Restart required'
            $reboot = $true
        }
        else {
            Write-Success 'Windows Update completed'
        }
    }
    catch {
        $hex = ('{0:X8}' -f ($_.Exception.HResult))
        Write-ErrorUI "Windows Update error: $($_.Exception.Message) (HRESULT 0x$hex)"
        Write-Detail 'Ensure policies/WSUS allow online updates and components are healthy.'
    }
    return $reboot
}

function Do-StoreUpdates {
    Write-Info 'Microsoft Store apps updating…'
    try {
        $winget = (Get-Command winget.exe -ErrorAction SilentlyContinue).Source
        if (-not $winget) {
            Write-Warn "winget not found. Install 'App Installer' from Microsoft Store."
            return $false
        }

        [void](Run-Command -FilePath $winget -Arguments @('source', 'update'))

        $sources = (& $winget source list) 2>&1 | Out-String
        if ($sources -notmatch 'msstore') {
            Write-Info "Adding 'msstore' source…"
            [void](Run-Command -FilePath $winget -Arguments @('source', 'add', '-n', 'msstore', '-a', 'https://storeedgefd.dsx.mp.microsoft.com/v9.0'))
        }

        Write-Info 'Upgrading user-scope apps from Microsoft Store…'
        $codeUser = Run-Command -FilePath $winget -Arguments @('upgrade', '--source', 'msstore', '--all', '--scope', 'user', '--accept-package-agreements', '--accept-source-agreements')
        if ($codeUser -eq 0) {
            Write-Success 'User-scope app updates complete'
        }
        else {
            Write-Warn "winget user-scope exit code $codeUser. Some apps may not have updated."
        }

        Write-Info 'Upgrading machine-scope apps from Microsoft Store…'
        $codeMachine = Run-Command -FilePath $winget -Arguments @('upgrade', '--source', 'msstore', '--all', '--scope', 'machine', '--accept-package-agreements', '--accept-source-agreements')
        if ($codeMachine -eq 0) {
            Write-Success 'Machine-scope app updates complete'
        }
        else {
            Write-Warn "winget machine-scope exit code $codeMachine. Some apps may not have updated."
        }
    }
    catch {
        Write-ErrorUI "Store update error: $($_.Exception.Message)"
    }
    return $false
}

function Do-OfficeUpdate {
    Write-Info 'Microsoft 365 update check…'
    $reboot = $false
    try {
        $c2r = Join-Path ${env:ProgramFiles} 'Common Files\microsoft shared\ClickToRun\OfficeC2RClient.exe'
        if (-not (Test-Path $c2r)) {
            $c2r = Join-Path ${env:ProgramFiles(x86)} 'Common Files\microsoft shared\ClickToRun\OfficeC2RClient.exe'
        }

        if (Test-Path $c2r) {
            Write-Info 'Invoking Office updater…'
            $code = Run-Command -FilePath $c2r -Arguments @('/update', 'user', 'displaylevel=false', 'forceappshutdown=true')
            if ($code -eq 0) {
                Write-Success 'Office updater invoked'
            }
            else {
                Write-Warn "Office updater exit code $code. Updates may not have installed."
            }

            if (Is-PendingReboot) {
                Write-Warn 'Restart may be required (Office)'
                $reboot = $true
            }
        }
        else {
            Write-Warn 'Office Click-to-Run not found — skipping.'
        }
    }
    catch {
        Write-ErrorUI "Office update error: $($_.Exception.Message)"
    }
    return $reboot
}
