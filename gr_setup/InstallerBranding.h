#pragma once
// Original installer identity and dialog branding: GR Studios AI LLC, 2026.
static HICON grInstallerLargeIcon, grInstallerSmallIcon;

static void GRInitializeInstallerBranding(HINSTANCE instance)
{
    SetCurrentProcessExplicitAppUserModelID(L"GRStudiosAI.GRTaskbar.Setup");
    grInstallerLargeIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_GR_TASKBAR), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED);
    grInstallerSmallIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_GR_TASKBAR), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
}

static LRESULT CALLBACK GRInstallerDialogIconHook(int code, WPARAM window, LPARAM data)
{
    if (code == HCBT_ACTIVATE) {
        WCHAR name[32];
        if (GetClassNameW((HWND)window, name, ARRAYSIZE(name)) && !wcscmp(name, L"#32770")) {
            SendMessageW((HWND)window, WM_SETICON, ICON_BIG, (LPARAM)grInstallerLargeIcon);
            SendMessageW((HWND)window, WM_SETICON, ICON_SMALL, (LPARAM)grInstallerSmallIcon);
        }
    }
    return CallNextHookEx(NULL, code, window, data);
}

static int GRInstallerMessageBox(HWND owner, PCWSTR text, PCWSTR caption, UINT flags)
{
    HHOOK hook = SetWindowsHookExW(WH_CBT, GRInstallerDialogIconHook, NULL, GetCurrentThreadId());
    MSGBOXPARAMSW parameters = { 0 };
    parameters.cbSize = sizeof(parameters);
    parameters.hwndOwner = owner;
    parameters.hInstance = GetModuleHandleW(NULL);
    parameters.lpszText = text;
    parameters.lpszCaption = caption;
    parameters.dwStyle = grInstallerLargeIcon ? (flags & ~MB_ICONMASK) | MB_USERICON : flags;
    parameters.lpszIcon = MAKEINTRESOURCEW(IDI_GR_TASKBAR);
    int result = MessageBoxIndirectW(&parameters);
    if (hook) UnhookWindowsHookEx(hook);
    return result;
}
