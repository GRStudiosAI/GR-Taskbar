# GR Taskbar signing and installer identity

Build from PowerShell with:

```powershell
.\scripts\Build-GRTaskbar.ps1 -SigningMode SelfSigned
```

Or use the `BuildSigned` target in `build.proj`. The existing solution, component
projects, resource compilation and installer packaging are used. The final file
is `D:\Projects\work\outputs\GR-Taskbar-Setup.exe` for this checkout.

The script discovers Visual Studio through its installer and reads the public
certificate from `D:\Projects\GR-Taskbar-Signing\GR-Taskbar-Public.cer` (the sibling
signing directory). It selects the matching private key in CurrentUser\My. The
PFX and its password are not read, copied, logged or stored in the repository.
Pass `-PublicCertificatePath` or set `GR_SIGN_CERT_THUMBPRINT` on another machine.
Pass `-VisualStudioPath`, `-SignToolPath` or `-WorkRoot` when needed.

The configured certificate is self-issued by GR Studios AI LLC. SelfSigned mode
accepts only the selected signer and the specific Windows untrusted-root result;
it does not add trusted roots or disable Windows verification/security policy.
The report explicitly says whether Windows automatically trusts the signature.
Use Trusted mode if a trusted-chain certificate is selected in the future.

Order: compile resources/components; sign GR component binaries with SHA-256 and
RFC 3161 SHA-256 timestamps; generate/sign a SHA-256 catalog; verify catalog data;
repackage setup without rebuilding signed components; stamp update metadata;
sign/timestamp setup last; verify signatures, approved icon resources and catalog;
write final SHA256SUMS.txt. Windows must reject a deliberately altered verification
fixture before a release is produced. Vendor binaries keep their existing embedded
signatures and are covered by the payload catalog.

Installed DLLs must remain byte-identical to the signed payload. The installer no
longer writes its update identity into the installed dxgi.dll DOS header; the GR
updater computes the identity from the unchanged signed file instead. The existing
installer metadata stamp is written before the installer is signed.

The approved purple GR ICO is shared from `gr_gui/resources/gr-taskbar.ico`, with
16, 20, 24, 32, 40, 48, 64, 128 and 256 pixel entries. The installer embeds that
resource and uses it for its native dialogs/title icons and process identity. The
build compares each embedded image's bytes against the approved source icon.

Do not treat self-signing as CA-verified company identity or automatic SmartScreen
reputation. A self-signed build can be distributed with its public certificate and
accurately labelled signing report; the private PFX must never be distributed.

Release sidecars: GR-Taskbar.cat, GR-Taskbar-Public.cer,
GR-Taskbar-SIGNING-REPORT.json and SHA256SUMS.txt. No signing credentials are needed
in source control. CI needs its own explicitly configured signing key/certificate;
the local private key is not automatically uploaded to GitHub.
