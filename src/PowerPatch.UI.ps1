$ErrorActionPreference = 'Continue'

$AppName = 'Power Patch'
$AppDesc = 'Update Windows OS, Microsoft Store apps, and Microsoft 365'
$RelaunchText = "This tool needs Administrator rights and STA mode.`nSelect OK to relaunch and approve UAC."
$ErrRelaunchT  = "$AppName — Relaunch error"
$ErrRunT       = "$AppName — Run error"
$InfoCopiedT   = "$AppName — Copied"
$ErrCopyT      = "$AppName — Copy failed"
$ErrUiT        = "$AppName — UI load error"
$RestartReqT   = "$AppName — Restart required"

Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase

function Is-Admin { ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) }
$needElevate = -not (Is-Admin)
$IsSTA = [Threading.Thread]::CurrentThread.ApartmentState -eq 'STA'
if ($needElevate -or -not $IsSTA) {
    [System.Windows.MessageBox]::Show($RelaunchText, $AppName, 'OK', 'Information') | Out-Null
    $ps = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $arg = @('-NoProfile','-ExecutionPolicy','Bypass','-STA','-WindowStyle','Hidden','-File',"`"$PSCommandPath`"")
    try { Start-Process -FilePath $ps -ArgumentList $arg -Verb RunAs -WindowStyle Hidden | Out-Null } catch { [System.Windows.MessageBox]::Show("$_",$ErrRelaunchT,'OK','Error') | Out-Null }
    return
}

function Test-Env {
    $minBuildOk = ($PSVersionTable.PSEdition -eq 'Desktop') -and ([Environment]::OSVersion.Version.Major -ge 10)
    $minBuildOk = $minBuildOk -and (([Environment]::OSVersion.Version.Major -gt 10) -or ([Environment]::OSVersion.Version.Build -ge 17763))
    $psOk = ($PSVersionTable.PSVersion.Major -gt 5) -or ($PSVersionTable.PSVersion.Major -eq 5 -and $PSVersionTable.PSVersion.Minor -ge 1)
    $wuaOk = $false
    try { $null = New-Object -ComObject Microsoft.Update.Session -ErrorAction Stop; $wuaOk = $true } catch { $wuaOk = $false }
    $winget = (Get-Command winget.exe -ErrorAction SilentlyContinue)
    $wingetOk = [bool]$winget
    $c2r = Join-Path ${env:ProgramFiles} 'Common Files\microsoft shared\ClickToRun\OfficeC2RClient.exe'
    $c2rOk = Test-Path $c2r
    if (-not $c2rOk) {
        $c2r = Join-Path ${env:ProgramFiles(x86)} 'Common Files\microsoft shared\ClickToRun\OfficeC2RClient.exe'
        $c2rOk = Test-Path $c2r
    }
    [pscustomobject]@{ OSOk=$minBuildOk; PSOk=$psOk; WUAOk=$wuaOk; WingetOk=$wingetOk; OfficeC2ROk=$c2rOk }
}

function Get-IsDarkMode { try { ((Get-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' -Name AppsUseLightTheme -ErrorAction Stop).AppsUseLightTheme -eq 0) } catch { $false } }

function Set-ThemeVars {
    param([bool]$Dark)

    $script:IsDark = $Dark

    if ($Dark) {
        $script:accentHex      = '#FF60A5FA'
        $script:accentHexHover = '#FF93C5FD'
        $script:bgHex          = '#FF020617'
        $script:fgHex          = '#FFF1F5F9'
        $script:panelHex       = '#FF0F172A'
        $script:borderHex      = '#FF1F2937'
        $script:muteHex        = '#FF94A3B8'
        $script:btnHex         = '#FF1F2937'
        $script:btnHexHover    = '#FF334155'
        $script:btnBorderHover = '#FF475569'
        $script:focusRing      = '#FF60A5FA'
        $script:infoHex        = '#FFE5E7EB'
        $script:detailHex      = '#FF94A3B8'
        $script:headerHex      = $fgHex
    } else {
        $script:accentHex      = '#FF2563EB'
        $script:accentHexHover = '#FF3B82F6'
        $script:bgHex          = '#FFF8FAFC'
        $script:fgHex          = '#FF0F172A'
        $script:panelHex       = '#FFFFFFFF'
        $script:borderHex      = '#FFE2E8F0'
        $script:muteHex        = '#FF64748B'
        $script:btnHex         = '#FFF1F5F9'
        $script:btnHexHover    = '#FFE2E8F0'
        $script:btnBorderHover = '#FFCBD5E1'
        $script:focusRing      = '#FF93C5FD'
        $script:infoHex        = '#FF1F2937'
        $script:detailHex      = '#FF475569'
        $script:headerHex      = $fgHex
    }
}
Set-ThemeVars -Dark (Get-IsDarkMode)

$cap = Test-Env
if (-not $cap.OSOk -or -not $cap.PSOk) {
    $msg = @()
    if (-not $cap.OSOk) { $msg += 'Supported on Windows 10/11 (build 17763+) only.' }
    if (-not $cap.PSOk) { $msg += 'Requires Windows PowerShell 5.1 or newer (Desktop edition).' }
    [System.Windows.MessageBox]::Show(($msg -join "`n"), $AppName, 'OK', 'Error') | Out-Null
    return
}

function Get-ExistingAppProcesses {
    try {
        $me = $PID
        $path = [System.IO.Path]::GetFullPath($PSCommandPath)
        $q = Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            ($_.Name -match '^(powershell|pwsh)\.exe$') -and $_.ProcessId -ne $me -and (
                ($_.CommandLine -match [Regex]::Escape($path)) -or
                ((Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue).MainWindowTitle -eq $AppName)
            )
        }
        return $q
    } catch { @() }
}
function Close-ExistingInstances {
    $others = Get-ExistingAppProcesses
    foreach ($proc in $others) {
        try {
            $p = Get-Process -Id $proc.ProcessId -ErrorAction Stop
            if ($p.CloseMainWindow()) { $p.WaitForExit(2000) | Out-Null }
            if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction Stop }
        } catch { }
    }
}
Close-ExistingInstances

$Xaml = @"
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 Title="$AppName"
 Width="900" Height="740"
 MinWidth="900" MinHeight="740"
 MaxWidth="900" MaxHeight="740"
 ResizeMode="CanMinimize"
 SizeToContent="Manual"
 WindowStartupLocation="CenterScreen"
 Background="$bgHex"
 FontFamily="Segoe UI" FontSize="13" ShowInTaskbar="True" AutomationProperties.Name="$AppName">
 <Window.Icon><DrawingImage/></Window.Icon>
 <Window.Resources>
   <Style x:Key="BaseButton" TargetType="Button">
     <Setter Property="Padding" Value="12,10"/>
     <Setter Property="Margin" Value="0,0,10,10"/>
     <Setter Property="MinWidth" Value="150"/>
     <Setter Property="BorderThickness" Value="1"/>
     <Setter Property="Cursor" Value="Hand"/>
     <Setter Property="SnapsToDevicePixels" Value="True"/>
     <Setter Property="Background" Value="$btnHex"/>
     <Setter Property="BorderBrush" Value="$borderHex"/>
     <Setter Property="Foreground" Value="$fgHex"/>
     <Setter Property="Template">
       <Setter.Value>
         <ControlTemplate TargetType="Button">
           <Border x:Name="root" Background="{TemplateBinding Background}" BorderBrush="{TemplateBinding BorderBrush}"
             BorderThickness="{TemplateBinding BorderThickness}" CornerRadius="10">
             <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center" RecognizesAccessKey="True"/>
           </Border>
           <ControlTemplate.Triggers>
             <Trigger Property="IsMouseOver" Value="True">
               <Setter TargetName="root" Property="Background" Value="$btnHexHover"/>
               <Setter TargetName="root" Property="BorderBrush" Value="$btnBorderHover"/>
             </Trigger>
             <Trigger Property="IsEnabled" Value="False">
               <Setter TargetName="root" Property="Opacity" Value="0.55"/>
             </Trigger>
             <Trigger Property="IsKeyboardFocused" Value="True">
               <Setter TargetName="root" Property="BorderBrush" Value="$focusRing"/>
               <Setter TargetName="root" Property="BorderThickness" Value="2"/>
             </Trigger>
           </ControlTemplate.Triggers>
         </ControlTemplate>
       </Setter.Value>
     </Setter>
   </Style>
   <Style x:Key="SecondaryButton" TargetType="Button" BasedOn="{StaticResource BaseButton}"/>
   <Style TargetType="CheckBox"><Setter Property="Foreground" Value="$fgHex"/><Setter Property="Margin" Value="0,0,0,8"/></Style>
   <Style TargetType="TextBlock"><Setter Property="Foreground" Value="$fgHex"/></Style>
   <Style TargetType="ProgressBar"><Setter Property="Height" Value="12"/><Setter Property="Foreground" Value="$accentHex"/>
     <Setter Property="Background" Value="$borderHex"/><Setter Property="Margin" Value="0,0,0,8"/></Style>
   <SolidColorBrush x:Key="CardBorder" Color="$borderHex"/>
   <SolidColorBrush x:Key="CardBackground" Color="$panelHex"/>
   <SolidColorBrush x:Key="Muted" Color="$muteHex"/>
 </Window.Resources>
 <Grid Margin="16" KeyboardNavigation.TabNavigation="Cycle">
   <Grid.RowDefinitions>
     <RowDefinition Height="Auto"/>
     <RowDefinition Height="Auto"/>
     <RowDefinition Height="Auto"/>
     <RowDefinition Height="*"/>
   </Grid.RowDefinitions>

   <StackPanel Orientation="Horizontal" Grid.Row="0" Margin="0,0,0,8">
     <TextBlock Text="$AppName" FontSize="20" FontWeight="SemiBold" Margin="0,0,12,0"/>
     <TextBlock Text="($AppDesc)" Foreground="{StaticResource Muted}" VerticalAlignment="Bottom"/>
   </StackPanel>

   <Border Grid.Row="1" Background="{StaticResource CardBackground}" BorderBrush="{StaticResource CardBorder}" BorderThickness="1"
     CornerRadius="12" Padding="14" Margin="0,0,0,12">
     <Grid>
       <Grid.ColumnDefinitions>
         <ColumnDefinition Width="*"/>
         <ColumnDefinition Width="*"/>
       </Grid.ColumnDefinitions>
       <StackPanel Grid.Column="0">
         <CheckBox x:Name="ChkWin" IsChecked="True" Content="_Windows OS (native WUA API)"/>
         <CheckBox x:Name="ChkStore" IsChecked="True" Content="Microsoft _Store (winget)"/>
         <CheckBox x:Name="ChkOffice" IsChecked="True" Content="Microsoft _365 (Click-to-Run)"/>
       </StackPanel>
       <StackPanel Grid.Column="1" Margin="24,0,0,0">
         <CheckBox x:Name="ChkDrivers" IsChecked="True" Content="Include _driver updates (Windows Update)"/>
         <CheckBox x:Name="ChkDetails" IsChecked="False" Content="Show _details (live command output)"/>
         <CheckBox x:Name="ChkAutoReboot" IsChecked="False" Content="_Restart automatically if required"/>
       </StackPanel>
     </Grid>
   </Border>

   <Border Grid.Row="2" Background="{StaticResource CardBackground}" BorderBrush="{StaticResource CardBorder}" BorderThickness="1"
     CornerRadius="12" Padding="12" Margin="0,0,0,12">
     <Grid>
       <Grid.ColumnDefinitions>
         <ColumnDefinition Width="*"/>
         <ColumnDefinition Width="320"/>
       </Grid.ColumnDefinitions>
       <Grid Grid.Column="0" Margin="0,0,12,0">
         <Grid.RowDefinitions>
           <RowDefinition Height="Auto"/>
           <RowDefinition Height="Auto"/>
         </Grid.RowDefinitions>
         <Grid.ColumnDefinitions>
           <ColumnDefinition Width="Auto"/>
           <ColumnDefinition Width="Auto"/>
         </Grid.ColumnDefinitions>
         <Button x:Name="BtnRun" Grid.Row="0" Grid.Column="0" Content="_Run Selected" Style="{StaticResource SecondaryButton}"/>
         <Button x:Name="BtnAll" Grid.Row="0" Grid.Column="1" Content="Run _All" Style="{StaticResource SecondaryButton}"/>
         <Button x:Name="BtnCopy" Grid.Row="1" Grid.Column="0" Content="_Copy Output" Style="{StaticResource SecondaryButton}"/>
         <Button x:Name="BtnClear" Grid.Row="1" Grid.Column="1" Content="C_lear Output" Style="{StaticResource SecondaryButton}"/>
       </Grid>
       <StackPanel Grid.Column="1" Orientation="Vertical" VerticalAlignment="Center">
         <ProgressBar x:Name="Prog" Height="12" Minimum="0" Maximum="100" Value="0"/>
         <TextBlock x:Name="LblStatus" Text="Ready" Foreground="{StaticResource Muted}" Margin="2,6,0,0"/>
       </StackPanel>
     </Grid>
   </Border>

   <Border Grid.Row="3" Background="{StaticResource CardBackground}" BorderBrush="{StaticResource CardBorder}" BorderThickness="1"
     CornerRadius="12" Padding="12">
     <DockPanel LastChildFill="True">
       <RichTextBox x:Name="OutBox" IsReadOnly="True" IsDocumentEnabled="True" BorderThickness="0"
         Background="{StaticResource CardBackground}" ScrollViewer.VerticalScrollBarVisibility="Auto"
         ScrollViewer.HorizontalScrollBarVisibility="Disabled">
         <FlowDocument x:Name="OutDoc"/>
       </RichTextBox>
     </DockPanel>
   </Border>
 </Grid>
</Window>
"@


try {
    $stringReader = New-Object System.IO.StringReader($Xaml)
    $xmlReader = [System.Xml.XmlReader]::Create($stringReader)
    $window = [Windows.Markup.XamlReader]::Load($xmlReader)
} catch { [System.Windows.MessageBox]::Show("$_",$ErrUiT,'OK','Error') | Out-Null; return }

try {
    $iconPathIco = Join-Path $PSScriptRoot 'assets\powerpatch.ico'
    $iconPathPng = Join-Path $PSScriptRoot 'assets\powerpatch.png'
    if (Test-Path $iconPathIco) {
        $fs = [System.IO.File]::OpenRead($iconPathIco)
        try { $decoder = [System.Windows.Media.Imaging.IconBitmapDecoder]::new($fs,[System.Windows.Media.Imaging.BitmapCreateOptions]::None,[System.Windows.Media.Imaging.BitmapCacheOption]::OnLoad); $img = $decoder.Frames[0] } finally { $fs.Dispose() }
    } elseif (Test-Path $iconPathPng) {
        $fs = [System.IO.File]::OpenRead($iconPathPng)
        try { $img = [System.Windows.Media.Imaging.PngBitmapDecoder]::new($fs,[System.Windows.Media.Imaging.BitmapCreateOptions]::None,[System.Windows.Media.Imaging.BitmapCacheOption]::OnLoad).Frames[0] } finally { $fs.Dispose() }
    }
    if ($img) {
        $window.Icon = $img
        if (-not $window.TaskbarItemInfo) { $window.TaskbarItemInfo = New-Object System.Windows.Shell.TaskbarItemInfo }
        $window.TaskbarItemInfo.Overlay = $img
        $window.TaskbarItemInfo.Description = $AppName
    }
} catch { }

$ChkWin        = $window.FindName('ChkWin')
$ChkStore      = $window.FindName('ChkStore')
$ChkOffice     = $window.FindName('ChkOffice')
$ChkDrivers    = $window.FindName('ChkDrivers')
$ChkDetails    = $window.FindName('ChkDetails')
$ChkAutoReboot = $window.FindName('ChkAutoReboot')
$BtnRun        = $window.FindName('BtnRun')
$BtnAll        = $window.FindName('BtnAll')
$BtnClear      = $window.FindName('BtnClear')
$BtnCopy       = $window.FindName('BtnCopy')
$Prog          = $window.FindName('Prog')
$LblStatus     = $window.FindName('LblStatus')
$OutBox        = $window.FindName('OutBox')
$OutDoc        = $OutBox.Document

if (-not $cap.WUAOk)       { $ChkWin.IsChecked=$false;   $ChkWin.IsEnabled=$false;   $ChkWin.ToolTip   ="Windows Update Agent unavailable" }
if (-not $cap.WingetOk)    { $ChkStore.IsChecked=$false; $ChkStore.IsEnabled=$false; $ChkStore.ToolTip ="Requires App Installer (winget)" }
if (-not $cap.OfficeC2ROk) { $ChkOffice.IsChecked=$false;$ChkOffice.IsEnabled=$false;$ChkOffice.ToolTip="Office Click-to-Run not detected" }

if (-not $cap.WUAOk -or -not $cap.WingetOk -or -not $cap.OfficeC2ROk) {
    $warn = @()
    if (-not $cap.WUAOk)       { $warn += 'Windows OS updates are disabled (WUA missing/unavailable).' }
    if (-not $cap.WingetOk)    { $warn += 'Microsoft Store updates are disabled (winget missing).' }
    if (-not $cap.OfficeC2ROk) { $warn += 'Microsoft 365 updates are disabled (Click-to-Run not found).' }
    [System.Windows.MessageBox]::Show(($warn -join "`n"), $AppName, 'OK', 'Information') | Out-Null
}

function Flush-UI {
    try {
        $frame = New-Object System.Windows.Threading.DispatcherFrame
        $null = $window.Dispatcher.BeginInvoke([System.Windows.Threading.DispatcherPriority]::Background,[System.Windows.Threading.DispatcherOperationCallback]{ param($f) $f.Continue = $false; $null },$frame)
        [System.Windows.Threading.Dispatcher]::PushFrame($frame)
    } catch { }
}

function New-Paragraph {
    param([string]$text,[string]$colorHex="#FFFFFFFF",[string]$weight="Normal",[int]$size=12,[switch]$Mono)
    $para = New-Object System.Windows.Documents.Paragraph
    $run  = New-Object System.Windows.Documents.Run
    $ts = (Get-Date).ToString("HH:mm:ss")
    $run.Text = "[$ts] $text"
    $a = [Convert]::ToByte($colorHex.Substring(1,2),16)
    $r = [Convert]::ToByte($colorHex.Substring(3,2),16)
    $g = [Convert]::ToByte($colorHex.Substring(5,2),16)
    $b = [Convert]::ToByte($colorHex.Substring(7,2),16)
    $run.Foreground = New-Object System.Windows.Media.SolidColorBrush ([System.Windows.Media.Color]::FromArgb($a,$r,$g,$b))
    $run.FontWeight = $weight
    $run.FontSize   = $size
    if ($Mono) { $run.FontFamily = 'Consolas' }
    [void]$para.Inlines.Add($run)
    return $para
}

function Add-Separator {
    $sep = New-Object System.Windows.Documents.Paragraph
    $rule = New-Object System.Windows.Documents.Run
    $rule.Text = '────────────────────────────────────────────────────────────────'
    $rule.Foreground = New-Object System.Windows.Media.SolidColorBrush ([System.Windows.Media.Colors]::DimGray)
    $rule.FontSize = 11
    [void]$sep.Inlines.Add($rule)
    [void]$OutDoc.Blocks.Add($sep)
    Flush-UI
}

function Write-Section($title){
    Add-Separator
    [void]$OutDoc.Blocks.Add((New-Paragraph -text ("— " + $title.ToUpper() + " —") -colorHex $headerHex -weight 'Bold' -size 14))
    Flush-UI
}

function Write-Header($t){ Write-Section $t }

function Write-Info($t) {
    [void]$OutDoc.Blocks.Add((New-Paragraph -text $t -colorHex $infoHex -size 12))
    $OutBox.ScrollToEnd()
    Flush-UI
}

function Write-Detail($t){
    if ($ChkDetails.IsChecked) {
        [void]$OutDoc.Blocks.Add((New-Paragraph -text " • $t" -colorHex $detailHex -size 12 -Mono))
        $OutBox.ScrollToEnd()
        Flush-UI
    }
}

function Write-Success($t){
    [void]$OutDoc.Blocks.Add((New-Paragraph -text ("✅ " + $t) -colorHex '#FF34D399' -weight 'Bold' -size 13))
    $OutBox.ScrollToEnd()
    Flush-UI
}

function Write-Warn($t){
    [void]$OutDoc.Blocks.Add((New-Paragraph -text ("⚠️ " + $t) -colorHex '#FFF59E0B' -weight 'Bold' -size 13))
    $OutBox.ScrollToEnd()
    Flush-UI
}

function Write-ErrorUI($t){
    [void]$OutDoc.Blocks.Add((New-Paragraph -text ("❌ " + $t) -colorHex '#FFEF4444' -weight 'Bold' -size 13))
    $OutBox.ScrollToEnd()
    Flush-UI
}

function Set-ButtonsEnabled([bool]$enabled){
    foreach($b in @($BtnRun,$BtnAll,$BtnClear,$BtnCopy,$ChkWin,$ChkStore,$ChkOffice,$ChkDrivers,$ChkDetails,$ChkAutoReboot)){
        if ($b) { $b.IsEnabled = $enabled }
    }
}

function Set-Progress([int]$percent,[string]$status=$null){
    if ($percent -lt 0 -or $percent -gt 100) { $percent = [Math]::Max(0,[Math]::Min(100,$percent)) }
    $Prog.IsIndeterminate = $false
    $Prog.Value = $percent
    if ($status) { $LblStatus.Text = $status }
    Flush-UI
}

function Should-ShowDetails { [bool]$ChkDetails.IsChecked }

function Sync-DriverOption {
    if ($ChkWin.IsChecked) {
        $ChkDrivers.IsEnabled = $true
        $ChkDrivers.ToolTip = $null
    } else {
        $ChkDrivers.IsChecked = $false
        $ChkDrivers.IsEnabled = $false
        $ChkDrivers.ToolTip = "Enable Windows OS updates to include driver updates."
    }
}

function Sync-RunSelectedEnabled { $BtnRun.IsEnabled = [bool]($ChkWin.IsChecked -or $ChkStore.IsChecked -or $ChkOffice.IsChecked) }

$ChkWin.Add_Checked({    Sync-DriverOption; Sync-RunSelectedEnabled })
$ChkWin.Add_Unchecked({  Sync-DriverOption; Sync-RunSelectedEnabled })
$ChkStore.Add_Checked({   Sync-RunSelectedEnabled })
$ChkStore.Add_Unchecked({ Sync-RunSelectedEnabled })
$ChkOffice.Add_Checked({   Sync-RunSelectedEnabled })
$ChkOffice.Add_Unchecked({ Sync-RunSelectedEnabled })

Sync-DriverOption
Sync-RunSelectedEnabled

$logicPath = Join-Path $PSScriptRoot 'PowerPatch.Core.ps1'
. $logicPath

$BtnRun.Add_Click({
    try {
        Set-ButtonsEnabled $false
        $OutDoc.Blocks.Clear() | Out-Null
        Write-Header $AppName
        Write-Info ("Selected Tasks: Windows={0}, Store={1}, Office={2}, Drivers={3}, Details={4}, AutoReboot={5}" -f $ChkWin.IsChecked, $ChkStore.IsChecked, $ChkOffice.IsChecked, $ChkDrivers.IsChecked, $ChkDetails.IsChecked, $ChkAutoReboot.IsChecked)
        Set-Progress 1 'Starting…'
        Start-Sleep -Seconds 3
        $winEnd = 34; $storeEnd = 67; $officeEnd = 100
        if ($ChkWin.IsChecked)   { Set-Progress 1 'Windows OS starting…';       Write-Header 'Windows OS';       $rebootNeeded = (Do-WindowsUpdate -IncludeDrivers:$($ChkDrivers.IsChecked)); Set-Progress $winEnd   'Windows OS completed' } else { $rebootNeeded = $false; Set-Progress $winEnd   'Windows OS skipped' }
        if ($ChkStore.IsChecked) { Set-Progress $winEnd 'Microsoft Store starting…'; Write-Header 'Microsoft Store apps'; Do-StoreUpdates | Out-Null;                                         Set-Progress $storeEnd 'Microsoft Store completed' } else { Set-Progress $storeEnd 'Microsoft Store skipped' }
        if ($ChkOffice.IsChecked){ Set-Progress $storeEnd 'Microsoft 365 starting…'; Write-Header 'Microsoft 365'; $rebootNeeded = (Do-OfficeUpdate) -or $rebootNeeded;                      Set-Progress $officeEnd 'Microsoft 365 completed' } else { Set-Progress $officeEnd 'Microsoft 365 skipped' }
        Write-Header 'All tasks completed'
        if ($rebootNeeded) {
            if ($ChkAutoReboot.IsChecked) {
                $resp = [System.Windows.MessageBox]::Show('A restart is required. Restart now?',$RestartReqT,'YesNo','Question')
                if ($resp -eq 'Yes') { Start-SafeRestart } else { Write-Warn 'Restart required; you chose later.' }
            } else { Write-Warn 'A restart is required to complete updates.' }
        }
        Write-Success 'Done.'
        Set-Progress $officeEnd 'All tasks completed'
        $timer = New-Object System.Windows.Threading.DispatcherTimer
        $timer.Interval = [TimeSpan]::FromSeconds(3)
        $null = $timer.Add_Tick({ param($s,$e) Set-Progress 0 'Ready'; $s.Stop() })
        $timer.Start()
    } catch {
        Write-ErrorUI "Run error: $($_.Exception.Message)"
        [System.Windows.MessageBox]::Show("A run error occurred.`n$($_.Exception.Message)",$ErrRunT,'OK','Error') | Out-Null
    }
    finally { Set-ButtonsEnabled $true }
})

$BtnAll.Add_Click({
    if ($ChkWin.IsEnabled)    { $ChkWin.IsChecked    = $true }
    if ($ChkStore.IsEnabled)  { $ChkStore.IsChecked  = $true }
    if ($ChkOffice.IsEnabled) { $ChkOffice.IsChecked = $true }
    if ($ChkDrivers.IsEnabled){ $ChkDrivers.IsChecked = $true }
    Sync-DriverOption
    Sync-RunSelectedEnabled
    $BtnRun.RaiseEvent((New-Object System.Windows.RoutedEventArgs([System.Windows.Controls.Button]::ClickEvent)))
})

$BtnClear.Add_Click({
    $OutDoc.Blocks.Clear() | Out-Null
    Set-Progress 0 'Ready'
    Write-Header 'Ready'
    Write-Info 'Choose tasks and select Run Selected or Run All. Toggle Show details for verbose output.'
})
$BtnCopy.Add_Click({
    try {
        $range = New-Object System.Windows.Documents.TextRange($OutDoc.ContentStart,$OutDoc.ContentEnd)
        [System.Windows.Clipboard]::SetText($range.Text)
        [System.Windows.MessageBox]::Show('Output copied.',$InfoCopiedT,'OK','Information') | Out-Null
    } catch {
        [System.Windows.MessageBox]::Show("Copy failed: $($_.Exception.Message)",$ErrCopyT,'OK','Error') | Out-Null
    }
})

Write-Header 'Ready'
Write-Info 'Choose tasks and select Run Selected or Run All. Toggle Show details for verbose output.'
$null = $window.ShowDialog()
