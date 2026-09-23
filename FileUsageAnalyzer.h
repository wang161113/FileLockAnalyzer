#pragma once
#include "stdafx.h"

extern UINT g_msgIsInstance;
extern UINT g_msgShowInstance;

#define SINGLE_INST_MUTEX_NAME       _T("Local\\FileUsageAnalyzer.SingleInstance")
#define SINGLE_INST_COPYDATA_PATH    1
#define MAIN_WND_CLASS_NAME          _T("FileUsageAnalyzerMainWnd")

class CFileUsageAnalyzerApp : public CWinApp
{
public:
    CFileUsageAnalyzerApp();
    virtual BOOL InitInstance();
    virtual int ExitInstance();

private:
    HANDLE m_hMutexSingle;

    bool EnsureSingleInstance(HWND* phFoundPrevWnd, CString& outPathArg);
};

extern CFileUsageAnalyzerApp theApp;
