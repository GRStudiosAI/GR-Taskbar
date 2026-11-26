# GR Taskbar Desktop Window Manager Service

**Maintained by GR Studios AI LLC**

*Where Imagination Meets Intelligence.*

## Overview

The GR Taskbar Desktop Window Manager (DWM) Service is an integrated Windows customization component designed to control rounded window corners in Windows 11.

This component supports the restoration of traditional square window corners for users who prefer classic Windows desktop appearances.

## Features

- Windows 11 rounded window corner customization.
- Classic Windows desktop appearance support.
- Integration with GR Taskbar and GR Settings.
- Windows service-based component management.
- Support for future desktop appearance enhancements.

## Component Naming

| Component | Name |
|---|---|
| Component directory | `gr_dwm` |
| Service executable | `gr_dwm_svc.exe` |
| Visual Studio solution | `gr_dwm.sln` |
| Visual Studio project | `gr_dwm.vcxproj` |
| Main source file | `gr_dwm.c` |
| x64 source file | `gr_dwm_x64.c` |
| Header file | `gr_dwm.h` |
| Resource file | `gr_dwm.rc` |
| Intended service identifier | `gr_dwm` |
| Intended service display name | GR Taskbar DWM Service |
| Installer | `GR-Taskbar-Setup.exe` |

## Windows Compatibility

The inherited implementation was originally tested on Windows 11 build 22000.434.

Compatibility with newer Windows builds must be verified through the GR Taskbar development and testing process.

## Installation and Service Management

The GR Taskbar installer is intended to manage the complete lifecycle of the DWM service, including:

- Component installation.
- Windows service registration.
- Service startup and shutdown.
- Component upgrades and repairs.
- Compatibility verification.
- Safe service removal.
- Transactional rollback when installation fails.

The service should be managed through GR Taskbar rather than through manual installation into native Windows system directories.

## Security and Reliability

The DWM service must:

- Protect native Windows system files.
- Validate Windows build compatibility.
- Preserve existing GR Taskbar configurations.
- Handle unsupported Windows versions safely.
- Restore default window behavior when disabled or removed.
- Provide diagnostic logging for service failures.
- Maintain compatibility with the GR Taskbar installer and servicing system.

## Development and Maintenance

The GR Taskbar Desktop Window Manager Service is maintained as part of the GR Taskbar project under GR Studios AI LLC.

Future development focuses on modernization, reliability, performance, desktop appearance customization and compatibility with newer Windows versions.

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