# GR Taskbar installer servicing

## What “1377” meant

The former dialog appended `g_uFailureLine`, set by the `CHECK_OK` macro using
`__LINE__` in `gr_setup/ep_setup.c`. The English text came from
`gr_setup/resources/lang/ep_setup.en-US.rc`, `IDS_SETUP_FAILED`. Thus 1377 was a
build-dependent source checkpoint, not Windows error 1377 and not proof of a
particular locked backup. The original build did not preserve its failing native
error or path. That historical native error cannot be recovered from the photo.

The former `InstallResourceHelper` deleted `.prev` files, deleted installed files
before decoding their replacements, and tried to relocate locked files into an
AppData cleanup directory. A locked backup, a locked destination, a missing cleanup
parent, or a cross-volume move could abort servicing. Its CreateFile check also
incorrectly tested a failed handle against NULL rather than INVALID_HANDLE_VALUE.
The first later CHECK_OK reported a line number, obscuring the underlying failure.

Inspection found GR Taskbar loaded in Explorer, a registered GR Taskbar shell
extension, prior cleanup files, and a mixture of installed component timestamps.
Those observations support investigating file servicing; they do not establish the
unrecorded original Windows error. Regression tests reproduce native Windows error
32 (sharing violation) with real file handles and verify its exact path/code log.

## Implemented handling

`gr_setup/Servicing.h` provides UTF-8 logs, Restart Manager owner diagnostics,
canonical path/Windows ownership/WRP checks, bounded same-volume rename retries,
staged publication, recoverable backups and a flushed rollback journal. Duplicate
operations on a destination retain its original backup. A later installer recovers
an interrupted transaction after closing settings and stopping its own shell/service.
Corrupt journals are retained for diagnosis rather than discarded.

The setup requests settings-window closure, validates Explorer's executable and
session, requests graceful shell shutdown, then uses a bounded termination fallback
only for that validated shell PID. It retains the shell's user token, restarts the
shell and checks that the taskbar window responds before committing file backups.
Other applications and other users' Explorer processes are not forcibly terminated;
injected code is not remotely unloaded from arbitrary applications.

Old `.prev` files are cleanup rather than an installation prerequisite. Locked
backup deletion uses MoveFileExW(MOVEFILE_DELAY_UNTIL_REBOOT). If a persistent lock
prevents replacement, the file changes are rolled back and a protected copy of the
complete installer is queued once in HKLM RunOnce for after reboot and administrator
sign-in. The resume attempt does not requeue itself indefinitely. This deliberately
avoids mixing partially queued old/new DLL replacements. A reboot is not forced.

The error dialog uses MessageBoxW and explicit Unicode bullet escapes. It reports
operation, file, decimal/hex native error, system text, rollback outcome and log
path. English resources explicitly declare UTF-8. Source checkpoints remain labeled
as checkpoints, separate from the native code.

Settings and custom image libraries are not deleted. Uninstall removes known
payload files and its own shortcut rather than recursively deleting the installation
folder or AppData. Native System32 files are outside the permitted targets; WRP
files, unowned Windows components and reparse-directory traversal are rejected.

This is a file transaction, not a Windows-wide registry/service transaction.
Existing COM/service registration steps still require elevated integration testing.
Registry registration is not covered by the file rollback guarantee.

## Logs and verification

Logs: `%LOCALAPPDATA%\GR-Taskbar\Logs\Setup-*.log` (the account running setup).
File recovery journal: `%PROGRAMFILES%\GR-Taskbar\servicing.rollback`.
Do not delete a recovery journal or retained backup after a failed restoration.

Run `tests/installer/run.cmd` from a Visual Studio x64 Native Tools prompt for
isolated regression coverage. Tests use actual file locks, but intercept reboot
and RunOnce operations. They do not restart the desktop or alter live registry
settings. The x86 helper/x64 application build and actual installer extraction/
re-extraction were also checked; packaged DLL hashes match the compiled DLLs.

Before public release, verify the elevated fresh-install/upgrade/repair/reinstall/
uninstall paths and a real reboot-resume cycle on supported Windows versions.
ARM64 has not been built locally because its compiler is unavailable.
