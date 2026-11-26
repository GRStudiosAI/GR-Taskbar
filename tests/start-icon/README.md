# Native Start artwork paint regression

Run `run.cmd` in a Visual Studio x64 Native Tools command prompt.

The test creates a hidden native window with a deliberately blue default painter,
installs the production Start artwork paint subclass, and exercises normal, hot,
pressed and checked accessibility states using transparent three-frame artwork.
It verifies that custom painting bypasses the blue painter, click messages reach
the native handler, disabling custom artwork restores native painting, and 1,000
state paints do not grow GDI object use. Layout checks cover 100%, 125%, 150%,
175% and 200% scaling.

This is an isolated regression test with a controlled accessibility provider. It
does not establish the real taskbar's accessibility flags or prove installed visual
behavior. Record those using `scripts/Trace-StartIcon.ps1 -Action Start`, exercise
Start on the installed build, then use `-Action Stop` to flush the bounded log.
No restart is required for the recorder; logs stay in the local GR Taskbar Logs
folder. No input polling timer or global mouse hook is installed.
