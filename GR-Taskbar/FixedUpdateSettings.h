#pragma once
#include <wchar.h>
#include <string.h>

// Original GR Studios AI LLC modifications. Update routing is product policy,
// not a user-editable registry preference. Saved older values are not executed.
static LSTATUS GRReadUpdateSetting(HKEY key, LPCWSTR name, LPDWORD reserved,
    LPDWORD type, LPBYTE data, LPDWORD bytes)
{
    (void)key; (void)reserved;
    if (!name || !bytes) return ERROR_INVALID_PARAMETER;
    LPCVOID source = NULL; DWORD required = 0, value = 0, kind = REG_DWORD;
    if (!_wcsicmp(name, L"UpdateURL") || !_wcsicmp(name, L"UpdateURLStaging")) {
        source = TEXT(UPDATES_RELEASE_INFO_URL_STABLE);
        required = (DWORD)((wcslen((LPCWSTR)source) + 1) * sizeof(WCHAR));
        kind = REG_SZ;
    } else {
        if (!_wcsicmp(name, L"UpdatePolicy")) value = UPDATE_POLICY_DEFAULT;
        else if (!_wcsicmp(name, L"UpdateTimeout")) value = UPDATES_DEFAULT_TIMEOUT;
        else if (_wcsicmp(name, L"UpdatePreferStaging") &&
            _wcsicmp(name, L"UpdateAllowDowngrades") && _wcsicmp(name, L"UpdateUseLocal"))
            return ERROR_FILE_NOT_FOUND;
        source = &value; required = sizeof(value);
    }
    DWORD capacity = *bytes; *bytes = required;
    if (type) *type = kind;
    if (!data) return ERROR_SUCCESS;
    if (capacity < required) return ERROR_MORE_DATA;
    memcpy(data, source, required);
    return ERROR_SUCCESS;
}
