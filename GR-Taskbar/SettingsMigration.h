#pragma once
// Original GR Studios AI LLC modifications. Legacy names below are narrowly
// scoped migration inputs, not branding or native Windows namespace rewrites.
static const WCHAR* grLegacySettingsKeys[] = {
    L"Software\\ExplorerPatcher",
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ExplorerPatcher",
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\GR-Taskbar"
};
static const WCHAR* grMigratedSettingsKeys[] = {
    L"Software\\GR-Taskbar", L"Software\\GR-Taskbar\\StartUI", L"Software\\GR-Taskbar\\StartUI"
};
static LONG grMigrationAttempted;
static BOOL grMigrationVerified;

static BOOL GRMigrationSkipValue(PCWSTR name)
{
    return !_wcsicmp(name,L"UpdateURL") || !_wcsicmp(name,L"UpdateURLStaging") ||
        !_wcsicmp(name,L"UpdatePreferStaging") || !_wcsicmp(name,L"UpdateAllowDowngrades") ||
        !_wcsicmp(name,L"UpdateUseLocal");
}
static LSTATUS GRMigrationCopyTree(HKEY source,HKEY target,UINT depth,BOOL missingOnly,DWORD* copied)
{
    if(depth>16)return ERROR_BAD_LENGTH;
    DWORD valueCount=0,maxName=0,maxData=0;
    LSTATUS status=RegQueryInfoKeyW(source,NULL,NULL,NULL,NULL,NULL,NULL,&valueCount,&maxName,&maxData,NULL,NULL);
    if(status)return status;
    if(valueCount>4096||maxName>16383||maxData>1024*1024)return ERROR_BAD_LENGTH;
    WCHAR* name=(WCHAR*)HeapAlloc(GetProcessHeap(),0,(maxName+2)*sizeof(WCHAR));
    BYTE* data=(BYTE*)HeapAlloc(GetProcessHeap(),0,maxData+2*sizeof(WCHAR));
    BYTE* verify=(BYTE*)HeapAlloc(GetProcessHeap(),0,maxData+2*sizeof(WCHAR));
    if(!name||!data||!verify){status=ERROR_NOT_ENOUGH_MEMORY;goto done;}
    for(DWORD i=0;i<valueCount;++i){
        DWORD chars=maxName+1,bytes=maxData,type=0;
        status=RegEnumValueW(source,i,name,&chars,NULL,&type,data,&bytes);if(status)goto done;
        if(missingOnly&&GRMigrationSkipValue(name))continue;
        DWORD existing=0;
        LSTATUS found=RegQueryValueExW(target,name,NULL,NULL,NULL,&existing);
        if(missingOnly&&found==ERROR_SUCCESS)continue;
        if(missingOnly&&found!=ERROR_FILE_NOT_FOUND){status=found;goto done;}
        status=RegSetValueExW(target,name,0,type,data,bytes);if(status)goto done;
        DWORD readBytes=maxData+2*sizeof(WCHAR),readType=0;
        status=RegQueryValueExW(target,name,NULL,&readType,verify,&readBytes);
        if(status||readType!=type||readBytes!=bytes||memcmp(data,verify,bytes)){status=ERROR_CRC;goto done;}
        ++*copied;
    }
    for(DWORD i=0;;++i){
        WCHAR child[256];DWORD chars=ARRAYSIZE(child);
        status=RegEnumKeyExW(source,i,child,&chars,NULL,NULL,NULL,NULL);
        if(status==ERROR_NO_MORE_ITEMS){status=ERROR_SUCCESS;break;}if(status)break;
        HKEY from=NULL,to=NULL;
        status=RegOpenKeyExW(source,child,0,KEY_READ,&from);
        if(!status)status=RegCreateKeyExW(target,child,0,NULL,0,KEY_READ|KEY_WRITE,NULL,&to,NULL);
        if(!status)status=GRMigrationCopyTree(from,to,depth+1,missingOnly,copied);
        if(to)RegCloseKey(to);if(from)RegCloseKey(from);if(status)break;
    }
done:
    if(name)HeapFree(GetProcessHeap(),0,name);if(data)HeapFree(GetProcessHeap(),0,data);if(verify)HeapFree(GetProcessHeap(),0,verify);
    return status;
}
static LSTATUS GRMigrationVerifyBackup(HKEY source,HKEY backup,UINT depth)
{
    if(depth>16)return ERROR_BAD_LENGTH;
    DWORD values=0,maxName=0,maxData=0;
    LSTATUS status=RegQueryInfoKeyW(source,NULL,NULL,NULL,NULL,NULL,NULL,&values,&maxName,&maxData,NULL,NULL);
    if(status)return status;
    if(values>4096||maxName>16383||maxData>1024*1024)return ERROR_BAD_LENGTH;
    WCHAR* name=(WCHAR*)HeapAlloc(GetProcessHeap(),0,(maxName+2)*sizeof(WCHAR));
    BYTE* a=(BYTE*)HeapAlloc(GetProcessHeap(),0,maxData+1),*b=(BYTE*)HeapAlloc(GetProcessHeap(),0,maxData+1);
    if(!name||!a||!b){status=ERROR_NOT_ENOUGH_MEMORY;goto done;}
    for(DWORD i=0;i<values;++i){
        DWORD chars=maxName+1,bytes=maxData,type=0,other=maxData+1,otherType=0;
        status=RegEnumValueW(source,i,name,&chars,NULL,&type,a,&bytes);if(status)goto done;
        status=RegQueryValueExW(backup,name,NULL,&otherType,b,&other);
        if(status||type!=otherType||bytes!=other||memcmp(a,b,bytes)){status=ERROR_CRC;goto done;}
    }
    for(DWORD i=0;;++i){
        WCHAR child[256];DWORD chars=ARRAYSIZE(child);
        status=RegEnumKeyExW(source,i,child,&chars,NULL,NULL,NULL,NULL);
        if(status==ERROR_NO_MORE_ITEMS){status=ERROR_SUCCESS;break;}if(status)break;
        HKEY from=NULL,to=NULL;
        status=RegOpenKeyExW(source,child,0,KEY_READ,&from);
        if(!status)status=RegOpenKeyExW(backup,child,0,KEY_READ,&to);
        if(!status)status=GRMigrationVerifyBackup(from,to,depth+1);
        if(from)RegCloseKey(from);if(to)RegCloseKey(to);if(status)break;
    }
done:
    if(name)HeapFree(GetProcessHeap(),0,name);if(a)HeapFree(GetProcessHeap(),0,a);if(b)HeapFree(GetProcessHeap(),0,b);
    return status;
}
static BOOL GRHasSeparateLegacyInstallation(void)
{
    HKEY root;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",0,KEY_READ|KEY_WOW64_64KEY,&root))return TRUE;
    BOOL separate=FALSE;
    for(DWORD i=0;;++i){WCHAR keyName[256];DWORD n=ARRAYSIZE(keyName);LSTATUS status=RegEnumKeyExW(root,i,keyName,&n,NULL,NULL,NULL,NULL);
        if(status==ERROR_NO_MORE_ITEMS)break;if(status)continue;
        HKEY key;if(RegOpenKeyExW(root,keyName,0,KEY_QUERY_VALUE,&key))continue;
        WCHAR name[128]=L"",location[MAX_PATH]=L"";DWORD bytes=sizeof(name);
        RegGetValueW(key,NULL,L"DisplayName",RRF_RT_REG_SZ,NULL,name,&bytes);bytes=sizeof(location);
        RegGetValueW(key,NULL,L"InstallLocation",RRF_RT_REG_SZ,NULL,location,&bytes);RegCloseKey(key);
        if(!_wcsicmp(name,L"ExplorerPatcher")&&(!location[0]||!wcsstr(location,L"GR-Taskbar"))){separate=TRUE;break;}
    }
    RegCloseKey(root);return separate;
}
static BOOL GRMigrateProductSettings(BOOL registryReadersRedirected)
{
    if(!registryReadersRedirected&&InterlockedCompareExchange(&grMigrationAttempted,1,0))return grMigrationVerified;
    DWORD copied=0;LSTATUS status=ERROR_SUCCESS;BOOL cleanup=registryReadersRedirected&&!GRHasSeparateLegacyInstallation();
    for(UINT i=0;i<ARRAYSIZE(grLegacySettingsKeys);++i){
        HKEY source=NULL,target=NULL,backup=NULL;WCHAR backupPath[160];
        status=RegOpenKeyExW(HKEY_CURRENT_USER,grLegacySettingsKeys[i],0,KEY_READ,&source);
        if(status==ERROR_FILE_NOT_FOUND){status=ERROR_SUCCESS;continue;}if(status)break;
        swprintf_s(backupPath,ARRAYSIZE(backupPath),L"Software\\GR-Taskbar\\MigrationBackup\\Source%u",i);
        status=RegCreateKeyExW(HKEY_CURRENT_USER,backupPath,0,NULL,0,KEY_READ|KEY_WRITE,NULL,&backup,NULL);
        DWORD backed=0;
        if(!status)status=GRMigrationCopyTree(source,backup,0,FALSE,&backed);
        if(!status)status=GRMigrationVerifyBackup(source,backup,0);
        if(!status)status=RegCreateKeyExW(HKEY_CURRENT_USER,grMigratedSettingsKeys[i],0,NULL,0,KEY_READ|KEY_WRITE,NULL,&target,NULL);
        if(!status)status=GRMigrationCopyTree(source,target,0,TRUE,&copied);
        if(!status)status=GRMigrationVerifyBackup(source,backup,0);
        if(source)RegCloseKey(source);if(target)RegCloseKey(target);if(backup)RegCloseKey(backup);
        if(status)break;
        if(cleanup){status=RegDeleteTreeW(HKEY_CURRENT_USER,grLegacySettingsKeys[i]);if(status==ERROR_FILE_NOT_FOUND)status=ERROR_SUCCESS;if(status)break;}
    }
    DWORD complete=status==ERROR_SUCCESS,pending=!cleanup;
    RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\GR-Taskbar\\Migration",L"Version",REG_DWORD,&complete,sizeof(complete));
    RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\GR-Taskbar\\Migration",L"CopiedValues",REG_DWORD,&copied,sizeof(copied));
    RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\GR-Taskbar\\Migration",L"CleanupPending",REG_DWORD,&pending,sizeof(pending));
    RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\GR-Taskbar\\Migration",L"NativeError",REG_DWORD,&status,sizeof(status));
    grMigrationVerified=complete;return complete;
}

static PCWSTR GRMapLegacyProductKey(PCWSTR subkey,WCHAR mapped[1024])
{
    if(!subkey)return subkey;
    for(UINT i=0;i<ARRAYSIZE(grLegacySettingsKeys);++i){SIZE_T length=wcslen(grLegacySettingsKeys[i]);
        if(!_wcsnicmp(subkey,grLegacySettingsKeys[i],length)&&(!subkey[length]||subkey[length]==L'\\')){
            if(swprintf_s(mapped,1024,L"%s%s",grMigratedSettingsKeys[i],subkey+length)>=0)return mapped;
            return subkey;
        }
    }
    return subkey;
}
