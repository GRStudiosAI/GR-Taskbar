# GR Taskbar

Windows desktop customization maintained by **GR Studios AI LLC**.

- Business website: [grstudios.shop](https://grstudios.shop)
- Source and issues: [GRStudiosAI/GR-Taskbar](https://github.com/GRStudiosAI/GR-Taskbar)
- Downloads: [Releases](https://github.com/GRStudiosAI/GR-Taskbar/releases)

## Current build

Version **26100.9711.71.14** is a development build. The Start icon hover/click
fix is confirmed on the development PC; wider compatibility testing remains.
Save open work before installing: setup restarts Windows Explorer.
Use `GR-Taskbar-Setup.exe` on Intel/AMD Windows. ARM64 builds require separate
compilation and validation; this development build was compiled for x64 and x86.

Open **GR Taskbar Settings** from Start or the taskbar context menu.
Settings use a compact sidebar and dropdown choices. Drag the top header to
move the window. Long pages have a vertical scrollbar and support the mouse
wheel. The window can be resized.

## Start icon

**Start icon** appears directly below **Start menu**. The embedded library
contains Windows XP, Windows 7, Windows 10, Windows 11, and your imported icons.
Each entry shows one icon. The selected preview changes in place when hovered
or pressed; it does not display three separate state images.

1. Select **Add images...** to import one or several files.
2. Choose an icon in the saved library.
3. Select **Use selected** to apply it.

Supported formats: PNG, BMP, ICO, JPG/JPEG. Use a square image up to 1024 pixels
per side, or an orb strip with three equal-height frames stacked vertically
(normal, hover, pressed), up to 1024 x 3072 pixels. Square images get automatic
hover and pressed effects. Imported files are copied into
`%LOCALAPPDATA%\GR-Taskbar\Orbs\`.

Start icon selection is independent of Start menu and taskbar styles. Actual
replacement uses a Windows 10 taskbar implementation, including the integrated
alternate taskbar. It does not replace the native Windows 11 XAML Start button.

## Start icon sizing

The **Start icon** page includes **Automatic**, **Small (16 px)**,
**Medium (24 px)**, **Large (32 px)** and **Custom (16–48 px)** sizing, plus
**Reset to default**. Values are logical pixels: the selected size follows
Windows display scaling independently on each monitor. Automatic is 24 logical
pixels on a standard 40-logical-pixel taskbar and adapts to other button sizes.

Artwork is centered, aspect-preserving and fitted inside the full Start button,
not the smaller theme-glyph rectangle. A common alpha bound removes transparent
margins without shifting or clipping normal, hover, pressed and active states.
Images use premultiplied-alpha WIC resampling; ICO files use their highest-resolution
usable frame. PNG inputs up to 4096 pixels wide are normalized to a bounded
high-resolution working image. Oversized custom choices are constrained to the
available Start button space. Size changes are persistent and repaint primary
and secondary taskbars without a Windows restart.
## Appearance and custom themes

The **Themes** page has a preset dropdown, custom taskbar color pickers,
custom `.eptheme` save/load, and **Undo last theme**. Select the areas a profile
may change. Start icon application is off by default and must be explicitly
enabled if a profile should replace your selected icon.

| Profile | What it configures |
| --- | --- |
| XP Luna Blue / Royale | Retro-inspired palette and available classic shell options |
| Windows Classic | Gray palette and available classic shell options |
| Windows 7 | Aero-inspired palette and available classic Explorer options |
| Windows 10 | Windows 10 taskbar and available Explorer/menu options |
| Windows 11 | Native Windows 11 taskbar and available modern Explorer/menu options |

All presets use compact taskbar buttons without text labels. They do not select
wide tabs. Restart Explorer after applying a profile to load structural changes.
Availability of older Start menus and Explorer layouts depends on the running
Windows build; a theme cannot restore an OS component that is absent.

Custom themes are stored under `%LOCALAPPDATA%\GR-Taskbar\Themes`. Theme files
contain validated appearance values, not executable commands. Theme images
remain file references; include the image when sharing a custom theme.
High-contrast colors take precedence over custom taskbar colors.

These profiles are not complete XP/7 shell replicas. Full XP task-button skins,
XP Explorer icon packs, XP Start menu layouts and direct Open-Shell/RetroBar
skin imports remain outside the implemented feature set. Do not describe this
build as having those features.

## Settings and updates

The installer and UI are branded **GR Taskbar**, published by **GR Studios AI LLC**.
The installed folder is `%PROGRAMFILES%\GR-Taskbar`; configuration uses
`HKCU\Software\GR-Taskbar`. Current GR Taskbar icon libraries and customization
settings remain supported. Obsolete first-brand migration code was removed.
Update defaults point to `GRStudiosAI/GR-Taskbar`. Network update checks and
installation are disabled during migration (`GR_UPDATER_VERIFIED=0`) until the
official release download and verification path passes testing. Saved update
preferences are preserved. The x64 installer is `GR-Taskbar-Setup.exe`.
## Build and publish

Open `GR-Taskbar.sln` with Visual Studio 2026 and the Windows C++ workload.
Restore NuGet packages, initialize the listed dependencies, build zlib, and
build the x86 helper, StartTileData component and x64 solution. The signed local
entry point is `scripts/Build-GRTaskbar.ps1`; see [SIGNING.md](SIGNING.md).
It signs components before packaging, verifies the payload catalog, and signs
the final installer after metadata stamping. The approved project icon is embedded.
Pinned x64 Microsoft payload files are hash-checked in a persistent build cache.

The current GitHub workflow validates source only; it does not build or publish
installers. Release automation, ARM64 validation and installed testing remain
outstanding. Do not treat a successful local compilation as a verified release.

## License

GR Taskbar is a modified GPL-2.0 distribution. GR Studios AI LLC's modification
notice, required original copyright notices and the unchanged GPL v2 text are
included in [LICENSE](LICENSE). Installed copies include `LICENSE.txt`. The
source and component notices are retained; changing the product name does not
remove them.

[CHANGELOG.md](CHANGELOG.md) describes GR Taskbar changes. Funding configuration
is maintained separately by the project owner.
Installer servicing now stages files, retains rollback backups and logs native Windows errors under %LOCALAPPDATA%\GR-Taskbar\Logs. Read [INSTALLER-SERVICING.md](INSTALLER-SERVICING.md) for recovery behavior and verification limits.
