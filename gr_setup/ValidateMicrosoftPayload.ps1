# GR Taskbar Microsoft Dependency Validation
# GR Studios AI LLC
# Where Imagination Meets Intelligence.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PayloadPath,

    [Parameter(Mandatory = $true)]
    [ValidateSet(34404, 43620)]
    [int]$ExpectedMachine,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedSHA256
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

try {
    Write-Host "GR Taskbar Microsoft Payload Validation"
    Write-Host "File: $PayloadPath"

    if (-not (Test-Path -LiteralPath $PayloadPath -PathType Leaf)) {
        throw "Required dependency does not exist: $PayloadPath"
    }

    $file = Get-Item -LiteralPath $PayloadPath
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)

    Write-Host "File size: $($file.Length) bytes"

    if ($bytes.Length -lt 64) {
        throw "Invalid or truncated dependency."
    }

    if ([BitConverter]::ToUInt16($bytes, 0) -ne 0x5A4D) {
        throw "Invalid DOS header."
    }

    $peOffset = [BitConverter]::ToInt32($bytes, 60)

    if ($peOffset -lt 64 -or $peOffset -gt ($bytes.Length - 24)) {
        throw "Invalid PE header offset."
    }

    if ([BitConverter]::ToUInt32($bytes, $peOffset) -ne 17744) {
        throw "Invalid PE signature."
    }

    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)

    $architecture = switch ($machine) {
        0xAA64 { "ARM64" }
        0x8664 { "x64" }
        0x014C { "x86" }
        0xA641 { "ARM64EC" }
        0xA64E { "ARM64X" }
        default { "Unknown" }
    }

    Write-Host "Detected architecture: $architecture"
    Write-Host ("Detected machine: 0x{0:X4}" -f $machine)
    Write-Host ("Expected machine: 0x{0:X4}" -f $ExpectedMachine)

    if ($machine -ne $ExpectedMachine) {
        throw (
            "Architecture mismatch. Expected 0x{0:X4}, received 0x{1:X4}." `
            -f $ExpectedMachine, $machine
        )
    }

    Write-Host "PE architecture validation passed."

    $actualHash = (
        Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    ).Hash

    Write-Host "Actual SHA256: $actualHash"

    if ($ExpectedSHA256 -ne "UNPINNED") {

        if ($ExpectedSHA256 -notmatch '^[A-Fa-f0-9]{64}$') {
            throw "Invalid expected SHA256 configuration."
        }

        Write-Host "Expected SHA256: $ExpectedSHA256"

        if ($actualHash -ne $ExpectedSHA256) {
            throw "Microsoft dependency SHA256 mismatch."
        }

        Write-Host "Pinned SHA256 validation passed."
    }
    else {
        Write-Host "Checking Microsoft Authenticode signature."

        $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName

        Write-Host "Signature status: $($signature.Status)"
        Write-Host "Signature message: $($signature.StatusMessage)"
        Write-Host "Signature type: $($signature.SignatureType)"

        if ($null -eq $signature.SignerCertificate) {
            throw (
                "No signer certificate available. " +
                "Signature status: $($signature.Status). " +
                "Details: $($signature.StatusMessage)"
            )
        }

        $subject = $signature.SignerCertificate.Subject

        Write-Host "Certificate subject: $subject"
        Write-Host "Certificate issuer: $($signature.SignerCertificate.Issuer)"
        Write-Host "Certificate thumbprint: $($signature.SignerCertificate.Thumbprint)"

        if ($signature.Status -ne "Valid") {
            throw (
                "Microsoft signature validation failed. " +
                "Status: $($signature.Status). " +
                "Details: $($signature.StatusMessage)"
            )
        }

        if ($subject -notmatch '(?:^|,)\s*O\s*=\s*Microsoft Corporation\s*(?:,|$)') {
            throw "Unexpected dependency publisher: $subject"
        }

        Write-Host "Microsoft Authenticode validation passed."
    }

    Write-Host "Microsoft dependency validation successful."
    Write-Host "Validated file: $PayloadPath"

    exit 0
}
catch {
    [Console]::Error.WriteLine("GR Taskbar Microsoft dependency validation failed.")
    [Console]::Error.WriteLine("File: $PayloadPath")
    [Console]::Error.WriteLine("Reason: $($_.Exception.Message)")

    exit 1
}
