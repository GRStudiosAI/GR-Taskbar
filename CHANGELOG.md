# GR Taskbar changelog

## 26100.9711.71.10 — 2026-11-25

- Replace destructive installer file replacement with staged writes and recoverable same-volume backups.
- Log operations, paths, native Windows codes and Restart Manager lock owners; display Unicode diagnostics.
- Close settings gracefully, stop only the validated current-session Explorer shell, retain its user token and verify restart.
- Retry temporary locks, roll back failed file changes and recover interrupted transactions from a flushed journal.
- Treat stale .prev files as cleanup; queue locked backup deletion at reboot.
- Queue a complete one-time servicing retry after reboot/admin sign-in when a persistent file lock prevents an update, rather than queuing partial DLL replacements.
- Preserve settings, custom libraries and unrelated installation files during uninstall; protect Windows components and reject reparse-directory traversal.
- Add installer file-servicing regression tests. Elevated OS integration and reboot-resume validation remain required before release.
## 26100.9711.71.9 — 2026-11-25

- Fixed undersized Start artwork by laying out against the full button bounds.
- Removed transparent padding using shared bounds across all three image states.
- Added Automatic, Small, Medium, Large and Custom logical-pixel sizes (16–48),
  immediate persistence, per-monitor DPI layout and Reset to default.
- Added cached WIC Fant filtering and highest-resolution ICO frame selection.
- Increased generated built-in resources to 256px per state and preserved all
  edge pixels in single-image pressed effects.
- Added DPI-aware sizing controls and actual-size preview in Start icon settings.
- Added explicit secondary-taskbar invalidation for immediate sizing changes.
- Updated original-modification ownership to GR Studios AI LLC; retained GPL v2
  and all required upstream notices. Windows retro-style options remain intact.
- Removed obsolete product migration code. Full retro shell, cursor and file-icon
  theme engines remain pending; current appearance presets are not complete skins.

## 26100.9711.71.8 — 2026-11-25

### Project and branding

- Renamed the local project to `D:\Projects\GR-Taskbar`.
- Renamed the main source project, solution and translation folder to GR-Taskbar.
- Renamed project folder prefixes from `ep_` to `gr_`, retaining their suffixes.
- Updated solution, resource, installer and workflow references for renamed paths.
- Updated the origin and updater defaults to GRStudiosAI/GR-Taskbar.
- Branded settings, installer metadata, app listing and shortcuts as GR Taskbar.
- Set GR Studios as publisher and grstudios.shop as the business/support website.
- Consolidated required notices into LICENSE and removed the separate notices file.
- Rewrote README for the current GR Taskbar feature set. Funding is unchanged.

### Repairs and compact settings

- Trimmed resource-line endings before comparing page names, fixing the missing
  embedded Start icon library.
- Added an explicit drag area to the custom window header and resize hit testing.
- Added a visible scrollbar and thumb/page scrolling for long settings pages.
- Reduced default size, spacing and typography toward the original compact layout.
- Replaced separate appearance preset links with one dropdown and an Apply action.
- Changed retro presets to combined icon buttons without text labels on both
  primary and secondary taskbars, with a one-time repair of the earlier layout.

### Icons and appearance

- Added independent XP, Windows 7, Windows 10 and Windows 11 Start icon choices.
- Added saved-library imports, duplicate-name preservation and in-place preview.
- Added PNG/BMP/ICO/JPEG decoding, square-icon effects and three-frame orb support.
- Hooked theme drawing in the alternate taskbar as well as the main shell.
- Added validated custom themes, color controls, six appearance profiles and undo.
- Added legacy settings/library migration for the project rename.

### Current limits

- Native Windows 11 XAML Start icon replacement is not implemented.
- Complete XP shell graphics, XP Start menu layouts and Explorer icon packs are
  not implemented. Presets combine palettes with existing supported options.
- ARM64 requires the corresponding compiler workload; local validation is x64.
- Live-shell rendering must be verified after installation of this build.

## Earlier development builds

- 26100.9711.71.6: initial appearance preview and settings redesign.
- 26100.9711.71.4: first custom-orb installer.

Earlier upstream development history remains in Git history. Original source
copyright notices and license terms remain applicable.