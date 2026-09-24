#include "stdafx.h"
#include "MainDlg.h"
#include "FileLockAnalyzer.h"
#include <set>

IMPLEMENT_DYNAMIC(CMainDlg, CDialogEx)

static const UINT WM_POST_INIT_ANALYZE = WM_USER + 201;

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
    ON_WM_CLOSE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DROPFILES()
    ON_MESSAGE(WM_TRAYNOTIFY, OnTrayNotify)
    ON_REGISTERED_MESSAGE(g_msgIsInstance, OnIsInstanceMsg)
    ON_REGISTERED_MESSAGE(g_msgShowInstance, OnShowInstanceMsg)
    ON_WM_COPYDATA()
    ON_MESSAGE(WM_POST_INIT_ANALYZE, OnPostInitAnalyze)

    ON_BN_CLICKED(IDC_BUTTON_BROWSE_FILE, &CMainDlg::OnBnClickedBrowseFile)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_FOLDER, &CMainDlg::OnBnClickedBrowseFolder)
    ON_BN_CLICKED(IDC_BUTTON_ANALYZE, &CMainDlg::OnBnClickedAnalyze)
    ON_BN_CLICKED(IDC_BUTTON_KILL_PROCESS, &CMainDlg::OnBnClickedKillProcess)
    ON_BN_CLICKED(IDC_BUTTON_CLOSE_HANDLE, &CMainDlg::OnBnClickedCloseHandle)
    ON_BN_CLICKED(IDC_BUTTON_UNLOCK_FILE, &CMainDlg::OnBnClickedUnlockFile)
    ON_BN_CLICKED(IDC_BUTTON_DELETE_FILE, &CMainDlg::OnBnClickedDeleteFile)
    ON_BN_CLICKED(IDC_BUTTON_REFRESH, &CMainDlg::OnBnClickedRefresh)
    ON_BN_CLICKED(IDC_BUTTON_TOGGLE_LANG, &CMainDlg::OnBnClickedToggleLang)
    ON_BN_CLICKED(IDC_BUTTON_REGISTER_SHELL, &CMainDlg::OnBnClickedRegisterShell)
    ON_BN_CLICKED(IDC_BUTTON_UNREGISTER_SHELL, &CMainDlg::OnBnClickedUnregisterShell)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_PROCESSES, &CMainDlg::OnNMDblclkListProcesses)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_PROCESSES, &CMainDlg::OnNMRclickListProcesses)
END_MESSAGE_MAP()

// ---------------- i18n strings ----------------

enum
{
    S_TITLE = 0,
    S_GROUP_TARGET,
    S_GROUP_RESULT,
    S_GROUP_SHELL,
    S_LABEL_PATH,
    S_BTN_FILE,
    S_BTN_DIR,
    S_BTN_ANALYZE,
    S_BTN_KILL,
    S_BTN_CLOSE_HDL,
    S_BTN_UNLOCK,
    S_BTN_DELETE,
    S_BTN_REFRESH,
    S_BTN_REG_SHELL,
    S_BTN_UNREG_SHELL,
    S_STATUS_READY,
    S_ERR_SEL_EMPTY,
    S_ERR_INVALID_PATH,
    S_MSG_ANALYZING,
    S_MSG_FOUND_N,
    S_MSG_NONE_FOUND,
    S_MSG_CONFIRM_KILL,
    S_MSG_CONFIRM_KILL_MULTI,
    S_MSG_CONFIRM_DELETE,
    S_MSG_SUCCESS,
    S_MSG_FAILED,
    S_MSG_KILL_SUMMARY,
    S_MSG_ADMIN_REQ,
    S_MSG_SHELL_REGISTERED,
    S_MSG_SHELL_UNREGISTERED,
    S_MSG_CONFIRM_ELEVATE,
    S_COL_PID,
    S_COL_NAME,
    S_COL_PATH,
    S_COL_LOCK,
    S_COL_TYPE,
    S_MENU_KILL,
    S_MENU_CLOSE_HDL,
    S_MENU_UNLOCK,
    S_MENU_DELETE,
    S_MENU_DETAILS,
    S_MSG_NONE_SELECTED,
    S_MSG_DELETE_PENDING_REBOOT,
    S_LAST
};

static LPCTSTR g_StrsEN[S_LAST] =
{
    _T("File Lock Analyzer"),                  // S_TITLE
    _T("Target"),                               // S_GROUP_TARGET
    _T("Lock Analysis Results"),                // S_GROUP_RESULT
    _T("Shell Integration"),                    // S_GROUP_SHELL
    _T("Path:"),                                // S_LABEL_PATH
    _T("File..."),                              // S_BTN_FILE
    _T("Dir..."),                               // S_BTN_DIR
    _T("Analyze"),                              // S_BTN_ANALYZE
    _T("Kill Process"),                         // S_BTN_KILL
    _T("Close Handle"),                         // S_BTN_CLOSE_HDL
    _T("Unlock/Unload"),                        // S_BTN_UNLOCK
    _T("Delete File"),                          // S_BTN_DELETE
    _T("Refresh"),                              // S_BTN_REFRESH
    _T("Register Shell Menu"),                  // S_BTN_REG_SHELL
    _T("Unregister Shell Menu"),                // S_BTN_UNREG_SHELL
    _T("Ready"),                                // S_STATUS_READY
    _T("Error: target path is empty."),         // S_ERR_SEL_EMPTY
    _T("Error: invalid path."),                 // S_ERR_INVALID_PATH
    _T("Analyzing..."),                         // S_MSG_ANALYZING
    _T("%d process(es) holding target."),       // S_MSG_FOUND_N
    _T("No locks detected."),                   // S_MSG_NONE_FOUND
    _T("Really terminate process %s (PID %u)?"),// S_MSG_CONFIRM_KILL
    _T("Really terminate %u selected process(es)?"), // S_MSG_CONFIRM_KILL_MULTI
    _T("Really delete locked file/folder %s?"), // S_MSG_CONFIRM_DELETE
    _T("Success."),                             // S_MSG_SUCCESS
    _T("Operation failed."),                    // S_MSG_FAILED
    _T("Terminated %u/%u selected process(es)."), // S_MSG_KILL_SUMMARY
    _T("This operation requires Administrator privilege."), // S_MSG_ADMIN_REQ
    _T("Shell context menu registered."),       // S_MSG_SHELL_REGISTERED
    _T("Shell context menu removed."),          // S_MSG_SHELL_UNREGISTERED
    _T("Restart as Administrator to proceed?"), // S_MSG_CONFIRM_ELEVATE
    _T("PID"),                                  // S_COL_PID
    _T("Process"),                              // S_COL_NAME
    _T("Process Path"),                         // S_COL_PATH
    _T("Locked File"),                          // S_COL_LOCK
    _T("Type"),                                 // S_COL_TYPE
    _T("Kill Process"),                         // S_MENU_KILL
    _T("Force Close Handle"),                   // S_MENU_CLOSE_HDL
    _T("Unlock / Unload Module"),               // S_MENU_UNLOCK
    _T("Delete File"),                          // S_MENU_DELETE
    _T("Details..."),                           // S_MENU_DETAILS
    _T("Please select at least one process."),  // S_MSG_NONE_SELECTED
    _T("File will be deleted on next reboot."), // S_MSG_DELETE_PENDING_REBOOT
};

static LPCTSTR g_StrsZH[S_LAST] =
{
    _T("文件占用分析工具"),
    _T("分析目标"),
    _T("占用分析结果"),
    _T("资源管理器集成"),
    _T("路径："),
    _T("文件..."),
    _T("目录..."),
    _T("开始分析"),
    _T("结束进程"),
    _T("强制关闭句柄"),
    _T("解除占用/卸载"),
    _T("删除文件"),
    _T("刷新"),
    _T("注册右键菜单"),
    _T("撤销右键菜单"),
    _T("就绪"),
    _T("错误：目标路径为空。"),
    _T("错误：路径无效。"),
    _T("正在分析..."),
    _T("检测到 %d 个进程持有目标。"),
    _T("未检测到占用进程。"),
    _T("确认结束进程 %s（PID %u）？"),
    _T("确认结束选中的 %u 个进程？"),
    _T("确认删除被占用的文件/文件夹 %s？"),
    _T("操作成功。"),
    _T("操作失败。"),
    _T("已结束 %u/%u 个选中进程。"),
    _T("本操作需要管理员权限。"),
    _T("已注册右键菜单。"),
    _T("已撤销右键菜单。"),
    _T("是否以管理员身份重新启动？"),
    _T("PID"),
    _T("进程名"),
    _T("进程路径"),
    _T("占用文件"),
    _T("类型"),
    _T("结束进程"),
    _T("强制关闭句柄"),
    _T("解除占用/卸载模块"),
    _T("删除文件"),
    _T("详细信息..."),
    _T("请先选中至少一个进程。"),
    _T("文件将在下次重启时删除。"),
};

CMainDlg::CMainDlg(CWnd* pParent /*=NULL*/)
    : CDialogEx(CMainDlg::IDD, pParent)
    , m_hIcon(NULL)
    , m_eLang(LANG_EN)
    , m_nOrgClientCX(0)
    , m_nOrgClientCY(0)
    , m_bAnchorsReady(FALSE)
    , m_bTrayReady(FALSE)
{
    int nSaved = AfxGetApp()->GetProfileInt(_T("Settings"), _T("Language"), -1);
    if (nSaved == LANG_EN || nSaved == LANG_ZH)
    {
        m_eLang = (ELanguage)nSaved;
    }
    else
    {
        LANGID langid = ::GetUserDefaultUILanguage();
        if (PRIMARYLANGID(langid) == LANG_CHINESE)
            m_eLang = LANG_ZH;
        else
            m_eLang = LANG_EN;
    }
    m_hIcon = AfxGetApp()->LoadIcon(IDI_MAIN_ICON);
}

CMainDlg::~CMainDlg()
{
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_PATH, m_EditPath);
    DDX_Control(pDX, IDC_LIST_PROCESSES, m_ListProcesses);
}

BOOL CMainDlg::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CDialogEx::PreCreateWindow(cs))
        return FALSE;
    // 注册（复用）我们专属的窗口类名，让单实例 FindWindow 能在 SW_HIDE 托盘隐藏时也找到老实例
    static bool bRegistered = false;
    if (!bRegistered)
    {
        WNDCLASSEX wc = {0};
        wc.cbSize        = sizeof(wc);
        if (!::GetClassInfoEx(AfxGetInstanceHandle(), MAIN_WND_CLASS_NAME, &wc))
        {
            wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = ::DefDlgProc;
            wc.hInstance     = AfxGetInstanceHandle();
            wc.hIcon         = NULL;
            wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.lpszMenuName  = NULL;
            wc.lpszClassName = MAIN_WND_CLASS_NAME;
            wc.hIconSm       = NULL;
            ::RegisterClassEx(&wc);
        }
        bRegistered = true;
    }
    cs.lpszClass = MAIN_WND_CLASS_NAME;
    return TRUE;
}

void CMainDlg::OnOK()
{
}

void CMainDlg::OnCancel()
{
}

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg && pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_RETURN)
        {
            HWND hFocus = ::GetFocus();
            if (hFocus == m_EditPath.GetSafeHwnd())
            {
                PerformAnalysis();
                return TRUE;
            }
            return TRUE;
        }
        if (pMsg->wParam == VK_ESCAPE)
        {
            return TRUE;
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

BOOL CMainDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    DragAcceptFiles(TRUE);

    InitListView();
    BuildAnchorList();
    ApplyLanguage();

    SetStatus(_T("%s"), Str(S_STATUS_READY));
    AddTrayIcon();

    PostMessage(WM_POST_INIT_ANALYZE, 0, 0);

    return TRUE;
}

LRESULT CMainDlg::OnPostInitAnalyze(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    ProcessCommandLineArgs();
    return 0;
}

void CMainDlg::OnDestroy()
{
    RemoveTrayIcon();
    CDialogEx::OnDestroy();
}

void CMainDlg::OnClose()
{
    if (m_bTrayReady)
    {
        ShowWindow(SW_HIDE);
        ::Shell_NotifyIcon(NIM_SETFOCUS, NULL);
    }
    else
    {
        EndDialog(IDCANCEL);
    }
}

void CMainDlg::AddTrayIcon()
{
    if (m_bTrayReady) return;
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = m_hWnd;
    nid.uID = IDI_MAIN_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYNOTIFY;
    nid.hIcon = m_hIcon ? m_hIcon : ::LoadIcon(NULL, IDI_INFORMATION);
    CString strTip = Str(S_TITLE);
    _tcsncpy_s(nid.szTip, 128, strTip, _TRUNCATE);
    m_bTrayReady = ::Shell_NotifyIcon(NIM_ADD, &nid);
}

void CMainDlg::RemoveTrayIcon()
{
    if (!m_bTrayReady) return;
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = m_hWnd;
    nid.uID = IDI_MAIN_ICON;
    ::Shell_NotifyIcon(NIM_DELETE, &nid);
    m_bTrayReady = FALSE;
}

LRESULT CMainDlg::OnTrayNotify(WPARAM wParam, LPARAM lParam)
{
    if (wParam != IDI_MAIN_ICON) return 0;
    UINT uMsg = (UINT)lParam;
    if (uMsg == WM_LBUTTONUP || uMsg == WM_LBUTTONDBLCLK)
    {
        ShowWindow(SW_RESTORE);
        ::SetForegroundWindow(m_hWnd);
    }
    else if (uMsg == WM_RBUTTONUP)
    {
        CMenu menu;
        if (!menu.CreatePopupMenu()) return 0;
        menu.AppendMenu(MF_STRING, ID_TRAY_EXIT, T(_T("E&xit"), _T("退出(&X)")));
        POINT pt;
        ::GetCursorPos(&pt);
        ::SetForegroundWindow(m_hWnd);
        int nCmd = (int)menu.TrackPopupMenu(
            TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
            pt.x, pt.y, this, NULL);
        if (nCmd == ID_TRAY_EXIT)
        {
            RemoveTrayIcon();
            EndDialog(IDCANCEL);
        }
    }
    return 0;
}

LRESULT CMainDlg::OnIsInstanceMsg(WPARAM, LPARAM)
{
    return 1;
}

LRESULT CMainDlg::OnShowInstanceMsg(WPARAM, LPARAM)
{
    if (!::IsWindowVisible(m_hWnd) || ::IsIconic(m_hWnd))
    {
        ShowWindow(SW_RESTORE);
    }
    ::SetForegroundWindow(m_hWnd);
    return 0;
}

BOOL CMainDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCDS)
{
    if (pCDS && pCDS->dwData == SINGLE_INST_COPYDATA_PATH && pCDS->lpData && pCDS->cbData > 0)
    {
        LPCTSTR lpszPath = (LPCTSTR)pCDS->lpData;
        int nChars = (int)(pCDS->cbData / sizeof(TCHAR));
        if (nChars > 0 && lpszPath[nChars - 1] == _T('\0'))
        {
            if (m_EditPath.GetSafeHwnd())
            {
                m_EditPath.SetWindowText(lpszPath);
                SetStatus(_T("%s %s"), T(_T("Analyzing:"), L"正在分析："), CString(lpszPath));
                MSG msg;
                while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { ::DispatchMessage(&msg); }
                PerformAnalysis();
            }
        }
    }
    OnShowInstanceMsg(0, 0);
    return CDialogEx::OnCopyData(pWnd, pCDS);
}

void CMainDlg::OnDropFiles(HDROP hDropInfo)
{
    UINT nFiles = ::DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
    if (nFiles >= 1)
    {
        TCHAR szPath[MAX_PATH] = {0};
        ::DragQueryFile(hDropInfo, 0, szPath, MAX_PATH);
        m_EditPath.SetWindowText(szPath);
        PerformAnalysis();
    }
    ::DragFinish(hDropInfo);
}

LPCTSTR CMainDlg::Str(int nResId)
{
    if (nResId < 0 || nResId >= S_LAST) return _T("");
    return (m_eLang == LANG_ZH) ? g_StrsZH[nResId] : g_StrsEN[nResId];
}

LPCTSTR CMainDlg::T(LPCTSTR lpszEN, LPCTSTR lpszZH)
{
    return (m_eLang == LANG_ZH) ? lpszZH : lpszEN;
}

CString CMainDlg::GetCopyrightText() const
{
    if (m_eLang == LANG_ZH)
        return _T("版权所有 © Finn Wang | 定制开发微信：FinnSoft");
    return _T("Copyright © Finn Wang | WeChat: FinnSoft");
}

void CMainDlg::ApplyLanguage()
{
    SetWindowText(Str(S_TITLE));
    SetDlgItemText(IDC_BUTTON_TOGGLE_LANG, (m_eLang == LANG_ZH) ? _T("EN") : _T("中"));

    SetDlgItemText(IDC_GROUP_TARGET, Str(S_GROUP_TARGET));
    SetDlgItemText(IDC_GROUP_RESULT, Str(S_GROUP_RESULT));
    SetDlgItemText(IDC_GROUP_UTILITY, Str(S_GROUP_SHELL));
    SetDlgItemText(IDC_STATIC_PATH_LABEL, Str(S_LABEL_PATH));
    SetDlgItemText(IDC_BUTTON_BROWSE_FILE, Str(S_BTN_FILE));
    SetDlgItemText(IDC_BUTTON_BROWSE_FOLDER, Str(S_BTN_DIR));
    SetDlgItemText(IDC_BUTTON_ANALYZE, Str(S_BTN_ANALYZE));
    SetDlgItemText(IDC_BUTTON_KILL_PROCESS, Str(S_BTN_KILL));
    SetDlgItemText(IDC_BUTTON_CLOSE_HANDLE, Str(S_BTN_CLOSE_HDL));
    SetDlgItemText(IDC_BUTTON_UNLOCK_FILE, Str(S_BTN_UNLOCK));
    SetDlgItemText(IDC_BUTTON_DELETE_FILE, Str(S_BTN_DELETE));
    SetDlgItemText(IDC_BUTTON_REFRESH, Str(S_BTN_REFRESH));
    SetDlgItemText(IDC_BUTTON_REGISTER_SHELL, Str(S_BTN_REG_SHELL));
    SetDlgItemText(IDC_BUTTON_UNREGISTER_SHELL, Str(S_BTN_UNREG_SHELL));
    SetDlgItemText(IDC_STATIC_COPYRIGHT, GetCopyrightText());

    m_ListProcesses.SetRedraw(FALSE);
    VERIFY(m_ListProcesses.DeleteColumn(4));
    VERIFY(m_ListProcesses.DeleteColumn(3));
    VERIFY(m_ListProcesses.DeleteColumn(2));
    VERIFY(m_ListProcesses.DeleteColumn(1));
    VERIFY(m_ListProcesses.DeleteColumn(0));
    RECT rc;
    m_ListProcesses.GetClientRect(&rc);
    int cx = rc.right - rc.left;
    if (cx < 100) cx = 520;
    m_ListProcesses.InsertColumn(0, Str(S_COL_PID),   LVCFMT_LEFT, (int)(cx * 0.08));
    m_ListProcesses.InsertColumn(1, Str(S_COL_NAME),  LVCFMT_LEFT, (int)(cx * 0.15));
    m_ListProcesses.InsertColumn(2, Str(S_COL_PATH),  LVCFMT_LEFT, (int)(cx * 0.32));
    m_ListProcesses.InsertColumn(3, Str(S_COL_LOCK),  LVCFMT_LEFT, (int)(cx * 0.32));
    m_ListProcesses.InsertColumn(4, Str(S_COL_TYPE),  LVCFMT_LEFT, (int)(cx * 0.13));
    m_ListProcesses.SetRedraw(TRUE);
    m_ListProcesses.Invalidate();
}

void CMainDlg::InitListView()
{
    DWORD dwStyle = m_ListProcesses.GetExtendedStyle();
    dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP;
    m_ListProcesses.SetExtendedStyle(dwStyle);

    RECT rc;
    m_ListProcesses.GetClientRect(&rc);
    int cx = rc.right - rc.left;
    if (cx < 100) cx = 520;
    m_ListProcesses.InsertColumn(0, _T("PID"),   LVCFMT_LEFT, (int)(cx * 0.08));
    m_ListProcesses.InsertColumn(1, _T("Process"), LVCFMT_LEFT, (int)(cx * 0.15));
    m_ListProcesses.InsertColumn(2, _T("Process Path"), LVCFMT_LEFT, (int)(cx * 0.32));
    m_ListProcesses.InsertColumn(3, _T("Locked File"), LVCFMT_LEFT, (int)(cx * 0.32));
    m_ListProcesses.InsertColumn(4, _T("Type"),  LVCFMT_LEFT, (int)(cx * 0.13));
}

void CMainDlg::ClearListView()
{
    m_ListProcesses.DeleteAllItems();
    m_arrLocks.clear();
}

void CMainDlg::PopulateListView(const LockInfoArray& arrLocks)
{
    m_ListProcesses.SetRedraw(FALSE);
    m_ListProcesses.DeleteAllItems();
    m_arrLocks = arrLocks;

    CString strBuf;
    for (size_t i = 0; i < arrLocks.size(); i++)
    {
        const ProcessLockInfo& info = arrLocks[i];
        strBuf.Format(_T("%u"), info.dwPID);
        int nItem = m_ListProcesses.InsertItem((int)i, strBuf);
        m_ListProcesses.SetItemText(nItem, 1, info.strProcessName);
        m_ListProcesses.SetItemText(nItem, 2, info.strProcessPath.IsEmpty() ? CString(_T("(N/A)")) : info.strProcessPath);
        m_ListProcesses.SetItemText(nItem, 3, info.strLockedFile);
        m_ListProcesses.SetItemText(nItem, 4, info.strLockType);
    }

    if (!arrLocks.empty())
    {
        // 结果出来后默认选中第一项，减少重复点击成本。
        m_ListProcesses.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_ListProcesses.EnsureVisible(0, FALSE);
    }

    m_ListProcesses.SetRedraw(TRUE);
    m_ListProcesses.Invalidate();
}

bool CMainDlg::GetSelectedProcess(ProcessLockInfo& outInfo)
{
    POSITION pos = m_ListProcesses.GetFirstSelectedItemPosition();
    if (!pos) return false;
    int nSel = m_ListProcesses.GetNextSelectedItem(pos);
    if (nSel < 0 || nSel >= (int)m_arrLocks.size()) return false;
    outInfo = m_arrLocks[(size_t)nSel];
    return true;
}

bool CMainDlg::GetSelectedProcesses(std::vector<ProcessLockInfo>& outInfos, bool bUniquePid)
{
    outInfos.clear();

    POSITION pos = m_ListProcesses.GetFirstSelectedItemPosition();
    if (!pos) return false;

    std::set<DWORD> seenPids;
    while (pos)
    {
        int nSel = m_ListProcesses.GetNextSelectedItem(pos);
        if (nSel < 0 || nSel >= (int)m_arrLocks.size())
            continue;

        const ProcessLockInfo& info = m_arrLocks[(size_t)nSel];
        if (bUniquePid && !seenPids.insert(info.dwPID).second)
            continue;

        outInfos.push_back(info);
    }

    return !outInfos.empty();
}

void CMainDlg::SetStatus(LPCTSTR lpszFormat, ...)
{
    CString str;
    va_list args;
    va_start(args, lpszFormat);
    str.FormatV(lpszFormat, args);
    va_end(args);
    CWnd* pSt = GetDlgItem(IDC_STATIC_STATUS);
    if (pSt) pSt->SetWindowText(str);
}

void CMainDlg::AppendStatus(LPCTSTR lpszText)
{
    if (!lpszText) return;
    CString strCur;
    CWnd* pSt = GetDlgItem(IDC_STATIC_STATUS);
    if (pSt) pSt->GetWindowText(strCur);
    if (strCur.GetLength() > 256) strCur = strCur.Left(256) + _T("...");
    CString strOut;
    strOut.Format(_T("%s   [%s]"), strCur, lpszText);
    if (pSt) pSt->SetWindowText(strOut);
}

void CMainDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, (WPARAM)dc.GetSafeHdc(), 0);
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

HCURSOR CMainDlg::OnQueryDragIcon()
{
    return (HCURSOR)m_hIcon;
}

// ---------------- Anchor / Resize ----------------

CRect CMainDlg::GetInitialWindowRect(int nCtrlID)
{
    CRect r;
    CWnd* p = GetDlgItem(nCtrlID);
    if (p) p->GetWindowRect(&r); else return CRect(0,0,0,0);
    ScreenToClient(&r);
    return r;
}

void CMainDlg::AddAnchorCtrl(int nCtrlID, EAnchorType anchor)
{
    SAnchorInfo ai;
    ai.nCtrlID = nCtrlID;
    CRect r = GetInitialWindowRect(nCtrlID);
    ai.nOrgLeft = r.left;
    ai.nOrgTop = r.top;
    ai.nOrgRight = r.right;
    ai.nOrgBottom = r.bottom;
    ai.anchor = anchor;
    m_arrAnchors.push_back(ai);
}

void CMainDlg::BuildAnchorList()
{
    CRect rc;
    GetClientRect(&rc);
    m_nOrgClientCX = rc.Width();
    m_nOrgClientCY = rc.Height();
    if (m_nOrgClientCX < 10 || m_nOrgClientCY < 10) return;

    m_arrAnchors.clear();

    AddAnchorCtrl(IDC_BUTTON_TOGGLE_LANG,  ANCHOR_TOPRIGHT);

    AddAnchorCtrl(IDC_GROUP_TARGET,          ANCHOR_TOPWIDTH);
    AddAnchorCtrl(IDC_STATIC_PATH_LABEL,     ANCHOR_TOPLEFT);
    AddAnchorCtrl(IDC_EDIT_PATH,             ANCHOR_TOPWIDTH);
    AddAnchorCtrl(IDC_BUTTON_BROWSE_FILE,    ANCHOR_TOPRIGHT);
    AddAnchorCtrl(IDC_BUTTON_BROWSE_FOLDER,  ANCHOR_TOPRIGHT);
    AddAnchorCtrl(IDC_BUTTON_ANALYZE,        ANCHOR_TOPLEFT);

    AddAnchorCtrl(IDC_GROUP_RESULT,          ANCHOR_ALL);
    AddAnchorCtrl(IDC_LIST_PROCESSES,        ANCHOR_ALL);
    AddAnchorCtrl(IDC_BUTTON_KILL_PROCESS,   ANCHOR_BOTTOMLEFT);
    AddAnchorCtrl(IDC_BUTTON_CLOSE_HANDLE,   ANCHOR_BOTTOMLEFT);
    AddAnchorCtrl(IDC_BUTTON_UNLOCK_FILE,    ANCHOR_BOTTOMLEFT);
    AddAnchorCtrl(IDC_BUTTON_DELETE_FILE,    ANCHOR_BOTTOMLEFT);
    AddAnchorCtrl(IDC_BUTTON_REFRESH,        ANCHOR_BOTTOMRIGHT);

    AddAnchorCtrl(IDC_GROUP_UTILITY,         ANCHOR_BOTTOMWIDTH);
    AddAnchorCtrl(IDC_BUTTON_REGISTER_SHELL, ANCHOR_BOTTOMLEFT);
    AddAnchorCtrl(IDC_BUTTON_UNREGISTER_SHELL,ANCHOR_BOTTOMLEFT);

    AddAnchorCtrl(IDC_STATIC_STATUS,         ANCHOR_BOTTOMWIDTH);
    AddAnchorCtrl(IDC_STATIC_COPYRIGHT,      ANCHOR_BOTTOMRIGHT);

    m_bAnchorsReady = TRUE;
}

void CMainDlg::ApplyAnchors(int cx, int cy)
{
    if (!m_bAnchorsReady || m_nOrgClientCX < 10 || m_nOrgClientCY < 10) return;
    int dx = cx - m_nOrgClientCX;
    int dy = cy - m_nOrgClientCY;

    HDWP hdwp = ::BeginDeferWindowPos((int)m_arrAnchors.size() + 4);
    for (size_t i = 0; i < m_arrAnchors.size(); i++)
    {
        const SAnchorInfo& ai = m_arrAnchors[i];
        CWnd* pWnd = GetDlgItem(ai.nCtrlID);
        if (!pWnd || !::IsWindow(pWnd->m_hWnd)) continue;

        int x = ai.nOrgLeft, y = ai.nOrgTop, w = ai.nOrgRight - ai.nOrgLeft, h = ai.nOrgBottom - ai.nOrgTop;
        switch (ai.anchor)
        {
        case ANCHOR_TOPLEFT:    break;
        case ANCHOR_TOPRIGHT:   x += dx; break;
        case ANCHOR_TOPWIDTH:   w += dx; break;
        case ANCHOR_BOTTOMLEFT: y += dy; break;
        case ANCHOR_BOTTOMRIGHT:x += dx; y += dy; break;
        case ANCHOR_BOTTOMWIDTH:w += dx; y += dy; break;
        case ANCHOR_ALL:        w += dx; h += dy; break;
        }
        ::DeferWindowPos(hdwp, pWnd->m_hWnd, NULL, x, y, w, h,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    ::EndDeferWindowPos(hdwp);
}

void CMainDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0 && m_bAnchorsReady)
    {
        ApplyAnchors(cx, cy);
        if (::IsWindow(m_ListProcesses.m_hWnd))
        {
            RECT rc;
            m_ListProcesses.GetClientRect(&rc);
            int cw = rc.right - rc.left;
            if (cw > 100)
            {
                m_ListProcesses.SetColumnWidth(0, (int)(cw * 0.08));
                m_ListProcesses.SetColumnWidth(1, (int)(cw * 0.15));
                m_ListProcesses.SetColumnWidth(2, (int)(cw * 0.32));
                m_ListProcesses.SetColumnWidth(3, (int)(cw * 0.32));
                m_ListProcesses.SetColumnWidth(4, LVSCW_AUTOSIZE_USEHEADER);
                int w4 = m_ListProcesses.GetColumnWidth(4);
                if (w4 < (int)(cw * 0.10)) w4 = (int)(cw * 0.10);
                m_ListProcesses.SetColumnWidth(4, w4);
            }
        }
    }
}

void CMainDlg::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI)
{
    if (lpMMI)
    {
        lpMMI->ptMinTrackSize.x = 480;
        lpMMI->ptMinTrackSize.y = 340;
    }
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

// ---------------- Buttons / Operations ----------------

void CMainDlg::OnBnClickedToggleLang()
{
    m_eLang = (m_eLang == LANG_EN) ? LANG_ZH : LANG_EN;
    AfxGetApp()->WriteProfileInt(_T("Settings"), _T("Language"), (int)m_eLang);
    ApplyLanguage();
}

void CMainDlg::OnBnClickedBrowseFile()
{
    CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        T(_T("All Files (*.*)|*.*||"),
          _T("所有文件 (*.*)|*.*||")), this);
    if (dlg.DoModal() == IDOK)
    {
        m_EditPath.SetWindowText(dlg.GetPathName());
    }
}

void CMainDlg::OnBnClickedBrowseFolder()
{
    BROWSEINFO bi;
    ::ZeroMemory(&bi, sizeof(bi));
    TCHAR szDisp[MAX_PATH] = {0};
    bi.hwndOwner = m_hWnd;
    bi.pszDisplayName = szDisp;
    bi.lpszTitle = T(_T("Select a folder to analyze"), _T("请选择要分析的文件夹"));
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (pidl)
    {
        TCHAR szPath[MAX_PATH] = {0};
        if (::SHGetPathFromIDList(pidl, szPath))
        {
            m_EditPath.SetWindowText(szPath);
        }
        LPMALLOC pMalloc = NULL;
        if (SUCCEEDED(::SHGetMalloc(&pMalloc)) && pMalloc)
        {
            pMalloc->Free(pidl);
            pMalloc->Release();
        }
    }
}

void CMainDlg::OnBnClickedAnalyze()
{
    PerformAnalysis();
}

void CMainDlg::PerformAnalysis()
{
    CString strPath;
    m_EditPath.GetWindowText(strPath);
    strPath.Trim();
    if (strPath.IsEmpty())
    {
        MessageBox(Str(S_ERR_SEL_EMPTY), Str(S_TITLE), MB_ICONWARNING);
        return;
    }

    DWORD dwAttr = ::GetFileAttributes(strPath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES)
    {
        CString s;
        s.Format(_T("%s\n\n%s"), Str(S_ERR_INVALID_PATH), strPath);
        MessageBox(s, Str(S_TITLE), MB_ICONWARNING);
        SetStatus(_T("%s - %s"), Str(S_ERR_INVALID_PATH), strPath);
        return;
    }

    SetStatus(_T("%s %s"), T(_T("Analyzing:"), L"正在分析："), strPath);
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { ::DispatchMessage(&msg); }

    LockInfoArray arr;
    BOOL bOk = FALSE;
    bool bIsDir = ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0);
    if (bIsDir)
        bOk = m_Detector.AnalyzeFolder(strPath, arr);
    else
        bOk = m_Detector.AnalyzeFile(strPath, arr);

    PopulateListView(arr);

    // 结果一定给用户反馈（不仅仅是状态栏）——用户从 shell 右键菜单过来必须看到东西
    if (!bOk)
    {
        CString s;
        s.Format(_T("%s\n\n%s: %s"),
            T(_T("Failed to analyze target."), L"分析失败。"),
            bIsDir ? T(_T("Folder"), L"文件夹") : T(_T("File"), L"文件"),
            strPath);
        MessageBox(s, Str(S_TITLE), MB_ICONWARNING);
        SetStatus(_T("%s - %s"), Str(S_MSG_FAILED), strPath);
    }
    else if (arr.empty())
    {
        CString s;
        s.Format(_T("%s\n\n%s: %s"), Str(S_MSG_NONE_FOUND),
            bIsDir ? T(_T("Folder"), L"文件夹") : T(_T("File"), L"文件"),
            strPath);
        MessageBox(s, Str(S_TITLE), MB_ICONINFORMATION);
        CString ss; ss.Format(Str(S_MSG_FOUND_N), 0);
        SetStatus(_T("%s"), ss);
    }
    else
    {
        CString s;
        s.Format(Str(S_MSG_FOUND_N), (int)arr.size());
        SetStatus(_T("%s"), s);
    }

    // 确保主窗口可见并置顶（从 shell 右键菜单 / 单实例 OnCopyData 进来时，窗口可能在托盘里）
    if (!::IsWindowVisible(m_hWnd) || ::IsIconic(m_hWnd))
        ::ShowWindow(m_hWnd, SW_RESTORE);
    ::SetForegroundWindow(m_hWnd);
}

void CMainDlg::OnBnClickedKillProcess()
{
    std::vector<ProcessLockInfo> arrSelected;
    if (!GetSelectedProcesses(arrSelected, true)) { MessageBox(Str(S_MSG_NONE_SELECTED), Str(S_TITLE), MB_ICONWARNING); return; }

    CString s;
    if (arrSelected.size() == 1)
        s.Format(Str(S_MSG_CONFIRM_KILL), arrSelected[0].strProcessName, arrSelected[0].dwPID);
    else
        s.Format(Str(S_MSG_CONFIRM_KILL_MULTI), (UINT)arrSelected.size());
    if (IDYES != MessageBox(s, Str(S_TITLE), MB_ICONQUESTION | MB_YESNO)) return;

    UINT nSuccess = 0;
    for (size_t i = 0; i < arrSelected.size(); ++i)
    {
        if (m_Detector.KillProcess(arrSelected[i].dwPID))
            ++nSuccess;
    }

    if (nSuccess > 0)
    {
        if (arrSelected.size() == 1)
            SetStatus(_T("%s [%s PID=%u]"), Str(S_MSG_SUCCESS), arrSelected[0].strProcessName, arrSelected[0].dwPID);
        else
            SetStatus(Str(S_MSG_KILL_SUMMARY), nSuccess, (UINT)arrSelected.size());

        PerformAnalysis();
        if (nSuccess == arrSelected.size())
            return;
    }

    if (!IsRunningAsAdmin() && IDYES == MessageBox(CString(Str(S_MSG_ADMIN_REQ)) + _T("\n") + Str(S_MSG_CONFIRM_ELEVATE), Str(S_TITLE), MB_ICONQUESTION | MB_YESNO))
    {
        RelaunchAsAdmin();
    }
    else if (nSuccess == 0)
    {
        MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
    }
}

void CMainDlg::OnBnClickedCloseHandle()
{
    ProcessLockInfo info;
    if (!GetSelectedProcess(info)) { MessageBox(Str(S_MSG_NONE_SELECTED), Str(S_TITLE), MB_ICONWARNING); return; }
    if (!IsRunningAsAdmin())
    {
        if (IDYES == MessageBox(CString(Str(S_MSG_ADMIN_REQ)) + _T("\n") + Str(S_MSG_CONFIRM_ELEVATE), Str(S_TITLE), MB_ICONQUESTION | MB_YESNO))
            RelaunchAsAdmin();
        return;
    }
    if (m_Detector.ForceCloseFileHandle(info.strLockedFile, info.dwPID))
    {
        SetStatus(_T("%s [%s]"), Str(S_MSG_SUCCESS), info.strLockedFile);
        PerformAnalysis();
    }
    else
    {
        MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
    }
}

void CMainDlg::OnBnClickedUnlockFile()
{
    ProcessLockInfo info;
    if (!GetSelectedProcess(info)) { MessageBox(Str(S_MSG_NONE_SELECTED), Str(S_TITLE), MB_ICONWARNING); return; }
    if (!IsRunningAsAdmin())
    {
        if (IDYES == MessageBox(CString(Str(S_MSG_ADMIN_REQ)) + _T("\n") + Str(S_MSG_CONFIRM_ELEVATE), Str(S_TITLE), MB_ICONQUESTION | MB_YESNO))
            RelaunchAsAdmin();
        return;
    }
    BOOL bOk1 = m_Detector.ForceCloseFileHandle(info.strLockedFile, info.dwPID);
    BOOL bOk2 = m_Detector.UnloadModuleFromProcess(info.strLockedFile, info.dwPID);
    if (bOk1 || bOk2)
    {
        SetStatus(_T("%s"), Str(S_MSG_SUCCESS));
        PerformAnalysis();
    }
    else
    {
        MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
    }
}

void CMainDlg::OnBnClickedDeleteFile()
{
    ProcessLockInfo info;
    if (!GetSelectedProcess(info)) { MessageBox(Str(S_MSG_NONE_SELECTED), Str(S_TITLE), MB_ICONWARNING); return; }
    CString s;
    s.Format(Str(S_MSG_CONFIRM_DELETE), info.strLockedFile);
    if (IDYES != MessageBox(s, Str(S_TITLE), MB_ICONQUESTION | MB_YESNO)) return;
    if (!IsRunningAsAdmin())
    {
        if (IDYES == MessageBox(CString(Str(S_MSG_ADMIN_REQ)) + _T("\n") + Str(S_MSG_CONFIRM_ELEVATE), Str(S_TITLE), MB_ICONQUESTION | MB_YESNO))
            RelaunchAsAdmin();
        return;
    }
    if (m_Detector.DeleteLockedFile(info.strLockedFile))
    {
        SetStatus(_T("%s. %s"), Str(S_MSG_SUCCESS), Str(S_MSG_DELETE_PENDING_REBOOT));
        PerformAnalysis();
    }
    else
    {
        MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
    }
}

void CMainDlg::OnBnClickedRefresh()
{
    PerformAnalysis();
}

void CMainDlg::OnNMDblclkListProcesses(NMHDR *pNMHDR, LRESULT *pResult)
{
    UNREFERENCED_PARAMETER(pNMHDR);
    if (pResult) *pResult = 0;
    ProcessLockInfo info;
    if (GetSelectedProcess(info)) ShowProcessDetails(info);
}

void CMainDlg::OnNMRclickListProcesses(NMHDR *pNMHDR, LRESULT *pResult)
{
    UNREFERENCED_PARAMETER(pNMHDR);
    if (pResult) *pResult = 0;
    ProcessLockInfo info;
    if (!GetSelectedProcess(info)) return;

    CMenu menu;
    if (!menu.CreatePopupMenu()) return;
    menu.AppendMenu(MF_STRING, 1, Str(S_MENU_KILL));
    menu.AppendMenu(MF_STRING, 2, Str(S_MENU_CLOSE_HDL));
    menu.AppendMenu(MF_STRING, 3, Str(S_MENU_UNLOCK));
    menu.AppendMenu(MF_STRING, 4, Str(S_MENU_DELETE));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 5, Str(S_MENU_DETAILS));

    POINT pt;
    ::GetCursorPos(&pt);
    int nCmd = (int)menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
        pt.x, pt.y, this, NULL);
    switch (nCmd)
    {
    case 1: OnBnClickedKillProcess();  break;
    case 2: OnBnClickedCloseHandle();  break;
    case 3: OnBnClickedUnlockFile();   break;
    case 4: OnBnClickedDeleteFile();   break;
    case 5: ShowProcessDetails(info);  break;
    default: break;
    }
}

void CMainDlg::ShowProcessDetails(const ProcessLockInfo& info)
{
    CString strProcessPathDisp;
    if (info.strProcessPath.IsEmpty())
        strProcessPathDisp = _T("(N/A)");
    else
        strProcessPathDisp = info.strProcessPath;

    CString strDetail;
    strDetail.Format(
        _T("Process Info:\n")
        _T("--------------------------------\n")
        _T("Name:    %s\n")
        _T("PID:     %u\n")
        _T("Path:    %s\n\n")
        _T("Lock Info:\n")
        _T("--------------------------------\n")
        _T("File:    %s\n")
        _T("Type:    %s\n"),
        info.strProcessName,
        info.dwPID,
        (LPCTSTR)strProcessPathDisp,
        info.strLockedFile,
        info.strLockType
    );
    MessageBox(strDetail, Str(S_TITLE), MB_ICONINFORMATION);
}

// ---------------- Shell Integration / Elevation ----------------

bool CMainDlg::IsRunningAsAdmin()
{
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup = NULL;
    if (!AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
        &AdministratorsGroup))
    {
        return false;
    }
    BOOL bIsAdmin = FALSE;
    CheckTokenMembership(NULL, AdministratorsGroup, &bIsAdmin);
    FreeSid(AdministratorsGroup);
    return (bIsAdmin == TRUE);
}

void CMainDlg::RelaunchAsAdmin(LPCTSTR lpszArgs)
{
    TCHAR szExe[MAX_PATH] = {0};
    ::GetModuleFileName(NULL, szExe, MAX_PATH);
    HINSTANCE h = ::ShellExecute(NULL, _T("runas"), szExe, lpszArgs, NULL, SW_SHOWNORMAL);
    ULONG_PTR code = (ULONG_PTR)h;
    if (code > 32)
    {
        CWnd* pMain = AfxGetMainWnd();
        if (pMain && pMain->GetSafeHwnd() != NULL && pMain->IsKindOf(RUNTIME_CLASS(CMainDlg)))
        {
            CMainDlg* pDlg = (CMainDlg*)pMain;
            if (pDlg->m_bTrayReady)
                pDlg->RemoveTrayIcon();
            pDlg->EndDialog(IDCANCEL);
        }
        return;
    }
    ELanguage ePick = LANG_EN;
    LANGID langid = ::GetUserDefaultUILanguage();
    if (PRIMARYLANGID(langid) == LANG_CHINESE) ePick = LANG_ZH;
    auto _P = [&](LPCTSTR en, LPCTSTR zh) -> LPCTSTR { return (ePick == LANG_ZH) ? zh : en; };
    LPCTSTR lpszTitle = _P(_T("File Lock Analyzer"), L"文件占用分析工具");
    UINT uIcon = MB_ICONERROR;

    CString strErr;
    switch ((int)code)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        strErr = _P(_T("Executable file not found."), L"找不到程序执行文件。");
        break;
    case ERROR_BAD_FORMAT:
        strErr = _P(_T("The program image is invalid (Bad EXE format)."), L"程序映像无效。");
        break;
    case SE_ERR_ACCESSDENIED:
        strErr = _P(_T("Access denied. Please try manually run as administrator."), L"拒绝访问，请手动以管理员身份运行。");
        break;
    case 1223:
        strErr = _P(_T("You cancelled the elevation request."), L"您取消了权限提升请求（UAC 点了否）。");
        break;
    default:
        {
            CString sFmt;
            sFmt.Format(_T("%s %u"),
                _P(_T("Failed to relaunch as administrator. Error code:"), L"提权重启失败，错误码："),
                (unsigned)code);
            ::MessageBox(NULL, sFmt, lpszTitle, uIcon);
            return;
        }
    }
    CString sAll = strErr + _T("\n\n") + CString(szExe);
    ::MessageBox(NULL, sAll, lpszTitle, uIcon);
}

bool CMainDlg::RegCreateSetStr(HKEY hRoot, LPCTSTR lpszSubKey, LPCTSTR lpszValue, LPCTSTR lpszData)
{
    HKEY hKey = NULL;
    DWORD dwDisp = 0;
    if (ERROR_SUCCESS != ::RegCreateKeyEx(hRoot, lpszSubKey, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisp))
    {
        return false;
    }
    bool bOk = true;
    DWORD cbData = (DWORD)((_tcslen(lpszData) + 1) * sizeof(TCHAR));
    if (ERROR_SUCCESS != ::RegSetValueEx(hKey, lpszValue, 0, REG_SZ, (const BYTE*)lpszData, cbData))
    {
        bOk = false;
    }
    ::RegCloseKey(hKey);
    return bOk;
}

bool CMainDlg::RegDeleteTree(HKEY hRoot, LPCTSTR lpszSubKey)
{
    LSTATUS st = ::RegDeleteTree(hRoot, lpszSubKey);
    if (st == ERROR_FILE_NOT_FOUND) return true;
    if (st != ERROR_SUCCESS)
    {
        HKEY hKey = NULL;
        if (ERROR_SUCCESS == ::RegOpenKeyEx(hRoot, lpszSubKey, 0, KEY_READ | KEY_SET_VALUE, &hKey))
        {
            ::RegDeleteValue(hKey, NULL);
            ::RegCloseKey(hKey);
        }
        st = ::RegDeleteKey(hRoot, lpszSubKey);
    }
    return (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
}

void CMainDlg::OnBnClickedRegisterShell()
{
    if (!IsRunningAsAdmin())
    {
        if (IDYES == MessageBox(CString(Str(S_MSG_ADMIN_REQ)) + _T("\n") + Str(S_MSG_CONFIRM_ELEVATE), Str(S_TITLE), MB_ICONQUESTION | MB_YESNO))
            RelaunchAsAdmin(_T("/shellregister"));
        return;
    }
    if (RegisterShellMenu())
        MessageBox(Str(S_MSG_SHELL_REGISTERED), Str(S_TITLE), MB_ICONINFORMATION);
    else
        MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
}

void CMainDlg::OnBnClickedUnregisterShell()
{
    if (!IsRunningAsAdmin())
    {
        if (IDYES == MessageBox(CString(Str(S_MSG_ADMIN_REQ)) + _T("\n") + Str(S_MSG_CONFIRM_ELEVATE), Str(S_TITLE), MB_ICONQUESTION | MB_YESNO))
            RelaunchAsAdmin(_T("/shellunregister"));
        return;
    }
    if (UnregisterShellMenu())
        MessageBox(Str(S_MSG_SHELL_UNREGISTERED), Str(S_TITLE), MB_ICONINFORMATION);
    else
        MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
}

bool CMainDlg::RegisterShellMenu()
{
    TCHAR szExe[MAX_PATH] = {0};
    ::GetModuleFileName(NULL, szExe, MAX_PATH);

    LPCTSTR lpszNameEN = _T("Analyze File Lock");
    LPCTSTR lpszNameZH = _T("使用文件占用分析工具检测");
    LPCTSTR lpszName = (m_eLang == LANG_ZH) ? lpszNameZH : lpszNameEN;

    struct SRootCfg { LPCTSTR lpszSubKey; LPCTSTR lpszCmdFmt; bool bBackground; };
    const SRootCfg roots[3] = {
        { _T("Directory\\shell\\FileLockAnalyzer"),               _T("\"%s\" \"%%1\""), false },
        { _T("Directory\\Background\\shell\\FileLockAnalyzer"),   _T("\"%s\" \"%%V\""), true  },
        { _T("AllFilesystemObjects\\shell\\FileLockAnalyzer"),    _T("\"%s\" \"%%1\""), false },
    };

    bool bAllOk = true;
    for (int k = 0; k < _countof(roots); k++)
    {
        CString skBase = roots[k].lpszSubKey;
        CString skCmd  = skBase + _T("\\command");
        CString strCmd;
        strCmd.Format(roots[k].lpszCmdFmt, szExe);
        bool b1 = RegCreateSetStr(HKEY_LOCAL_MACHINE, _T("Software\\Classes\\") + skBase, NULL,    lpszName);
        bool b2 = RegCreateSetStr(HKEY_LOCAL_MACHINE, _T("Software\\Classes\\") + skBase, _T("Icon"), szExe);
        if (roots[k].bBackground)
        {
            // Background verb 兼容：显式 NoWorkingDirectory 避免 shell32 把 %V 当 working dir 解析；
            //  NeverDefault 告诉 Explorer 不要把它当默认双击 verb。
            RegCreateSetStr(HKEY_LOCAL_MACHINE, _T("Software\\Classes\\") + skBase, _T("NoWorkingDirectory"), _T(""));
            RegCreateSetStr(HKEY_LOCAL_MACHINE, _T("Software\\Classes\\") + skBase, _T("NeverDefault"),     _T(""));
        }
        bool b3 = RegCreateSetStr(HKEY_LOCAL_MACHINE, _T("Software\\Classes\\") + skCmd, NULL, strCmd);
        if (!b1 || !b2 || !b3) bAllOk = false;
    }

    // 刷两遍确保 Explorer 把旧缓存清干净
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, NULL, NULL);
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH,         NULL, NULL);
    ::Sleep(80);
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, NULL, NULL);
    return bAllOk;
}

bool CMainDlg::UnregisterShellMenu()
{
    LPCTSTR szRoots[6] = {
        _T("Directory\\shell\\FileLockAnalyzer"),
        _T("Directory\\Background\\shell\\FileLockAnalyzer"),
        _T("AllFilesystemObjects\\shell\\FileLockAnalyzer"),
        _T("Directory\\shell\\FileUsageAnalyzer"),
        _T("Directory\\Background\\shell\\FileUsageAnalyzer"),
        _T("AllFilesystemObjects\\shell\\FileUsageAnalyzer"),
    };
    bool bAllOk = true;
    for (int k = 0; k < _countof(szRoots); k++)
    {
        if (!RegDeleteTree(HKEY_LOCAL_MACHINE, CString(_T("Software\\Classes\\")) + szRoots[k])) bAllOk = false;
        if (!RegDeleteTree(HKEY_CURRENT_USER,  CString(_T("Software\\Classes\\")) + szRoots[k])) bAllOk = false;
    }
    LPCTSTR szOldRoots[8] = {
        _T("*\\shell\\FileLockAnalyzer"),
        _T("Directory\\shell\\FileLockAnalyzer"),
        _T("Directory\\Background\\shell\\FileLockAnalyzer"),
        _T("AllFilesystemObjects\\shell\\FileLockAnalyzer"),
        _T("*\\shell\\FileUsageAnalyzer"),
        _T("Directory\\shell\\FileUsageAnalyzer"),
        _T("Directory\\Background\\shell\\FileUsageAnalyzer"),
        _T("AllFilesystemObjects\\shell\\FileUsageAnalyzer"),
    };
    for (int k = 0; k < _countof(szOldRoots); k++)
    {
        if (!RegDeleteTree(HKEY_CLASSES_ROOT, szOldRoots[k])) bAllOk = false;
    }
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH, NULL, NULL);
    return bAllOk;
}

// ---------------- Command Line Args (from Shell menu or drag/drop) ----------------

void CMainDlg::ProcessCommandLineArgs()
{
    auto DoAnalyze = [this](LPCTSTR lpszPath) -> void
    {
        if (!lpszPath || !m_EditPath.GetSafeHwnd()) return;
        CString strPath(lpszPath);
        strPath.Trim();
        if (strPath.IsEmpty())
        {
            SetStatus(_T("%s"), T(_T("Ready"), L"就绪"));
            return;
        }
        DWORD dwAttr = ::GetFileAttributes(strPath);
        if (dwAttr == INVALID_FILE_ATTRIBUTES)
        {
            SetStatus(_T("%s - %s"), Str(S_ERR_INVALID_PATH), strPath);
            return;
        }
        while (strPath.GetLength() > 1 &&
               (strPath[strPath.GetLength() - 1] == _T('\\') || strPath[strPath.GetLength() - 1] == _T('/')))
            strPath = strPath.Left(strPath.GetLength() - 1);
        m_EditPath.SetWindowText(strPath);
        SetStatus(_T("%s %s"), T(_T("Analyzing:"), L"正在分析："), strPath);
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { ::DispatchMessage(&msg); }
        PerformAnalysis();
    };

    if (!m_strCmdLinePath.IsEmpty())
    {
        DoAnalyze(m_strCmdLinePath);
        return;
    }

    CString strCmdLine = ::GetCommandLine();
    int nArgs = 0;
    LPWSTR* ppArglist = ::CommandLineToArgvW(strCmdLine, &nArgs);
    if (!ppArglist) return;

    CString strPath;
    bool bShellReg = false, bShellUnreg = false;
    for (int i = 1; i < nArgs; i++)
    {
        CString a = ppArglist[i];
        if (a.CompareNoCase(_T("/shellregister")) == 0)   bShellReg = true;
        else if (a.CompareNoCase(_T("/shellunregister")) == 0) bShellUnreg = true;
        else if (a.GetLength() > 2 && a[0] != _T('/') && a[0] != _T('-'))
        {
            strPath = a;
        }
    }
    ::LocalFree(ppArglist);
    strPath.Trim();

    if (bShellReg)
    {
        if (IsRunningAsAdmin())
        {
            if (RegisterShellMenu())
                MessageBox(Str(S_MSG_SHELL_REGISTERED), Str(S_TITLE), MB_ICONINFORMATION);
            else
                MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
        }
        else
        {
            MessageBox(Str(S_MSG_ADMIN_REQ), Str(S_TITLE), MB_ICONERROR);
        }
        return;
    }
    if (bShellUnreg)
    {
        if (IsRunningAsAdmin())
        {
            if (UnregisterShellMenu())
                MessageBox(Str(S_MSG_SHELL_UNREGISTERED), Str(S_TITLE), MB_ICONINFORMATION);
            else
                MessageBox(Str(S_MSG_FAILED), Str(S_TITLE), MB_ICONERROR);
        }
        else
        {
            MessageBox(Str(S_MSG_ADMIN_REQ), Str(S_TITLE), MB_ICONERROR);
        }
        return;
    }
    DoAnalyze(strPath);
}
