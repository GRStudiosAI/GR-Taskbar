# Original signed build orchestration: GR Studios AI LLC, 2026.
[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [ValidateSet('amd64','arm64')][string]$Platform = 'amd64',
    [ValidateSet('SelfSigned','Trusted')][string]$SigningMode = 'SelfSigned',
    [string]$CertificateThumbprint = $env:GR_SIGN_CERT_THUMBPRINT,
    [string]$PublicCertificatePath,
    [string]$WorkRoot,
    [string]$VisualStudioPath,
    [string]$SignToolPath,
    [string]$TimestampUrl = 'http://timestamp.digicert.com',
    [switch]$SkipCompile
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $WorkRoot) { $WorkRoot = Join-Path $root 'release-work' }
$WorkRoot = Join-Path $WorkRoot $Platform
$WorkRoot = [IO.Path]::GetFullPath($WorkRoot)
if (-not $PublicCertificatePath) {
    $PublicCertificatePath = Join-Path $root 'scripts\signing\GR-Taskbar-Public.cer'
}
if (-not $CertificateThumbprint) {
    if (-not (Test-Path -LiteralPath $PublicCertificatePath)) { throw 'Provide a public certificate path or GR_SIGN_CERT_THUMBPRINT. Never put a PFX/password in the repository.' }
    $public = New-Object Security.Cryptography.X509Certificates.X509Certificate2($PublicCertificatePath)
    $CertificateThumbprint = $public.Thumbprint
}
$CertificateThumbprint = $CertificateThumbprint.Replace(' ','').ToUpperInvariant()
if ($CertificateThumbprint -notmatch '^[A-F0-9]{40}$') { throw 'Invalid certificate thumbprint.' }
$certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint"
if (-not $certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)) { throw 'The signing certificate must be current and have an accessible private key.' }
if (@($certificate.EnhancedKeyUsageList | Where-Object { $_.ObjectId -eq '1.3.6.1.5.5.7.3.3' }).Count -eq 0) { throw 'The certificate is not enabled for code signing.' }
if ($SigningMode -eq 'Trusted' -and $certificate.Subject -eq $certificate.Issuer) { throw 'Trusted mode requires a trusted certificate chain. Use SelfSigned mode for your self-issued certificate.' }

New-Item -ItemType Directory -Path $WorkRoot,(Join-Path $WorkRoot 'logs'),(Join-Path $WorkRoot 'outputs') -Force | Out-Null
if (-not $VisualStudioPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) { $VisualStudioPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
}
if (-not $VisualStudioPath) { throw 'Visual Studio C++ tools were not found; pass -VisualStudioPath.' }
$msbuild = Join-Path $VisualStudioPath 'MSBuild\Current\Bin\MSBuild.exe'
$vcvars = Join-Path $VisualStudioPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $msbuild) -or -not (Test-Path -LiteralPath $vcvars)) { throw 'Missing Visual Studio build tools.' }

if (-not $SignToolPath) {
    # Pinned, official Microsoft SDK tools; private keys are never downloaded/copied.
    $sdkVersion = '10.0.26100.9457'
    $sdkRoot = Join-Path $WorkRoot "tools\WindowsSDK\$sdkVersion"
    $SignToolPath = Join-Path $sdkRoot 'bin\10.0.26100.0\x64\signtool.exe'
    if (-not (Test-Path -LiteralPath $SignToolPath)) {
        New-Item -ItemType Directory -Path (Split-Path $sdkRoot -Parent) -Force | Out-Null
        $package = Join-Path (Split-Path $sdkRoot -Parent) "Microsoft.Windows.SDK.BuildTools.$sdkVersion.nupkg"
        Invoke-WebRequest -Uri "https://api.nuget.org/v3-flatcontainer/microsoft.windows.sdk.buildtools/$sdkVersion/microsoft.windows.sdk.buildtools.$sdkVersion.nupkg" -OutFile $package -UseBasicParsing
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [IO.Compression.ZipFile]::ExtractToDirectory($package,$sdkRoot)
    }
}
if (-not (Test-Path -LiteralPath $SignToolPath)) { throw 'SignTool was not found.' }
. (Join-Path $PSScriptRoot 'ReleaseVerification.ps1')

function Invoke-GRBuild {
    param([string[]]$Commands, [string]$Label)
    $batch = Join-Path $WorkRoot "logs\build-$Label.cmd"
    $lines = @('@echo off','setlocal DisableDelayedExpansion',('call "{0}" x64 >nul' -f $vcvars),'if errorlevel 1 exit /b 1',('cd /d "{0}"' -f $root))
    foreach ($command in $Commands) { $lines += $command; $lines += 'if errorlevel 1 exit /b 1' }
    [IO.File]::WriteAllLines($batch,$lines,[Text.Encoding]::Default)
    & $env:ComSpec /d /c $batch
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $Label (exit $LASTEXITCODE)." }
}

$arch = if ($Platform -eq 'amd64') { 'x64' } else { 'ARM64' }
$output = Join-Path $root "build\$Configuration\$arch"
$payload = Join-Path $output 'gr_setup_files'
if (-not $SkipCompile) {
    # Check the absolute resolved cache location before removing generated staging.
    if (Test-Path -LiteralPath $payload) {
        $resolved = (Resolve-Path -LiteralPath $payload).ProviderPath
        $expected = [IO.Path]::GetFullPath($payload)
        if ($resolved -ne $expected -or -not $resolved.StartsWith($root + '\',[StringComparison]::OrdinalIgnoreCase) -or
            (Get-Item -LiteralPath $payload).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Unsafe payload staging location.' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
    Invoke-GRBuild -Label compile -Commands @(
        'nuget restore GR-Taskbar.sln',
        'nuget restore gr_starttiledata\gr_starttiledata.slnx',
        ('call BuildDependencies{0}.bat' -f $Configuration),
        ('"{0}" GR-Taskbar.sln /m /p:Configuration={1} /p:Platform=IA-32 /v:minimal' -f $msbuild,$Configuration),
        ('"{0}" gr_starttiledata\gr_starttiledata.vcxproj /m /p:Configuration={1} /p:Platform={2} "/p:SolutionDir={3}/" /v:minimal' -f $msbuild,$Configuration,$arch,$root.Replace('\','/')),
        ('"{0}" GR-Taskbar.sln /m /p:Configuration={1} /p:Platform={2} /v:minimal' -f $msbuild,$Configuration,$Platform)
    )
}
if (-not (Test-Path -LiteralPath $payload)) { throw 'Compiled payload staging is missing.' }

$script:checks = @()
function Sign-GRFile([string]$Path) {
    Write-Host ('Signing ' + [IO.Path]::GetFileName($Path))
    & $SignToolPath sign /sha1 $CertificateThumbprint /s My /fd SHA256 /tr $TimestampUrl /td SHA256 /d 'GR Taskbar' /du 'https://grstudios.shop' $Path
    if ($LASTEXITCODE -ne 0) { throw "Signing/timestamping failed: $Path" }
    $script:checks += Test-GRSignature -Path $Path -Thumbprint $CertificateThumbprint -Mode $SigningMode
}

$names = @('GR-Taskbar.amd64.dll','GR-Taskbar.arm64.dll','GR-Settings.dll','GR-Taskbar-DwmService.exe',
    'GR-Taskbar-StartMenu.dll','GR-Taskbar-StartTileData.dll','GR-Taskbar-WeatherHost.dll','GR-Taskbar-WeatherProxy.dll',
    'GR-Taskbar-Extra.dll','GR-Taskbar-Win7AltTab.dll','GR-Taskbar-SetupPatch.exe',
    'GR-Taskbar-ReleaseName.exe','GR-Taskbar-ReleaseNotes.exe')
$required = @('GR-Settings.dll','GR-Taskbar-DwmService.exe','GR-Taskbar-StartMenu.dll',
    'GR-Taskbar-StartTileData.dll','GR-Taskbar-WeatherHost.dll','GR-Taskbar-WeatherProxy.dll')
$required += $(if ($Platform -eq 'amd64') { 'GR-Taskbar.amd64.dll' } else { 'GR-Taskbar.arm64.dll' })
foreach ($name in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $output $name))) { throw "Required component missing: $name" }
    if (-not (Test-Path -LiteralPath (Join-Path $payload $name))) { throw "Required component missing from installer payload: $name" }
}
foreach ($name in $names) {
    $file = Join-Path $output $name
    if (Test-Path -LiteralPath $file) {
        Sign-GRFile $file
        $member = Join-Path $payload $name
        if (Test-Path -LiteralPath $member) { Copy-Item -LiteralPath $file -Destination $member -Force }
    }
}
$x86 = Join-Path $root "build\$Configuration\Win32\GR-Taskbar.IA-32.dll"
if (-not (Test-Path -LiteralPath $x86)) { throw 'The IA-32 helper is missing.' }
Sign-GRFile $x86
Copy-Item -LiteralPath $x86 -Destination (Join-Path $payload 'GR-Taskbar.IA-32.dll') -Force

# Prove that verification rejects altered file contents without changing trust stores.
$probe = Join-Path $WorkRoot 'logs\GR-signature-tamper-test.dll'
Copy-Item -LiteralPath (Join-Path $output 'GR-Settings.dll') -Destination $probe -Force
$bytes = [IO.File]::ReadAllBytes($probe); $bytes[64] = $bytes[64] -bxor 1; [IO.File]::WriteAllBytes($probe,$bytes)
if ([GRBuildVerification]::TrustStatus($probe) -ne [Convert]::ToUInt32('80096010',16)) { throw 'Signature tamper regression check failed.' }
Remove-Item -LiteralPath $probe -Force

$stagedCatalog = Join-Path $payload 'GR-Taskbar.cat'
if (Test-Path -LiteralPath $stagedCatalog) { Remove-Item -LiteralPath $stagedCatalog -Force }
$catalog = Join-Path $output 'GR-Taskbar.cat'
if (Test-Path -LiteralPath $catalog) { Remove-Item -LiteralPath $catalog -Force }
New-FileCatalog -Path $payload -CatalogFilePath $catalog -CatalogVersion 2 | Out-Null
Sign-GRFile $catalog
if ((Test-FileCatalog -Path $payload -CatalogFilePath $catalog).ToString() -ne 'Valid') { throw 'Catalog verification failed before packaging.' }

# Only repackage setup: rebuilding component projects here would invalidate signatures.
$solutionDirectory = $root.Replace('\','/') + '/'
Invoke-GRBuild -Label package -Commands @(
    ('"{0}" gr_setup\gr_setup.vcxproj /p:Configuration={1} /p:Platform={2} /p:BuildProjectReferences=false /p:GRIncludeCatalog=true "/p:SolutionDir={3}" /v:minimal' -f $msbuild,$Configuration,$arch,$solutionDirectory)
)
if ((Test-FileCatalog -Path $payload -CatalogFilePath $catalog -FilesToSkip 'GR-Taskbar.cat').ToString() -ne 'Valid') { throw 'Packaged payload differs from its signed catalog.' }
$installer = Join-Path $output 'GR-Taskbar-Setup.exe'
$main = Join-Path $output $(if ($Platform -eq 'amd64') { 'GR-Taskbar.amd64.dll' } else { 'GR-Taskbar.arm64.dll' })
& (Join-Path $output 'GR-Taskbar-SetupPatch.exe') $main $installer
if ($LASTEXITCODE -ne 0) { throw 'Installer update identity stamping failed.' }
Sign-GRFile $installer
if (-not [GRBuildVerification]::IconMatches($installer,(Join-Path $root 'gr_gui\resources\gr-taskbar.ico'))) { throw 'Installer icon resources do not match the approved project ICO.' }

$release = Join-Path $WorkRoot 'outputs'
$finalName = if ($Platform -eq 'amd64') { 'GR-Taskbar-Setup.exe' } else { 'GR-Taskbar-Setup-ARM64.exe' }
$final = Join-Path $release $finalName
Copy-Item -LiteralPath $installer -Destination $final -Force
Copy-Item -LiteralPath $catalog -Destination (Join-Path $release 'GR-Taskbar.cat') -Force
Export-Certificate -Cert $certificate -FilePath (Join-Path $release 'GR-Taskbar-Public.cer') | Out-Null
$checks += Test-GRSignature -Path $final -Thumbprint $CertificateThumbprint -Mode $SigningMode
$report = [ordered]@{
    Version = (Get-Item -LiteralPath $final).VersionInfo.FileVersion
    SigningMode = $SigningMode
    TrustStoreChanged = $false
    CertificatePrivateKeyCopied = $false
    TamperDetection = 'Passed: Windows rejected altered content with TRUST_E_BAD_DIGEST'
    CatalogHashAlgorithm = 'SHA256'
    CatalogPayloadVerification = 'Valid'
    InstallerIcon = 'Matches approved ICO bytes at every embedded resolution'
    Signatures = $checks
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $release 'GR-Taskbar-SIGNING-REPORT.json') -Encoding UTF8
$manifest = @($finalName,'GR-Taskbar.cat','GR-Taskbar-Public.cer','GR-Taskbar-SIGNING-REPORT.json') | ForEach-Object {
    $hash = Get-FileHash -LiteralPath (Join-Path $release $_) -Algorithm SHA256
    '{0}  {1}' -f $hash.Hash.ToLowerInvariant(),$_
}
[IO.File]::WriteAllLines((Join-Path $release 'SHA256SUMS.txt'),$manifest,[Text.UTF8Encoding]::new($false))
Write-Host "Signed installer: $final"
Write-Host "Signing mode: $SigningMode; no Windows trust/security policy was changed."
