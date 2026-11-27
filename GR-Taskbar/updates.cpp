// GR Studios AI LLC stable-release updater. Required upstream notices remain in LICENSE.
#include "updates.h"
#include <windows.data.json.h>
#include <roapi.h>
#include <Shlwapi.h>
#include <string>
#include <wil/resource.h>
#include <wil/result_macros.h>
#include <wrl/async.h>
#pragma comment(lib, "Shlwapi.lib")
using namespace Microsoft::WRL;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
#include "StableUpdate.h"
DWORD CheckForUpdatesThread(LPVOID params)
{
    return GRStableWorker(params);
}
