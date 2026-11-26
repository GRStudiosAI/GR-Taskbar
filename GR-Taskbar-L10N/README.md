# GR-Taskbar-L10N

**Developed and maintained by GR Studios AI LLC**

*Where Imagination Meets Intelligence.*

## Overview

This directory contains the localization and translation resources for [GR Taskbar](https://github.com/GRStudiosAI/GR-Taskbar).

GR Taskbar provides Windows desktop customization, taskbar personalization, Start menu customization, custom Start icons, retro desktop appearances and integrated GR Settings.

## Localization Resources

The base English language resources are maintained within the main GR Taskbar repository:

- [GR Settings, Notifications and Crash Dialog](https://github.com/GRStudiosAI/GR-Taskbar/tree/master/gr_gui/resources/lang)
- [GR Taskbar Installer](https://github.com/GRStudiosAI/GR-Taskbar/tree/master/gr_setup/resources/lang)

All localization resources must follow the current GR Taskbar naming conventions and branding standards.

## Supported Languages

| Language | Language Code |
|---|---|
| English (United States) | en-US |
| Chinese (Simplified) | zh-CN |
| Chinese (Traditional) | zh-TW |
| Czech | cs-CZ |
| Dutch | nl-NL |
| French | fr-FR |
| German | de-DE |
| Greek | el-GR |
| Hungarian | hu-HU |
| Indonesian | id-ID |
| Japanese | ja-JP |
| Korean | ko-KR |
| Lithuanian | lt-LT |
| Polish | pl-PL |
| Portuguese (Brazil) | pt-BR |
| Romanian | ro-RO |
| Russian | ru-RU |
| Spanish (Spain) | es-ES |
| Turkish | tr-TR |
| Ukrainian | uk-UA |

## Localization Standards

All localization resource files must:

- Use UTF-8 encoding with BOM.
- Use CRLF (`\r\n`) line endings.
- Represent double quotes (`"`) as double double quotes (`""`) where required by Windows resource syntax.
- Follow the formatting of the base English resource files.
- Preserve resource identifiers and formatting placeholders.
- Preserve Unicode characters and language-specific punctuation.
- Use current GR Taskbar terminology and branding.
- Maintain compatibility with the GR Taskbar build system.

## File Naming Conventions

Localization files must use the following naming structure:

- GR Settings: `gr_gui.<language-code>.rc`
- GR Taskbar Installer: `gr_setup.<language-code>.rc`

Examples:

- `gr_gui.en-US.rc`
- `gr_gui.de-DE.rc`
- `gr_setup.en-US.rc`
- `gr_setup.de-DE.rc`

## Official Branding

The following branding must remain consistent across all localization resources:

- **Company:** GR Studios AI LLC
- **Product:** GR Taskbar
- **Settings Interface:** GR Settings
- **Installer:** GR Taskbar Setup
- **Localization Component:** GR-Taskbar-L10N

## Contributing

Translation corrections, language improvements, and additional localization support are welcome through the official GR Taskbar repository.

Contributors should ensure that translations are accurate, properly formatted, and compatible with the latest GR Taskbar source code.

New localization keys should be synchronized with the base English resources to maintain compatibility across all supported languages.

## Quality Assurance

Localization changes should be tested for:

- Correct text rendering.
- Unicode compatibility.
- Dialog and interface alignment.
- Text clipping and truncation.
- Installer message accuracy.
- GR Settings compatibility.
- Consistent product branding.
- Resource compilation compatibility.

## Third-Party Localization

Inherited translation resources remain subject to their applicable licenses and attribution requirements.

## Official Project Resources

- [GR Taskbar Repository](https://github.com/GRStudiosAI/GR-Taskbar)
- [Report an Issue](https://github.com/GRStudiosAI/GR-Taskbar/issues)
- [GR Studios AI GitHub](https://github.com/GRStudiosAI)
- [GR Studios AI Website](https://studio.grstudios.shop)
- [Support Development](https://github.com/sponsors/GRStudiosAI)

---

**GR Studios AI LLC**

*Where Imagination Meets Intelligence.*