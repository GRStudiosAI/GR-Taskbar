# Installer servicing regression tests

Run `run.cmd` in a Visual Studio x64 Native Tools command prompt. Tests use an
isolated temporary directory and real Windows file locks. Reboot deletion calls
are intercepted; no Explorer shutdown or live registry writes are performed.

Cases cover fresh payload creation, upgrades, repair/reinstall, known-file
uninstall, native sharing error 32 with rollback, a transient lock, publish
failure, durable journal recovery (including a target removed then recreated),
locked stale `.prev` cleanup, Unicode paths, and native Windows path rejection.

These are file-servicing regression tests, not complete OS installation tests.
Actual elevated shell/COM/service integration and a reboot-resume cycle must also
be tested on supported Windows versions before a public release.
