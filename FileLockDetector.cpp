#include "stdafx.h"
#include "FileLockDetector.h"

#define SystemExtendedHandleInformation 64
#define ObjectNameInformation 1
#define ObjectTypeInformation 2

typedef struct _MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
    PVOID Object;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR HandleValue;
    ULONG GrantedAccess;
    USHORT CreatorBackTraceIndex;
    USHORT ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
} MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PMY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _MY_SYSTEM_HANDLE_INFORMATION_EX
{
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} MY_SYSTEM_HANDLE_INFORMATION_EX, *PMY_SYSTEM_HANDLE_INFORMATION_EX;

#ifndef _UNICODESTRING_DEFINED
#define _UNICODESTRING_DEFINED
typedef struct _MY_UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} MY_UNICODE_STRING, *PMY_UNICODE_STRING;
#endif

typedef struct _MY_OBJECT_NAME_INFORMATION
{
    MY_UNICODE_STRING Name;
} MY_OBJECT_NAME_INFORMATION, *PMY_OBJECT_NAME_INFORMATION;

typedef struct _MY_OBJECT_TYPE_INFORMATION
{
    MY_UNICODE_STRING TypeName;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
    ULONG TotalPagedPoolUsage;
    ULONG TotalNonPagedPoolUsage;
    ULONG TotalNamePoolUsage;
    ULONG TotalHandleTableUsage;
    ULONG HighWaterNumberOfObjects;
    ULONG HighWaterNumberOfHandles;
    ULONG HighWaterPagedPoolUsage;
    ULONG HighWaterNonPagedPoolUsage;
    ULONG HighWaterNamePoolUsage;
    ULONG HighWaterHandleTableUsage;
    ULONG InvalidAttributes;
    GENERIC_MAPPING GenericMapping;
    ULONG ValidAccess;
    BOOLEAN SecurityRequired;
    BOOLEAN MaintainHandleCount;
    USHORT MaintainTypeList;
    ULONG PoolType;
    ULONG PagedPoolUsage;
    ULONG NonPagedPoolUsage;
} MY_OBJECT_TYPE_INFORMATION, *PMY_OBJECT_TYPE_INFORMATION;

CFileLockDetector::CFileLockDetector(void)
    : m_pfnNtQuerySystemInformation(NULL)
    , m_pfnNtDuplicateObject(NULL)
    , m_pfnNtQueryObject(NULL)
    , m_bNtApiReady(FALSE)
{
    HMODULE hNtDll = ::GetModuleHandle(_T("ntdll.dll"));
    if (hNtDll)
    {
        m_pfnNtQuerySystemInformation = (PFN_NtQuerySystemInformation)
            ::GetProcAddress(hNtDll, "NtQuerySystemInformation");
        m_pfnNtDuplicateObject = (PFN_NtDuplicateObject)
            ::GetProcAddress(hNtDll, "NtDuplicateObject");
        m_pfnNtQueryObject = (PFN_NtQueryObject)
            ::GetProcAddress(hNtDll, "NtQueryObject");

        if (m_pfnNtQuerySystemInformation && m_pfnNtDuplicateObject && m_pfnNtQueryObject)
        {
            m_bNtApiReady = TRUE;
        }
    }
    EnableDebugPrivilege();
}

CFileLockDetector::~CFileLockDetector(void)
{
}

BOOL CFileLockDetector::EnableDebugPrivilege()
{
    HANDLE hToken = NULL;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;

    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
    {
        ::CloseHandle(hToken);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
    {
        ::CloseHandle(hToken);
        return FALSE;
    }

    ::CloseHandle(hToken);
    return (::GetLastError() == ERROR_SUCCESS);
}

CString CFileLockDetector::GetProcessNameByPID(DWORD dwPID)
{
    CString strName;
    HANDLE hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return strName;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (::Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (pe32.th32ProcessID == dwPID)
            {
                strName = pe32.szExeFile;
                break;
            }
        } while (::Process32Next(hSnapshot, &pe32));
    }
    ::CloseHandle(hSnapshot);
    return strName;
}

CString CFileLockDetector::GetProcessPathByPID(DWORD dwPID)
{
    CString strPath;
    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
    if (hProcess)
    {
        TCHAR szPath[MAX_PATH] = {0};
        DWORD dwSize = MAX_PATH;
        if (::QueryFullProcessImageName(hProcess, 0, szPath, &dwSize))
        {
            strPath = szPath;
        }
        ::CloseHandle(hProcess);
    }
    return strPath;
}

HANDLE CFileLockDetector::OpenProcessWithAccess(DWORD dwPID, DWORD dwAccess)
{
    return ::OpenProcess(dwAccess, FALSE, dwPID);
}

BOOL CFileLockDetector::IsFileInsideFolder(LPCTSTR lpszFilePath, LPCTSTR lpszFolderPath)
{
    if (!lpszFilePath || !lpszFolderPath)
        return FALSE;

    CString strFile(lpszFilePath);
    CString strFolder(lpszFolderPath);
    strFile.MakeLower();
    strFolder.MakeLower();

    if (strFolder.Right(1) != _T("\\"))
        strFolder += _T("\\");

    return (strFile.Find(strFolder) == 0);
}

BOOL CFileLockDetector::EnumerateFolderFiles(LPCTSTR lpszFolderPath, std::vector<CString>& arrFiles)
{
    if (!lpszFolderPath)
        return FALSE;

    CString strSearchPath(lpszFolderPath);
    if (strSearchPath.Right(1) != _T("\\"))
        strSearchPath += _T("\\");
    strSearchPath += _T("*.*");

    WIN32_FIND_DATA findData;
    HANDLE hFind = ::FindFirstFile(strSearchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return FALSE;

    CString strFolder(lpszFolderPath);
    if (strFolder.Right(1) != _T("\\"))
        strFolder += _T("\\");

    BOOL bAddedAny = FALSE;
    do
    {
        if (_tcscmp(findData.cFileName, _T(".")) == 0 || _tcscmp(findData.cFileName, _T("..")) == 0)
            continue;

        CString strFullPath = strFolder + findData.cFileName;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (EnumerateFolderFiles(strFullPath, arrFiles))
                bAddedAny = TRUE;
        }
        else
        {
            arrFiles.push_back(strFullPath);
            bAddedAny = TRUE;
        }
    } while (::FindNextFile(hFind, &findData));

    ::FindClose(hFind);
    return bAddedAny;
}

BOOL CFileLockDetector::AnalyzeFile(LPCTSTR lpszFilePath, LockInfoArray& arrLocks)
{
    if (!lpszFilePath)
        return FALSE;

    arrLocks.clear();

    AnalyzeByRestartManager(lpszFilePath, arrLocks);
    if (m_bNtApiReady)
    {
        AnalyzeByNtQueryInfo(lpszFilePath, arrLocks);
    }

    return TRUE;
}

BOOL CFileLockDetector::AnalyzeFolder(LPCTSTR lpszFolderPath, LockInfoArray& arrLocks)
{
    if (!lpszFolderPath)
        return FALSE;

    arrLocks.clear();

    std::vector<CString> arrFiles;
    BOOL bEnumOk = EnumerateFolderFiles(lpszFolderPath, arrFiles);

    if (bEnumOk)
    {
        std::map<DWORD, size_t> mapPidToIdx;
        for (size_t i = 0; i < arrFiles.size(); i++)
        {
            LockInfoArray arrTemp;
            AnalyzeByRestartManager(arrFiles[i], arrTemp);
            for (size_t j = 0; j < arrTemp.size(); j++)
            {
                DWORD pid = arrTemp[j].dwPID;
                auto it = mapPidToIdx.find(pid);
                if (it == mapPidToIdx.end())
                {
                    mapPidToIdx[pid] = arrLocks.size();
                    arrLocks.push_back(arrTemp[j]);
                }
                else
                {
                    ProcessLockInfo& existing = arrLocks[it->second];
                    if (existing.strLockedFile.GetLength() < 160 &&
                        existing.strLockedFile.Find(arrTemp[j].strLockedFile) < 0)
                    {
                        existing.strLockedFile += _T("; ") + arrTemp[j].strLockedFile;
                    }
                }
            }
        }
    }

    if (m_bNtApiReady)
    {
        AnalyzeByNtQueryInfo(lpszFolderPath, arrLocks);
    }

    return bEnumOk || m_bNtApiReady;
}

BOOL CFileLockDetector::AnalyzeByRestartManager(LPCTSTR lpszFilePath, LockInfoArray& arrLocks)
{
    DWORD dwSession = 0;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = {0};
    DWORD dwResult = ::RmStartSession(&dwSession, 0, szSessionKey);
    if (dwResult != ERROR_SUCCESS)
        return FALSE;

    dwResult = ::RmRegisterResources(dwSession, 1, &lpszFilePath, 0, NULL, 0, NULL);
    if (dwResult != ERROR_SUCCESS)
    {
        ::RmEndSession(dwSession);
        return FALSE;
    }

    DWORD dwBytesNeeded = 0;
    UINT nProcInfoNeeded = 0;
    UINT nProcInfo = 0;
    RM_PROCESS_INFO* pProcInfo = NULL;
    DWORD dwRebootReasons = RmRebootReasonNone;

    dwResult = ::RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, pProcInfo, &dwRebootReasons);
    if (dwResult == ERROR_MORE_DATA)
    {
        pProcInfo = (RM_PROCESS_INFO*)new BYTE[sizeof(RM_PROCESS_INFO) * nProcInfoNeeded];
        nProcInfo = nProcInfoNeeded;
        dwResult = ::RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, pProcInfo, &dwRebootReasons);
    }

    if (dwResult == ERROR_SUCCESS)
    {
        for (UINT i = 0; i < nProcInfo; i++)
        {
            ProcessLockInfo info;
            info.dwPID = pProcInfo[i].Process.dwProcessId;
            info.strProcessName = pProcInfo[i].strAppName;
            info.strLockedFile = lpszFilePath;
            info.strLockType = _T("RM_API");
            info.strProcessPath = GetProcessPathByPID(info.dwPID);
            info.hProcess = NULL;
            arrLocks.push_back(info);
        }
    }

    if (pProcInfo)
        delete[] (BYTE*)pProcInfo;

    ::RmEndSession(dwSession);
    return TRUE;
}

BOOL CFileLockDetector::AnalyzeByNtQueryInfo(LPCTSTR lpszPath, LockInfoArray& arrLocks)
{
    if (!m_bNtApiReady || !lpszPath)
        return FALSE;

    ULONG_PTR ulBufSize = 0x200000;
    PMY_SYSTEM_HANDLE_INFORMATION_EX pHandleInfo = NULL;
    NTSTATUS status;

    do
    {
        if (pHandleInfo)
            delete[] (BYTE*)pHandleInfo;
        pHandleInfo = (PMY_SYSTEM_HANDLE_INFORMATION_EX)new (std::nothrow) BYTE[(size_t)ulBufSize];
        if (!pHandleInfo)
            return FALSE;
        status = m_pfnNtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemExtendedHandleInformation,
            pHandleInfo, (ULONG)ulBufSize, (PULONG)&ulBufSize);
    } while (status == 0xC0000004 && ulBufSize < 0x8000000ULL);

    if (status != 0 || !pHandleInfo)
    {
        if (pHandleInfo) delete[] (BYTE*)pHandleInfo;
        return FALSE;
    }

    const ULONG_PTR cbHeader = FIELD_OFFSET(MY_SYSTEM_HANDLE_INFORMATION_EX, Handles[0]);
    const ULONG_PTR cbEntry = sizeof(MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);

    ULONG_PTR nHandles = pHandleInfo->NumberOfHandles;
    if (nHandles > 4000000ULL || nHandles * cbEntry + cbHeader > ulBufSize)
    {
        delete[] (BYTE*)pHandleInfo;
        return FALSE;
    }

    CString strTargetPath(lpszPath);
    if (strTargetPath.IsEmpty())
    {
        delete[] (BYTE*)pHandleInfo;
        return FALSE;
    }
    strTargetPath.MakeLower();

    CString strTargetNtPath;
    TCHAR szDeviceName[MAX_PATH] = {0};
    TCHAR szDrive[3] = {0};
    szDrive[0] = strTargetPath[0];
    szDrive[1] = _T(':');
    szDrive[2] = _T('\0');
    if (::QueryDosDevice(szDrive, szDeviceName, MAX_PATH))
    {
        strTargetNtPath = strTargetPath;
        strTargetNtPath.Replace(strTargetPath.Left(2), szDeviceName);
    }

    std::set<DWORD> setPIDAdded;
    for (size_t i = 0; i < arrLocks.size(); i++)
        setPIDAdded.insert(arrLocks[i].dwPID);

    BOOL bIsDir = PathIsDirectory(lpszPath);
    BOOL bRet = FALSE;

    for (ULONG_PTR i = 0; i < nHandles; i++)
    {
        PMY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX pEntry = &pHandleInfo->Handles[i];

        if (pEntry->ObjectTypeIndex < 28 || pEntry->ObjectTypeIndex > 40)
            continue;

        DWORD dwPID = (DWORD)pEntry->UniqueProcessId;
        if (dwPID == 0 || dwPID == 4)
            continue;

        HANDLE hProcess = ::OpenProcess(PROCESS_DUP_HANDLE | PROCESS_VM_READ, FALSE, dwPID);
        if (!hProcess)
            continue;

        HANDLE hDupHandle = NULL;
        status = m_pfnNtDuplicateObject(hProcess, (HANDLE)pEntry->HandleValue,
            ::GetCurrentProcess(), &hDupHandle, 0, 0, 0);
        if (status != 0 || !hDupHandle)
        {
            ::CloseHandle(hProcess);
            continue;
        }

        const ULONG ulNameLen = 0x2000;
        MY_OBJECT_NAME_INFORMATION* pNameInfo = (MY_OBJECT_NAME_INFORMATION*)new (std::nothrow) BYTE[ulNameLen];
        if (pNameInfo)
        {
            ULONG ulUsed = 0;
            status = m_pfnNtQueryObject(hDupHandle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
                pNameInfo, ulNameLen, &ulUsed);

            if (status == 0 && pNameInfo->Name.Buffer && pNameInfo->Name.Length > 0
                && pNameInfo->Name.Length <= (USHORT)(ulNameLen - sizeof(MY_UNICODE_STRING)))
            {
                CString strObjName;
                int nChars = pNameInfo->Name.Length / sizeof(WCHAR);
                LPWSTR pBuf = strObjName.GetBufferSetLength(nChars);
                ::memcpy(pBuf, pNameInfo->Name.Buffer, pNameInfo->Name.Length);
                strObjName.ReleaseBufferSetLength(nChars);

                CString strObjNameLower = strObjName;
                strObjNameLower.MakeLower();

                BOOL bMatch = FALSE;
                if (bIsDir)
                {
                    if (IsFileInsideFolder(strObjName, strTargetNtPath))
                        bMatch = TRUE;
                }
                else
                {
                    if (strObjNameLower == strTargetNtPath ||
                        (!strTargetNtPath.IsEmpty() && strObjNameLower.Find(strTargetNtPath) >= 0))
                    {
                        bMatch = TRUE;
                    }
                }

                if (bMatch && setPIDAdded.find(dwPID) == setPIDAdded.end())
                {
                    ProcessLockInfo info;
                    info.dwPID = dwPID;
                    info.strProcessName = GetProcessNameByPID(info.dwPID);
                    info.strProcessPath = GetProcessPathByPID(info.dwPID);
                    info.strLockedFile = strObjName;
                    info.strLockType = _T("Handle");
                    info.hProcess = NULL;
                    arrLocks.push_back(info);
                    setPIDAdded.insert(dwPID);
                    bRet = TRUE;
                }
            }
            delete[] (BYTE*)pNameInfo;
        }

        ::CloseHandle(hDupHandle);
        ::CloseHandle(hProcess);
    }

    delete[] (BYTE*)pHandleInfo;
    return bRet;
}

BOOL CFileLockDetector::KillProcess(DWORD dwPID)
{
    if (dwPID == ::GetCurrentProcessId())
        return FALSE;

    HANDLE hProcess = ::OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, dwPID);
    if (!hProcess)
    {
        EnableDebugPrivilege();
        hProcess = ::OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, dwPID);
        if (!hProcess)
            return FALSE;
    }

    BOOL bResult = ::TerminateProcess(hProcess, 0);
    ::CloseHandle(hProcess);
    return bResult;
}

BOOL CFileLockDetector::ForceCloseFileHandle(LPCTSTR lpszFilePath, DWORD dwPID)
{
    if (!m_bNtApiReady || !lpszFilePath)
        return FALSE;

    ULONG_PTR ulBufSize = 0x200000;
    PMY_SYSTEM_HANDLE_INFORMATION_EX pHandleInfo = NULL;
    NTSTATUS status;

    do
    {
        if (pHandleInfo)
            delete[] (BYTE*)pHandleInfo;
        pHandleInfo = (PMY_SYSTEM_HANDLE_INFORMATION_EX)new (std::nothrow) BYTE[(size_t)ulBufSize];
        if (!pHandleInfo)
            return FALSE;
        status = m_pfnNtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemExtendedHandleInformation,
            pHandleInfo, (ULONG)ulBufSize, (PULONG)&ulBufSize);
    } while (status == 0xC0000004 && ulBufSize < 0x8000000ULL);

    if (status != 0)
    {
        if (pHandleInfo) delete[] (BYTE*)pHandleInfo;
        return FALSE;
    }

    const ULONG_PTR cbHeader = FIELD_OFFSET(MY_SYSTEM_HANDLE_INFORMATION_EX, Handles[0]);
    const ULONG_PTR cbEntry = sizeof(MY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);
    ULONG_PTR nHandles = pHandleInfo->NumberOfHandles;
    if (nHandles > 4000000ULL || nHandles * cbEntry + cbHeader > ulBufSize)
    {
        delete[] (BYTE*)pHandleInfo;
        return FALSE;
    }

    CString strTargetPath(lpszFilePath);
    strTargetPath.MakeLower();

    CString strTargetNtPath;
    TCHAR szDeviceName[MAX_PATH] = {0};
    TCHAR szDrive[3] = {0};
    szDrive[0] = strTargetPath[0];
    szDrive[1] = _T(':');
    szDrive[2] = _T('\0');
    if (::QueryDosDevice(szDrive, szDeviceName, MAX_PATH))
    {
        strTargetNtPath = strTargetPath;
        strTargetNtPath.Replace(strTargetPath.Left(2), szDeviceName);
    }

    BOOL bResult = FALSE;

    for (ULONG_PTR i = 0; i < nHandles; i++)
    {
        PMY_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX pEntry = &pHandleInfo->Handles[i];
        if ((DWORD)pEntry->UniqueProcessId != dwPID)
            continue;
        if (pEntry->ObjectTypeIndex < 28 || pEntry->ObjectTypeIndex > 40)
            continue;

        HANDLE hProcess = ::OpenProcess(PROCESS_DUP_HANDLE | PROCESS_VM_READ, FALSE, dwPID);
        if (!hProcess)
            continue;

        HANDLE hDupHandle = NULL;
        status = m_pfnNtDuplicateObject(hProcess, (HANDLE)pEntry->HandleValue,
            ::GetCurrentProcess(), &hDupHandle, 0, 0, 0);
        if (status != 0 || !hDupHandle)
        {
            ::CloseHandle(hProcess);
            continue;
        }

        const ULONG ulNameLen = 0x2000;
        MY_OBJECT_NAME_INFORMATION* pNameInfo = (MY_OBJECT_NAME_INFORMATION*)new (std::nothrow) BYTE[ulNameLen];
        if (pNameInfo)
        {
            ULONG ulUsed = 0;
            status = m_pfnNtQueryObject(hDupHandle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
                pNameInfo, ulNameLen, &ulUsed);

            if (status == 0 && pNameInfo->Name.Buffer && pNameInfo->Name.Length > 0
                && pNameInfo->Name.Length <= (USHORT)(ulNameLen - sizeof(MY_UNICODE_STRING)))
            {
                CString strObjName;
                int nChars = pNameInfo->Name.Length / sizeof(WCHAR);
                LPWSTR pBuf = strObjName.GetBufferSetLength(nChars);
                ::memcpy(pBuf, pNameInfo->Name.Buffer, pNameInfo->Name.Length);
                strObjName.ReleaseBufferSetLength(nChars);
                strObjName.MakeLower();

                if (strObjName.Find(strTargetNtPath) >= 0)
                {
                    HANDLE hProcessForClose = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwPID);
                    if (hProcessForClose)
                    {
                        ::DuplicateHandle(hProcessForClose, (HANDLE)pEntry->HandleValue,
                            NULL, NULL, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
                        ::CloseHandle(hProcessForClose);
                        bResult = TRUE;
                    }
                }
            }
            delete[] (BYTE*)pNameInfo;
        }

        ::CloseHandle(hDupHandle);
        ::CloseHandle(hProcess);
    }

    delete[] (BYTE*)pHandleInfo;
    return bResult;
}

BOOL CFileLockDetector::UnloadModuleFromProcess(LPCTSTR lpszModulePath, DWORD dwPID)
{
    if (!lpszModulePath)
        return FALSE;

    HANDLE hProcess = ::OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, dwPID);
    if (!hProcess)
    {
        EnableDebugPrivilege();
        hProcess = ::OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, dwPID);
        if (!hProcess)
            return FALSE;
    }

    HMODULE* phModules = NULL;
    DWORD cbNeeded = 0;
    if (!::EnumProcessModulesEx(hProcess, NULL, 0, &cbNeeded, LIST_MODULES_ALL))
    {
        ::CloseHandle(hProcess);
        return FALSE;
    }

    phModules = (HMODULE*)new BYTE[cbNeeded];
    if (!::EnumProcessModulesEx(hProcess, phModules, cbNeeded, &cbNeeded, LIST_MODULES_ALL))
    {
        delete[] (BYTE*)phModules;
        ::CloseHandle(hProcess);
        return FALSE;
    }

    BOOL bResult = FALSE;
    CString strTargetMod(lpszModulePath);
    strTargetMod.MakeLower();

    DWORD nModules = cbNeeded / sizeof(HMODULE);
    for (DWORD i = 0; i < nModules; i++)
    {
        TCHAR szModPath[MAX_PATH] = {0};
        if (::GetModuleFileNameEx(hProcess, phModules[i], szModPath, MAX_PATH))
        {
            CString strModPath(szModPath);
            strModPath.MakeLower();
            if (strModPath == strTargetMod || strModPath.Find(strTargetMod) >= 0)
            {
                LPTHREAD_START_ROUTINE pfnFreeLib = (LPTHREAD_START_ROUTINE)::GetProcAddress(
                    ::GetModuleHandle(_T("kernel32.dll")), "FreeLibrary");
                HANDLE hThread = ::CreateRemoteThread(hProcess, NULL, 0, pfnFreeLib,
                    phModules[i], 0, NULL);
                if (hThread)
                {
                    ::WaitForSingleObject(hThread, 5000);
                    ::CloseHandle(hThread);
                    bResult = TRUE;
                    break;
                }
            }
        }
    }

    delete[] (BYTE*)phModules;
    ::CloseHandle(hProcess);
    return bResult;
}

BOOL CFileLockDetector::DeleteLockedFile(LPCTSTR lpszFilePath)
{
    if (!lpszFilePath)
        return FALSE;

    LockInfoArray arrLocks;
    AnalyzeFile(lpszFilePath, arrLocks);

    for (size_t i = 0; i < arrLocks.size(); i++)
    {
        if (arrLocks[i].dwPID != ::GetCurrentProcessId())
        {
            ForceCloseFileHandle(lpszFilePath, arrLocks[i].dwPID);
        }
    }

    ::Sleep(100);

    if (::DeleteFile(lpszFilePath))
        return TRUE;

    if (::GetLastError() == ERROR_ACCESS_DENIED || ::GetLastError() == ERROR_SHARING_VIOLATION)
    {
        TCHAR szTemp[MAX_PATH] = {0};
        ::GetTempPath(MAX_PATH, szTemp);
        ::PathRemoveFileSpec(szTemp);
        CString strMoveTo = CString(szTemp) + _T("\\$DelTmp_") + ::PathFindFileName(lpszFilePath);
        if (::MoveFileEx(lpszFilePath, strMoveTo, MOVEFILE_REPLACE_EXISTING))
        {
            ::MoveFileEx(strMoveTo, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            return TRUE;
        }
        return ::MoveFileEx(lpszFilePath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }

    return FALSE;
}
