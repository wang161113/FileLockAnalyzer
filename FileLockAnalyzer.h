#pragma once
#include "stdafx.h"

extern UINT g_msgIsInstance;
extern UINT g_msgShowInstance;

#define SINGLE_INST_MUTEX_NAME       _T("Local\\FileLockAnalyzer.SingleInstance")
#define SINGLE_INST_COPYDATA_PATH    1
#define MAIN_WND_CLASS_NAME          _T("FileLockAnalyzerMainWnd")

class CFileLockAnalyzerApp : public CWinApp
{
public:
    CFileLockAnalyzerApp();
    virtual BOOL InitInstance();
    virtual int ExitInstance();

private:
    HANDLE m_hMutexSingle;

    bool EnsureSingleInstance(HWND* phFoundPrevWnd, CString& outPathArg);
};

extern CFileLockAnalyzerApp theApp;
