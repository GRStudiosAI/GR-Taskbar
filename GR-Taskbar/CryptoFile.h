#pragma once
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

static BOOL GRSha256File(PCWSTR path, BYTE digest[32])
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    BCRYPT_ALG_HANDLE algorithm = NULL; BCRYPT_HASH_HANDLE hash = NULL;
    BYTE* object = NULL; DWORD objectSize = 0, returned = 0, bytes = 0;
    BYTE buffer[65536]; BOOL ok = FALSE;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (BYTE*)&objectSize, sizeof(objectSize), &returned, 0) < 0)
        goto done;
    object = (BYTE*)HeapAlloc(GetProcessHeap(), 0, objectSize);
    if (!object || BCryptCreateHash(algorithm, &hash, object, objectSize, NULL, 0, 0) < 0) goto done;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &bytes, NULL)) goto done;
        if (!bytes) break;
        if (BCryptHashData(hash, buffer, bytes, 0) < 0) goto done;
    }
    ok = BCryptFinishHash(hash, digest, 32, 0) >= 0;
done:
    {
        DWORD error = ok ? ERROR_SUCCESS : GetLastError();
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        if (object) HeapFree(GetProcessHeap(), 0, object);
        CloseHandle(file); SetLastError(error ? error : ok ? ERROR_SUCCESS : ERROR_INVALID_DATA);
    }
    return ok;
}

static void GRHashHex(const BYTE* bytes, SIZE_T count, WCHAR* output)
{
    static const WCHAR digits[] = L"0123456789abcdef";
    for (SIZE_T i = 0; i < count; ++i) {
        output[2 * i] = digits[bytes[i] >> 4]; output[2 * i + 1] = digits[bytes[i] & 15];
    }
    output[2 * count] = 0;
}
