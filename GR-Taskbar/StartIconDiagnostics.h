#pragma once
#include <stdarg.h>
#include "../version.h"

// Bounded, opt-in diagnostics. Painting only appends to memory; disk output is
// produced when recording is stopped. No input hooks, polling or decode work.
#define GR_ORB_TRACE_ROWS 256
#define GR_ORB_TRACE_COLUMNS 768
static SRWLOCK grOrbTraceLock = SRWLOCK_INIT;
static volatile LONG grOrbTraceEnabled;
static WCHAR (*grOrbTraceRows)[GR_ORB_TRACE_COLUMNS];
static UINT grOrbTraceCount, grOrbTraceDropped;

static void GRStartIconTrace(PCWSTR format, ...)
{
    if (!InterlockedCompareExchange(&grOrbTraceEnabled, 0, 0) || !TryAcquireSRWLockExclusive(&grOrbTraceLock)) return;
    DWORD saved = GetLastError();
    if (grOrbTraceEnabled && grOrbTraceRows) {
        if (grOrbTraceCount < GR_ORB_TRACE_ROWS) {
            WCHAR* row = grOrbTraceRows[grOrbTraceCount++];
            int prefix = swprintf_s(row, GR_ORB_TRACE_COLUMNS, L"tick=%llu pid=%lu tid=%lu ",
                GetTickCount64(), GetCurrentProcessId(), GetCurrentThreadId());
            if (prefix > 0) {
                va_list args; va_start(args, format);
                _vsnwprintf_s(row + prefix, GR_ORB_TRACE_COLUMNS - prefix, _TRUNCATE, format, args);
                va_end(args);
            }
        } else ++grOrbTraceDropped;
    }
    ReleaseSRWLockExclusive(&grOrbTraceLock);
    SetLastError(saved);
}

static void GRStartIconTraceControl(BOOL enabled)
{
    AcquireSRWLockExclusive(&grOrbTraceLock);
    if (!!grOrbTraceEnabled == !!enabled) { ReleaseSRWLockExclusive(&grOrbTraceLock); return; }
    InterlockedExchange(&grOrbTraceEnabled, FALSE);
    if (!enabled && grOrbTraceRows) {
        WCHAR path[MAX_PATH];
        DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
        if (count && count < MAX_PATH - 100) {
            wcscat_s(path, MAX_PATH, L"\\GR-Taskbar"); CreateDirectoryW(path, NULL);
            wcscat_s(path, MAX_PATH, L"\\Logs"); CreateDirectoryW(path, NULL);
            WCHAR file[MAX_PATH];
            swprintf_s(file, MAX_PATH, L"%s\\StartIcon-%lu-%llu.log", path, GetCurrentProcessId(), GetTickCount64());
            HANDLE output = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (output != INVALID_HANDLE_VALUE) {
                char utf8[GR_ORB_TRACE_COLUMNS * 4]; DWORD written;
                for (UINT i = 0; i < grOrbTraceCount; ++i) {
                    int length = WideCharToMultiByte(CP_UTF8, 0, grOrbTraceRows[i], -1, utf8, sizeof(utf8), NULL, NULL);
                    if (length > 0) { WriteFile(output, utf8, (DWORD)length - 1, &written, NULL); WriteFile(output, "\r\n", 2, &written, NULL); }
                }
                char summary[128];
                int length = sprintf_s(summary, sizeof(summary), "bounded recording: entries=%u dropped=%u\r\n", grOrbTraceCount, grOrbTraceDropped);
                if (length > 0) WriteFile(output, summary, (DWORD)length, &written, NULL);
                CloseHandle(output);
            }
        }
        HeapFree(GetProcessHeap(), 0, grOrbTraceRows); grOrbTraceRows = NULL;
    }
    if (enabled) {
        grOrbTraceRows = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, GR_ORB_TRACE_ROWS * sizeof(*grOrbTraceRows));
        grOrbTraceCount = grOrbTraceDropped = 0;
        if (grOrbTraceRows) InterlockedExchange(&grOrbTraceEnabled, TRUE);
    }
    ReleaseSRWLockExclusive(&grOrbTraceLock);
}
