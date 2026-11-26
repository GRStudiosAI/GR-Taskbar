# GR Taskbar Changelog

Developed and maintained by GR Studios AI LLC.

## 26100.9711.71.14 — November 26, 2026 (development)

- Corrected an observed update race: Windows may restart Explorer before DLL
  publication ends, leaving an old mapped renderer behind newly versioned files.
- The installer now accepts only a responsive shell process created after its
  post-publication boundary, with bounded reloads of an early respawn.
- Added native process-creation tests for pre-publication, post-publication and
  invalid processes. Full file-servicing regression checks pass.

On the live PC, reloading after 71.13 publication activated the new custom painter.
Its normal/hover transition was recorded without the native blue square. After
the 71.14 live update, native records captured normal frame 0, hover frame 1 and
pressed frame 2. The user confirmed the issue was working on the live PC.

## 26100.9711.71.13 — November 26, 2026 (development)

- Replaced custom Start button painting independently of the native tinted-glyph
  cache and blue highlight. Windows retains activation and keyboard handling.
- Added native accessibility state selection with an on-paint pointer fallback;
  normal, hover and pressed/active artwork share the same transparent layout.
- Restored Automatic sizing to 24 logical pixels on a 40-pixel taskbar.
- Added bounded, opt-in Start icon diagnostics and isolated paint regression tests.
- Corrected renamed component, resource and generated proxy build references.
- Included StartTileData explicitly in the signed build and required-payload checks.
- Added SHA-256 verification and persistent caching for pinned x64 Microsoft payloads.
- Updated existing DWM service configuration to reference the renamed executable.
- Disabled updater networking during migration pending release-path verification.

Installed desktop hover/pressed/active validation is still required. This entry
does not certify full retro skins, gallery redevelopment or all installation paths.

## 26100.9711.71.10 — November 25, 2026

### Installer improvements

- Implemented staged file replacement with recoverable backups.
- Added detailed installer diagnostics and native Windows error logging.
- Added file-lock detection using Windows Restart Manager.
- Improved controlled Explorer shutdown and restart.
- Added automatic retry handling for temporary file locks.
- Implemented transactional rollback and interrupted installation recovery.
- Improved obsolete backup file cleanup.
- Added deferred servicing retries following a system restart.
- Improved preservation of user settings and custom libraries.
- Added protection against unintended file deletion.
- Added installer servicing regression tests.

## 26100.9711.71.9 — November 25, 2026

### Start icon improvements

- Corrected undersized Start button artwork.
- Improved artwork positioning and scaling.
- Added Automatic, Small, Medium, Large and Custom icon sizes.
- Added configurable icon dimensions from 16 to 48 logical pixels.
- Added per-monitor DPI-aware rendering.
- Added immediate configuration persistence.
- Added Reset to Default functionality.
- Improved image filtering and ICO frame selection.
- Increased generated artwork resolution to 256 pixels per state.
- Added actual-size artwork previews.
- Improved secondary taskbar refresh handling.

### Project identity

- Updated ownership information for original GR Taskbar modifications.
- Established GR Studios AI LLC branding.
- Removed obsolete product migration code.

## 26100.9711.71.8 — November 25, 2026

### Project modernization

- Established the GR Taskbar project directory.
- Renamed the main source project and solution.
- Renamed project component directories.
- Updated solution, resource, installer and workflow references.
- Configured the GR Taskbar repository and intended update infrastructure.
- Updated installer metadata and application branding.
- Updated application shortcuts.
- Updated business and support website information.
- Rewrote project documentation.

### Settings improvements

- Corrected embedded Start icon library discovery.
- Added a draggable custom window header.
- Improved window resizing.
- Added visible scrollbars and scrolling controls.
- Improved interface spacing and typography.
- Added a consolidated appearance preset selector.
- Improved retro appearance preset controls.
- Added secondary taskbar appearance support.

### Icons and appearance

- Added independent Windows XP, Windows 7, Windows 10 and Windows 11-inspired Start icon options.
- Added custom artwork importing.
- Added duplicate filename preservation.
- Added artwork previews.
- Added PNG, BMP, ICO and JPEG support.
- Added three-frame Start icon artwork support.
- Added generated effects for single-frame artwork.
- Added customizable appearance profiles.
- Added custom color controls.
- Added appearance undo functionality.
- Added settings and custom artwork migration.

## 26100.9711.71.6

- Introduced appearance previews.
- Introduced the redesigned Settings interface.
- Expanded desktop appearance customization.

## 26100.9711.71.4

- Introduced custom Start icon support.
- Introduced the initial custom Start icon installer.

## Latest development progress

### Completed

- Implemented digital signing infrastructure.
- Expanded GR Taskbar branding.
- Continued component filename and directory migration.

### In development

- Complete normal, hover, pressed and active Start icon rendering corrections.
- Remove unwanted black backgrounds from Orbs gallery previews.
- Eliminate unwanted Start button background overlays.
- Complete the independent GR Taskbar update system.
- Remove obsolete files following component renaming.
- Complete GR Settings branding and interface modernization.
- Replace the Properties context menu entry with GR Settings.
- Implement dedicated GR Settings application and taskbar icons.
- Complete custom installer icon integration.
- Complete registry-based Settings corrections.
- Expand Start menu customization.
- Implement complete Windows XP-inspired retro themes.
- Implement complete Windows 7-inspired retro themes.
- Implement Mouse Cursors settings and custom cursor libraries.
- Expand File Explorer appearance customization.
- Complete installer, signature and SHA-256 verification.
- Complete Windows runtime, installation and stability testing.

---

Copyright © 2026 GR Studios AI LLC — Original GR Taskbar development and modifications.

This changelog documents GR Taskbar development exclusively. Applicable third-party license information is maintained separately.
