#include "stdafx.h"
#include "FileUsageAnalyzer.h"
#include "MainDlg.h"
#include <shellapi.h>

UINT g_msgIsInstance   = 0;
UINT g_msgShowInstance = 0;

CFileUsageAnalyzerApp theApp;

static void TraceF(LPCTSTR lpszFmt, ...)
{
    va_list args;
    va_start(args, lpszFmt);
    TCHAR szBuf[512];
    _vstprintf_s(szBuf, lpszFmt, args);
    va_end(args);
    ::OutputDebugString(_T("[FUA] "));
    ::OutputDebugString(szBuf);
    ::OutputDebugString(_T("\n"));
}

struct SEnumCtx { HWND hFound; };

static bool IsOurProcessWindow(HWND hWnd)
{
    DWORD dwPID = 0;
    ::GetWindowThreadProcessId(hWnd, &dwPID);
    if (dwPID == 0) return false;
    HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPID);
    if (!hProc) return false;
    TCHAR szPath[MAX_PATH] = { 0 };
    DWORD dwSize = _countof(szPath);
    BOOL bOk = ::QueryFullProcessImageName(hProc, 0, szPath, &dwSize);
    ::CloseHandle(hProc);
    if (!bOk) return false;
    LPCTSTR pName = _tcsrchr(szPath, _T('\\'));
    if (pName) pName++; else pName = szPath;
    return (_tcsicmp(pName, _T("FileUsageAnalyzer.exe")) == 0);
}

static bool IsCurrentProcessAdmin()
{
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup = NULL;
    if (!::AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &AdministratorsGroup))
    {
        return false;
    }
    BOOL bMember = FALSE;
    if (!::CheckTokenMembership(NULL, AdministratorsGroup, &bMember))
        bMember = FALSE;
    ::FreeSid(AdministratorsGroup);
    return (bMember != FALSE);
}

static bool IsProcessAdminByHwnd(HWND hWnd)
{
    if (!hWnd) return false;
    DWORD dwPID = 0;
    ::GetWindowThreadProcessId(hWnd, &dwPID);
    if (dwPID == 0) return false;
    HANDLE hProc = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPID);
    if (!hProc) return false;
    HANDLE hToken = NULL;
    BOOL bOk = ::OpenProcessToken(hProc, TOKEN_QUERY, &hToken);
    ::CloseHandle(hProc);
    if (!bOk || !hToken) return false;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup = NULL;
    BOOL bMember = FALSE;
    if (::AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &AdministratorsGroup))
    {
        if (!::CheckTokenMembership(hToken, AdministratorsGroup, &bMember))
            bMember = FALSE;
        ::FreeSid(AdministratorsGroup);
    }
    ::CloseHandle(hToken);
    return (bMember != FALSE);
}

static BOOL CALLBACK EnumFindInstanceProc(HWND hWnd, LPARAM lParam)
{
    SEnumCtx* pCtx = (SEnumCtx*)lParam;
    TCHAR szCls[64] = { 0 };
    if (::GetClassName(hWnd, szCls, _countof(szCls)) > 0)
    {
        if (::_tcscmp(szCls, MAIN_WND_CLASS_NAME) == 0)
        {
            if (::GetParent(hWnd) == NULL)
            {
                if (IsOurProcessWindow(hWnd))
                {
                    pCtx->hFound = hWnd;
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

CFileUsageAnalyzerApp::CFileUsageAnalyzerApp()
    : m_hMutexSingle(NULL)
{
}

bool CFileUsageAnalyzerApp::EnsureSingleInstance(HWND* phFoundPrevWnd, CString& outPathArg)
{
    outPathArg.Empty();
    if (phFoundPrevWnd) *phFoundPrevWnd = NULL;

    CString strCmdLine = ::GetCommandLine();
    TraceF(_T("Start. cmdline=%s"), strCmdLine);

    if (!g_msgIsInstance)
        g_msgIsInstance   = ::RegisterWindowMessage(_T("FileUsageAnalyzer.IsInstance"));
    if (!g_msgShowInstance)
        g_msgShowInstance = ::RegisterWindowMessage(_T("FileUsageAnalyzer.ShowInstance"));

    // 1) 一次性管理命令（/shellregister /shellunregister）绝对白名单，不参与任何单实例检测
    bool bShellCmd = false;
    int nArgs0 = 0;
    LPWSTR* pArgs0 = ::CommandLineToArgvW(::GetCommandLineW(), &nArgs0);
    if (pArgs0)
    {
        for (int i = 1; i < nArgs0; ++i)
        {
            CString a = pArgs0[i];
            if (a.CompareNoCase(_T("/shellregister"))   == 0 ||
                a.CompareNoCase(_T("/shellunregister")) == 0)
            {
                bShellCmd = true;
                break;
            }
        }
        ::LocalFree(pArgs0);
    }
    if (bShellCmd)
    {
        TraceF(_T("shell-register flag, bypass single-instance check."));
        return true;
    }

    // 2) 统一解析命令行中的目标路径（可能来自 shell 右键 %1 / %V）
    int nArgs = 0;
    LPWSTR* pArgs = ::CommandLineToArgvW(::GetCommandLineW(), &nArgs);
    if (pArgs)
    {
        for (int i = 1; i < nArgs; ++i)
        {
            CString arg = pArgs[i];
            if (arg.GetLength() <= 2) continue;
            if (arg[0] == _T('/') || arg[0] == _T('-')) continue;
            outPathArg = arg;
            break;
        }
        ::LocalFree(pArgs);
    }

    // 3) [最高优先级] HWND级去重：通过专属窗口类名定位老实例 —— 不依赖Mutex，无视Session/权限问题
    HWND hFound = ::FindWindow(MAIN_WND_CLASS_NAME, NULL);
    TraceF(_T("FindWindow(%s)=%p"), MAIN_WND_CLASS_NAME, hFound);
    if (hFound != NULL && !IsOurProcessWindow(hFound))
    {
        TraceF(_T("FindWindow hit=%p but NOT our process (IsOurProcessWindow=false). Ignoring false hit."), hFound);
        hFound = NULL;
    }
    if (hFound == NULL)
    {
        SEnumCtx ctx; ctx.hFound = NULL;
        ::EnumWindows(EnumFindInstanceProc, (LPARAM)&ctx);
        hFound = ctx.hFound;
        TraceF(_T("EnumWindows fallback hFound=%p"), hFound);
    }

    if (hFound != NULL)
    {
        bool bNewIsAdmin = IsCurrentProcessAdmin();
        bool bOldIsAdmin = IsProcessAdminByHwnd(hFound);
        TraceF(_T("old instance found: hwnd=%p, path=%s, new_admin=%d, old_admin=%d"),
            hFound, outPathArg.IsEmpty() ? _T("(none)") : outPathArg.GetString(),
            bNewIsAdmin ? 1 : 0, bOldIsAdmin ? 1 : 0);

        // 权限升级：新实例是管理员、老实例不是管理员 → 不拦截新实例，允许并存
        // （用户特意右键"管理员身份运行"就是想要管理员权限，老实例没权限不能替代）
        if (bNewIsAdmin && !bOldIsAdmin)
        {
            TraceF(_T("new instance elevated (admin), old instance not elevated. Allow new instance start side-by-side, skip block."));
            if (m_hMutexSingle)
            {
                ::CloseHandle(m_hMutexSingle);
                m_hMutexSingle = NULL;
            }
            return true;
        }

        if (!outPathArg.IsEmpty())
        {
            COPYDATASTRUCT cds;
            cds.dwData = SINGLE_INST_COPYDATA_PATH;
            cds.cbData = (DWORD)((outPathArg.GetLength() + 1) * sizeof(TCHAR));
            cds.lpData = (PVOID)(LPCTSTR)outPathArg;
            DWORD_PTR lResult = 0;
            LRESULT lRes = ::SendMessageTimeout(hFound, WM_COPYDATA, 0, (LPARAM)&cds,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &lResult);
            TraceF(_T("WM_COPYDATA send=%p lResult=%llu"), (void*)lRes, (ULONGLONG)lResult);
        }
        ::SendMessageTimeout(hFound, g_msgShowInstance, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, NULL);
        if (::IsIconic(hFound))
            ::ShowWindow(hFound, SW_RESTORE);
        else if (!::IsWindowVisible(hFound))
            ::ShowWindow(hFound, SW_SHOW);
        ::SetForegroundWindow(hFound);
        ::BringWindowToTop(hFound);
        if (phFoundPrevWnd) *phFoundPrevWnd = hFound;
        TraceF(_T("blocking new instance (old hwnd take over)."));
        return false;
    }

    // 4) HWND级未找到 → 再用Mutex做二次判定（防并发启动、老实例挂起但Mutex残留等场景）
    m_hMutexSingle = ::CreateMutex(NULL, FALSE, SINGLE_INST_MUTEX_NAME);
    DWORD dwErr = ::GetLastError();
    if (m_hMutexSingle && dwErr != ERROR_ALREADY_EXISTS)
    {
        TraceF(_T("no mutex + no hwnd: first instance, allow DoModal."));
        return true;
    }

    // 5) Mutex已存在但HWND仍找不到 = 老实例挂起 / Mutex残留 / 其他异常  → 兜底：启动新实例
    TraceF(_T("mutex exists but no HWND (old hung / mutex leak). Fallback: allow new instance start. Never silent exit."));
    if (m_hMutexSingle)
    {
        ::CloseHandle(m_hMutexSingle);
        m_hMutexSingle = NULL;
    }
    return true;
}

BOOL CFileUsageAnalyzerApp::InitInstance()
{
    if (!CWinApp::InitInstance())
        return FALSE;

    CString strPath;
    HWND hPrev = NULL;
    if (!EnsureSingleInstance(&hPrev, strPath))
    {
        TraceF(_T("single-instance blocked new process. Exiting."));
        return FALSE;
    }

    SetRegistryKey(_T("FileUsageAnalyzer"));

    ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    {
        WNDCLASSEX wc = { 0 };
        if (!::GetClassInfoEx(AfxGetInstanceHandle(), MAIN_WND_CLASS_NAME, &wc))
        {
            wc.cbSize        = sizeof(WNDCLASSEX);
            wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = ::DefDlgProc;
            wc.cbClsExtra    = 0;
            wc.cbWndExtra    = DLGWINDOWEXTRA;
            wc.hInstance     = AfxGetInstanceHandle();
            wc.hIcon         = NULL;
            wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.lpszMenuName  = NULL;
            wc.lpszClassName = MAIN_WND_CLASS_NAME;
            wc.hIconSm       = NULL;
            ATOM atom = ::RegisterClassEx(&wc);
            if (atom == 0)
            {
                DWORD dwRc = ::GetLastError();
                TraceF(_T("FATAL: RegisterClassEx(%s) failed err=%u"), MAIN_WND_CLASS_NAME, dwRc);
                ::CoUninitialize();
                return FALSE;
            }
            TraceF(_T("RegisterClassEx ok: atom=%u, class=%s"), (UINT)atom, MAIN_WND_CLASS_NAME);
        }
    }

    CMainDlg dlg;
    if (!strPath.IsEmpty())
        dlg.m_strCmdLinePath = strPath;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
    }
    else if (nResponse == IDCANCEL)
    {
    }

    ::CoUninitialize();
    return FALSE;
}

int CFileUsageAnalyzerApp::ExitInstance()
{
    if (m_hMutexSingle) { ::CloseHandle(m_hMutexSingle); m_hMutexSingle = NULL; }
    return CWinApp::ExitInstance();
}
