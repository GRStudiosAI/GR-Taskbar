# GR Taskbar changelog

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