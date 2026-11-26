# Development status — 2026-11-25

The current request is recorded in D:\Projects\work\audit\overhaul-request.md.
This is an active overhaul, not a declaration that the final release is complete.

## Verified evidence

- Installer 71.10 completed a live upgrade and recovery with return code 3010;
  old loaded backup files were queued for deletion. Explorer responded afterward.
- The live test exposed excessive desktop downtime while servicing 1,949 artwork
  files. Source ordering now keeps artwork servicing outside the binary shell-stop
  interval and retains unchanged files. This revised ordering needs runtime testing.
- Isolated file-servicing tests cover native sharing error 32, temporary locks,
  mapped DLL rollback, interrupted/corrupt journals, reboot queue failure, Unicode,
  repair/reinstall/uninstall and rejection of native Windows targets.
- The selected catgirl PNG produces identical pixels to the pre-gallery decoder,
  with three distinct frame hashes. It is 724 x 724 pixels per frame.
- All 1,941 non-excluded installed-library artwork files decode. Native alpha-aware
  BMP loading restores transparency in 1,789 files that WIC had treated as opaque.
  The audited number of distinct three-frame files rises from 1,688 to 1,709.
- The speculative cursor/timer state override could discard native hover state;
  source now uses the previous native theme-state path with pressed/checked states.
  Actual taskbar hover/active/transparency verification remains outstanding.
- Both source and installed Orbs libraries contained 1,949 files. The Nudes
  category is excluded from gallery enumeration and from future packaging. Original
  source artwork is retained; provenance/redistribution rights still need review.
- The approved project icon has all nine required resolutions. Your signing
  certificate has an accessible private key and signed a SHA-256/RFC 3161 test file.
  Windows detects a tampered test file. The certificate is self-signed, not
  automatically trusted. No trust stores or security policies were changed.

## Still outstanding

Start menu redevelopment; actual Start taskbar runtime confirmation and blue-square
investigation; category/search/filter/asynchronous bounded gallery; full retro
shell skins; cursor library/roles/animated previews/schemes; full UI modernization;
all component/file/service/registry naming migration and safe old-file retirement;
complete upstream endpoint/dependency audit; CI signing configuration; controlled
fresh installation/uninstallation/reboot testing; broad Windows/DPI/monitor and
performance validation. Signing/icon build infrastructure is being verified now.

Lightweight widget/skin support and broader app dark mode remain future scope.
Per-app compatibility and exclusions will be necessary; universal support has not
been implemented or promised. Mica and external companion applications are not
required by the planned direction.
