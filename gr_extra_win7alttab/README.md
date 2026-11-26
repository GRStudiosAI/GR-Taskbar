# GR Taskbar Windows 7 Alt-Tab Module

**Maintained by GR Studios AI LLC**

*Where Imagination Meets Intelligence.*

## Overview

The GR Taskbar Windows 7 Alt-Tab Module is an integrated compatibility component designed to support the classic Windows 7 Alt-Tab experience on newer Windows versions.

## Features

- Classic Windows 7 Alt-Tab appearance.
- Integration with GR Taskbar.
- Classic desktop customization support.
- Compatibility with the GR Taskbar module loading system.
- Preservation of traditional window-switching functionality.

## Required Components

| Component | Description |
|---|---|
| `gr_extra_win7alttab.dll` | Windows 7 Alt-Tab compatibility module. |
| `gr_extra.dll` | GR Taskbar chainloader responsible for loading additional compatibility modules. |
| `AltTab.dll` | Original Windows 7 Alt-Tab implementation required by the inherited compatibility module. |

## Component Naming

- Component directory: `gr_extra_win7alttab`
- Dynamic-link library: `gr_extra_win7alttab.dll`
- Visual Studio project: `gr_extra_win7alttab.vcxproj`
- Project filters: `gr_extra_win7alttab.vcxproj.filters`
- Chainloader: `gr_extra.dll`
- Additional module naming convention: `gr_extra_*.dll`

## Installation and Integration

Installation and component management should be handled through the GR Taskbar installer.

The installation process must:

- Verify required component availability.
- Validate Windows version compatibility.
- Protect native Windows system files.
- Preserve existing GR Taskbar configurations.
- Support safe installation, upgrades, repairs and removal.
- Provide rollback handling if installation fails.

Native Windows system files must not be overwritten or replaced manually.

## Development and Maintenance

This module is maintained as part of the GR Taskbar project under GR Studios AI LLC.

Future development focuses on modernization, reliability, performance, Windows compatibility and integration with GR Taskbar's classic desktop customization features.

Inherited source code remains subject to its applicable licensing and attribution requirements.

## Official Project Resources

- [GR Taskbar Repository](https://github.com/GRStudiosAI/GR-Taskbar)
- [Report an Issue](https://github.com/GRStudiosAI/GR-Taskbar/issues)
- [GR Studios AI GitHub](https://github.com/GRStudiosAI)
- [Official Website](https://studio.grstudios.shop)
- [Support Development](https://github.com/sponsors/GRStudiosAI)

---

**GR Studios AI LLC**

*Where Imagination Meets Intelligence.*