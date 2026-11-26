# GR Taskbar Custom Libraries Chainloader

**Maintained by GR Studios AI LLC**

*Where Imagination Meets Intelligence.*

## Overview

The GR Taskbar Custom Libraries Chainloader is an integrated component designed to extend GR Taskbar through additional modular libraries.

The chainloader provides a centralized mechanism for managing compatible extension modules after GR Taskbar completes its initialization.

## Purpose

The chainloader is intended to:

- Support multiple GR Taskbar extension modules.
- Provide a centralized module management system.
- Maintain compatibility with classic Windows customization features.
- Reduce dependencies between individual extension components.
- Support future GR Taskbar features and enhancements.
- Maintain reliable and controlled component initialization.

## Component Naming

All chainloader components must follow the GR Taskbar naming conventions.

| Component | Name |
|---|---|
| Component directory | `gr_extra` |
| Chainloader library | `gr_extra.dll` |
| Visual Studio project | `gr_extra.vcxproj` |
| Project filters | `gr_extra.vcxproj.filters` |
| Resource file | `gr_extra.rc` |
| Proposed entry point | `gr_extra_EntryPoint` |
| Extension naming convention | `gr_extra_*.dll` |

## Integrated Extension Modules

The GR Taskbar extension architecture includes the following compatibility module:

- `gr_extra_win7alttab.dll` — Windows 7 Alt-Tab compatibility module.

Additional modules may be introduced as GR Taskbar development progresses.

## Security and Reliability

The GR Taskbar chainloader should:

- Validate extension module authenticity and compatibility.
- Restrict module discovery to approved installation locations.
- Prevent unauthorized or untrusted library loading.
- Maintain compatibility with supported Windows versions.
- Protect native Windows system files.
- Provide detailed diagnostic logging.
- Handle component initialization failures safely.
- Preserve existing GR Taskbar settings and configurations.

## Installation and Management

The chainloader and its supported extension modules should be installed, updated, repaired and removed through the official GR Taskbar installer.

Manual modification of native Windows system directories is not recommended.

## Development and Maintenance

The GR Taskbar Custom Libraries Chainloader is maintained as part of the GR Taskbar project under GR Studios AI LLC.

Future development focuses on modernization, modular architecture, security, reliability, performance and compatibility.

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