#pragma once

#include "resource.h"
#include "FileLockDetector.h"
#include <vector>

enum ELanguage
{
    LANG_EN = 0,
    LANG_ZH = 1
};

enum EAnchorType
{
    ANCHOR_TOPLEFT = 0,
    ANCHOR_TOPRIGHT,
    ANCHOR_TOPWIDTH,
    ANCHOR_BOTTOMLEFT,
    ANCHOR_BOTTOMRIGHT,
    ANCHOR_BOTTOMWIDTH,
    ANCHOR_ALL,
};

struct SAnchorInfo
{
    int nCtrlID;
    int nOrgLeft;
    int nOrgTop;
    int nOrgRight;
    int nOrgBottom;
    EAnchorType anchor;
};

class CMainDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CMainDlg)
    DECLARE_MESSAGE_MAP()

public:
    CMainDlg(CWnd* pParent = NULL);
    virtual ~CMainDlg();

    enum { IDD = IDD_MAIN_DIALOG };

public:
    HICON m_hIcon;
    CEdit m_EditPath;
    CListCtrl m_ListProcesses;

    CFileLockDetector m_Detector;
    LockInfoArray m_arrLocks;

    ELanguage m_eLang;
    int m_nOrgClientCX;
    int m_nOrgClientCY;
    std::vector<SAnchorInfo> m_arrAnchors;
    BOOL m_bAnchorsReady;
    BOOL m_bTrayReady;

    CString m_strCmdLinePath;

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK();
    virtual void OnCancel();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnDestroy();
    afx_msg void OnClose();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
    afx_msg void OnDropFiles(HDROP hDropInfo);
    afx_msg LRESULT OnTrayNotify(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnIsInstanceMsg(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnShowInstanceMsg(WPARAM wParam, LPARAM lParam);
    afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
    afx_msg LRESULT OnPostInitAnalyze(WPARAM wParam, LPARAM lParam);

    afx_msg void OnBnClickedBrowseFile();
    afx_msg void OnBnClickedBrowseFolder();
    afx_msg void OnBnClickedAnalyze();
    afx_msg void OnBnClickedKillProcess();
    afx_msg void OnBnClickedCloseHandle();
    afx_msg void OnBnClickedUnlockFile();
    afx_msg void OnBnClickedDeleteFile();
    afx_msg void OnBnClickedRefresh();
    afx_msg void OnBnClickedToggleLang();
    afx_msg void OnBnClickedRegisterShell();
    afx_msg void OnBnClickedUnregisterShell();

    afx_msg void OnNMDblclkListProcesses(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnNMRclickListProcesses(NMHDR *pNMHDR, LRESULT *pResult);

public:
    void InitListView();
    void PopulateListView(const LockInfoArray& arrLocks);
    void ClearListView();
    void PerformAnalysis();
    bool GetSelectedProcess(ProcessLockInfo& outInfo);
    bool GetSelectedProcesses(std::vector<ProcessLockInfo>& outInfos, bool bUniquePid = true);
    void ShowProcessDetails(const ProcessLockInfo& info);
    void SetStatus(LPCTSTR lpszFormat, ...);
    void AppendStatus(LPCTSTR lpszText);

    void BuildAnchorList();
    void ApplyAnchors(int cx, int cy);
    void AddAnchorCtrl(int nCtrlID, EAnchorType anchor);
    CRect GetInitialWindowRect(int nCtrlID);

    void AddTrayIcon();
    void RemoveTrayIcon();

    void ApplyLanguage();
    LPCTSTR Str(int nResId);
    LPCTSTR T(LPCTSTR lpszEN, LPCTSTR lpszZH);
    CString GetCopyrightText() const;

    bool RegisterShellMenu();
    bool UnregisterShellMenu();
    static bool IsRunningAsAdmin();
    static void RelaunchAsAdmin(LPCTSTR lpszArgs = NULL);
    static bool RegCreateSetStr(HKEY hRoot, LPCTSTR lpszSubKey, LPCTSTR lpszValue, LPCTSTR lpszData);
    static bool RegDeleteTree(HKEY hRoot, LPCTSTR lpszSubKey);

    void ProcessCommandLineArgs();
};
