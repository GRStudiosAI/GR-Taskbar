# GR StartTileData

**Maintained by GR Studios AI LLC**

*Where Imagination Meets Intelligence.*

## Overview

GR StartTileData is an integrated component of GR Taskbar designed to maintain Windows 10 Start menu compatibility on Windows 11 24H2, starting with builds 26xxx.8493, and subsequent Windows versions.

## Purpose

The component is intended to:

- Preserve Windows 10 Start menu functionality.
- Maintain compatibility with GR Taskbar.
- Support classic Windows desktop customization.
- Maintain compatibility with supported Windows builds.
- Integrate with the GR Taskbar build and installation system.

## Technical Foundation

The inherited implementation is based on the analysis of the following publicly distributed Windows components:

- `StartTileData.dll` — Build 22621.1992, part of Windows 11 22H2.
- `SharedStartModel.dll` — Build 14361, used as a reference for member, argument, and local variable names.

The original implementation is a reverse-engineered, source-level reconstruction derived from publicly distributed Windows binaries and symbols. No leaked or confidential Microsoft source code was used in that reconstruction.

## Component Naming

The GR Taskbar migration uses the following component names:

- Component directory: `gr_starttiledata`
- Dynamic-link library: `gr_starttiledata.dll`
- Visual Studio project: `gr_starttiledata.vcxproj`
- Visual Studio solution: `gr_starttiledata.slnx`
- Module definition: `gr_starttiledata.def`
- IDL project: `gr_starttiledata_idl.vcxproj`

## Development and Maintenance

GR StartTileData is maintained as part of the GR Taskbar project under GR Studios AI LLC.

Future development focuses on compatibility, reliability, performance, modernization and integration with GR Taskbar.

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