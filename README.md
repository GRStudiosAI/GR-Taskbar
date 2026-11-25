# GR Taskbar

Windows desktop customization maintained by **GR Studios AI LLC**.

- Business website: [grstudios.shop](https://grstudios.shop)
- Source and issues: [GRStudiosAI/GR-Taskbar](https://github.com/GRStudiosAI/GR-Taskbar)
- Downloads: [Releases](https://github.com/GRStudiosAI/GR-Taskbar/releases)

## Current build

Version **26100.9711.71.8** is a development build. Save open work before
installing: setup restarts Windows Explorer. Use `ep_setup.exe` on Intel/AMD
Windows; ARM64 releases use `ep_setup_arm64.exe` when available.

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

## Upgrading from the earlier build

The installer and UI are branded **GR Taskbar**, published by **GR Studios AI LLC**.
The main installed folder is `%PROGRAMFILES%\GR-Taskbar`. Legacy settings and
saved icon/theme libraries are imported without overwriting settings already
chosen in GR Taskbar. Legacy local data is retained as a rollback backup.
The retained component CLSID allows servicing the existing installation.

Update defaults point to `GRStudiosAI/GR-Taskbar`. The release asset names remain
`ep_setup.exe` and `ep_setup_arm64.exe` for updater compatibility. The separately
provided `GR-Taskbar-Setup-26100.9711.71.8.exe` is the same x64 installer.

## Build and publish

Open `GR-Taskbar.sln` with Visual Studio 2026 and the Windows C++ workload.
Restore NuGet packages, initialize the listed dependencies, build zlib, and
build the x86 helper and x64 solution. The existing workflow builds x64 and
ARM64 from `build.proj` and the `gr_` component folders.

After installed testing, commit the reviewed source with a message starting
with `rel_`, create a version tag and push both to this repository. The tag
workflow publishes installer assets. Keep the stable release asset filenames
unchanged and increment `version.h` for subsequent releases.

## License

GR Taskbar is a modified GPL-2.0 distribution. GR Studios AI LLC's modification
notice, required original copyright notices and the unchanged GPL v2 text are
included in [LICENSE](LICENSE). Installed copies include `LICENSE.txt`. The
source and component notices are retained; changing the product name does not
remove them.

[CHANGELOG.md](CHANGELOG.md) describes GR Taskbar changes. Funding configuration
is maintained separately by the project owner.