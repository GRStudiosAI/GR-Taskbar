# Development status — 2026-11-26

The current request is recorded in D:\Projects\work\audit\2026-11-26\request.md.
This is an active overhaul, not a declaration that the final release is complete.

## Verified evidence

- Live 71.13 servicing exposed an Explorer auto-restart race: PID 7320 started at
  12:25:45 before the Windows shim was published at 12:25:50. Its mapped shim size
  was 659,456 bytes while the published image was 643,072 bytes. FileVersionInfo
  read the new on-disk version and did not establish the mapped version.
- A controlled post-publication reload produced PID 9944 with the expected mapped
  image size, valid debug-symbol state (custom style 3 and loaded bitmap), and real
  Start paint records. Normal frame 0 and hover frame 1 were captured, with no
  blue square in the recorded hover frames. This is not a completed pressed test.
- Build 71.14 adds the post-publication process-creation barrier to installer
  restart verification. Native process timing and full servicing tests passed;
  its signed package passed 16 signature and catalog checks. Live update testing
  of that installer completed successfully. Publication ended at 12:37:16 and
  Explorer PID 8492 was created afterward; the new restart check passed. Live
  debug-symbol reads confirmed custom style 3 and a loaded custom bitmap.
- On 71.14, the bounded live paint recording captured normal/frame 0 (2),
  hover/frame 1 (114), and pressed/frame 2 (10). The user confirmed "its working
  now". Temporary diagnostics were then disabled. The PC's reported hover/click
  issue is resolved; wider DPI, multi-monitor and OS coverage remains untested.

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
- Inspection of the installed alternate taskbar binary found state-zero pearl
  glyph rendering followed by separately tinted cached copies and background
  painting. Theme-state-only frame selection cannot supply the three states on
  that path. Custom artwork now handles native Start paint messages, reading
  accessibility pressed/checked/hot flags and an on-paint pointer fallback.
  Native input handling remains in place. No polling timer or mouse hook was added.
- Isolated production-painter tests passed for all four accessibility states,
  bypassing a deliberately blue native painter and forwarding clicks. A 1,000-paint
  run took 47 ms without GDI object growth. DPI layout checks cover 100–200%.
  These controlled-provider tests do not replace installed taskbar validation.
- The x64 component compiled after these changes; the signed 71.12 package passed
  signature, catalog, tamper and nine-resolution installer-icon checks. Its final
  audit found StartTileData missing, so installation was put on hold. Build 71.13
  explicitly compiles that component and refuses a missing required payload member.
  Final 71.13 subsequently passed 16 signature checks, catalog/tamper checks,
  installer-icon checks, and an independent embedded-ZIP CRC/member audit (2,048
  entries). All nine required payload members matched verified staging bytes.
  Installed testing of the corrected package remains outstanding.
- Explorer was confirmed still running 71.11 after the screenshot report. Its
  saved orb path referenced a missing file under the previous product directory.
  The existing GR library artwork is 724 x 2172 with distinct frame hashes and
  transparent corners in all three frames. Prepared the current user's GR profile
  with 97 missing settings copied without overwriting existing values or importing
  old update URLs; the new selection points to the existing GR library asset.
  Previous settings/files were retained. This was a one-time local data migration,
  not an installer feature or a claim that all prior profiles migrate automatically.
- Both source and installed Orbs libraries contained 1,949 files. The Nudes
  category is excluded from gallery enumeration and from future packaging. Original
  source artwork is retained; provenance/redistribution rights still need review.
- The approved project icon has all nine required resolutions. Your signing
  certificate has an accessible private key and signed a SHA-256/RFC 3161 test file.
  Windows detects a tampered test file. The certificate is self-signed, not
  automatically trusted. No trust stores or security policies were changed.

## Still outstanding

Start menu redevelopment; wider Start icon DPI/monitor/build coverage;
category/search/filter/asynchronous bounded gallery; full retro
shell skins; cursor library/roles/animated previews/schemes; full UI modernization;
all component/file/service/registry naming migration and safe old-file retirement;
complete upstream endpoint/dependency audit; CI signing configuration; controlled
fresh installation/uninstallation/reboot testing; broad Windows/DPI/monitor and
performance validation. The signed local build has passed digest, timestamp,
catalog, tamper and installer-icon verification; release CI remains unverified.

Lightweight widget/skin support and broader app dark mode remain future scope.
Per-app compatibility and exclusions will be necessary; universal support has not
been implemented or promised. Mica and external companion applications are not
required by the planned direction.
