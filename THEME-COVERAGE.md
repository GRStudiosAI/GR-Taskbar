# GR Taskbar theme coverage — remaining implementation

Requested coverage includes Windows XP, Vista, 7, 8/8.1, 10 and 11 visual styles.
Each theme should be able to include taskbar appearance, icon-only task buttons,
Start menu appearance, a separately selectable Start icon/orb, Explorer appearance,
window decoration where supported, mouse cursors, and file/folder icon mappings.
Saved custom themes should preserve these choices together. Individual components
must remain independently selectable, with previews and a reset to the prior/default
configuration. No separate application is required or planned.

## Current implemented scope

Color/settings presets, independent Start icon library and three-state artwork,
Start icon size controls, and selected existing shell appearance options.
The presets do not constitute a complete recreation of each Windows shell.

## Outstanding scope

- Complete retro taskbar painting and icon-only layout for every supported mode.
- Actual XP/Vista/7/8 Start menu layouts and corresponding Explorer/window skins.
- Cursor pack import, preview, pointer-role mappings, persistence, apply and reset.
- File/folder icon pack import, preview, explicit type/role mappings and restoration.
- Theme manifest/versioning for those components and atomic application/rollback.
- Validation of every component on the supported Windows host versions and displays.

Cursor and shell icon changes affect the user's desktop beyond the settings window.
Implementation must retain prior values so removing or resetting a theme restores
them. Imported resources must be copied into the user's GR Taskbar library instead
of depending on the original upload location. Missing files must leave working
system defaults. System binaries must not be patched to apply artwork.

Original modifications belong to GR Studios AI LLC. Required source and artwork
licenses and attribution remain in the distribution. Windows version names identify
visual targets, not a claim that GR Taskbar runs on every historical Windows release.

## Requested future direction

Complete named themes (for example a Jarvis-inspired desktop) should apply all
supported components together while allowing individual overrides and restoration
of the previous configuration. Start11 and Curtains are behavior references for
integrated Start menu/window styling, not external runtime dependencies or bundled
proprietary code. Lightweight desktop widgets and skins with Rainmeter-like
flexibility are also requested. Measure idle CPU, memory, animation frame time and
battery impact before making performance claims. Mica styling is excluded from
this requested direction. These features remain future work.

Built-in broader dark-mode styling, including apps that ignore the Windows theme, is requested. Coverage must be tested per rendering framework and app; provide per-app exclusions and restoration. Universal compatibility is not currently implemented or promised.
