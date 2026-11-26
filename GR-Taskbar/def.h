#ifndef _H_DEF_H_
#define _H_DEF_H_
#define APPID L"Microsoft.Windows.Explorer"
#define REGPATH "Software\\GR-Taskbar"
#define REGPATH_OLD "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\GR-Taskbar"
#define REGPATH_STARTMENU REGPATH_OLD
#define SPECIAL_FOLDER CSIDL_PROGRAM_FILES
#define SPECIAL_FOLDER_LEGACY CSIDL_APPDATA
#define PRODUCT_NAME "GR-Taskbar"
#define PRODUCT_DISPLAY_NAME "GR Taskbar"
#define PRODUCT_PUBLISHER "GR Studios AI LLC"
#define PRODUCT_WEBSITE "https://grstudios.shop"
#define APP_RELATIVE_PATH "\\" PRODUCT_NAME
#define EP_CLSID_LITE "D17F1E1A-5919-4427-8F89-A1A8503CA3EB"
#define EP_CLSID "{D17F1E1A-5919-4427-8F89-A1A8503CA3EB}"
#define DOSMODE_OFFSET 78
#ifndef _M_ARM64
#define SETUP_UTILITY_NAME "ep_setup.exe"
#else
#define SETUP_UTILITY_NAME "ep_setup_arm64.exe"
#endif
#define TOAST_BUFSIZ 1536
#define SEH_REGPATH "Control Panel\\Quick Actions\\Control Center\\QuickActionsStateCapture\\GR-Taskbar"
#define EP_SETUP_HELPER_SWITCH "/CreateExplorerShellUnelevatedAfterServicing"
#define EP_DWM_SERVICENAME "ep_dwm_" EP_CLSID_LITE
#define EP_DWM_EVENTNAME "Global\\ep_dwm_2_" EP_CLSID_LITE
#define EP_SETUP_EVENTNAME "Global\\ep_setup_" EP_CLSID_LITE
#endif
