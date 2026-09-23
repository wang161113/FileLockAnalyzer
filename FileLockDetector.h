#pragma once
#include "stdafx.h"

struct ProcessLockInfo
{
    DWORD dwPID;
    CString strProcessName;
    CString strProcessPath;
    CString strLockedFile;
    CString strLockType;
    HANDLE hProcess;
};

typedef std::vector<ProcessLockInfo> LockInfoArray;

class CFileLockDetector
{
public:
    CFileLockDetector(void);
    ~CFileLockDetector(void);

    BOOL AnalyzeFile(LPCTSTR lpszFilePath, LockInfoArray& arrLocks);
    BOOL AnalyzeFolder(LPCTSTR lpszFolderPath, LockInfoArray& arrLocks);

    BOOL KillProcess(DWORD dwPID);
    BOOL ForceCloseFileHandle(LPCTSTR lpszFilePath, DWORD dwPID);
    BOOL UnloadModuleFromProcess(LPCTSTR lpszModulePath, DWORD dwPID);
    BOOL DeleteLockedFile(LPCTSTR lpszFilePath);

    static CString GetProcessNameByPID(DWORD dwPID);
    static CString GetProcessPathByPID(DWORD dwPID);
    static HANDLE OpenProcessWithAccess(DWORD dwPID, DWORD dwAccess);

private:
    BOOL AnalyzeByRestartManager(LPCTSTR lpszFilePath, LockInfoArray& arrLocks);
    BOOL AnalyzeByNtQueryInfo(LPCTSTR lpszFilePath, LockInfoArray& arrLocks);
    BOOL EnumerateFolderFiles(LPCTSTR lpszFolderPath, std::vector<CString>& arrFiles);

    BOOL EnableDebugPrivilege();
    BOOL IsFileInsideFolder(LPCTSTR lpszFilePath, LPCTSTR lpszFolderPath);

    typedef NTSTATUS (NTAPI *PFN_NtQuerySystemInformation)(
        SYSTEM_INFORMATION_CLASS SystemInformationClass,
        PVOID SystemInformation,
        ULONG SystemInformationLength,
        PULONG ReturnLength
    );
    typedef NTSTATUS (NTAPI *PFN_NtDuplicateObject)(
        HANDLE SourceProcessHandle,
        HANDLE SourceHandle,
        HANDLE TargetProcessHandle,
        PHANDLE TargetHandle,
        ACCESS_MASK DesiredAccess,
        ULONG Attributes,
        ULONG Options
    );
    typedef NTSTATUS (NTAPI *PFN_NtQueryObject)(
        HANDLE Handle,
        OBJECT_INFORMATION_CLASS ObjectInformationClass,
        PVOID ObjectInformation,
        ULONG ObjectInformationLength,
        PULONG ReturnLength
    );

    PFN_NtQuerySystemInformation m_pfnNtQuerySystemInformation;
    PFN_NtDuplicateObject m_pfnNtDuplicateObject;
    PFN_NtQueryObject m_pfnNtQueryObject;

    BOOL m_bNtApiReady;
};
