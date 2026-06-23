#include "stdafx.h"
#include "StaticHeapAnalyzerGUI.h"
#include "StaticHeapAnalyzerGUIDlg.h"

BEGIN_MESSAGE_MAP(CStaticHeapAnalyzerGUIApp, CWinApp)
END_MESSAGE_MAP()

CStaticHeapAnalyzerGUIApp theApp;

CStaticHeapAnalyzerGUIApp::CStaticHeapAnalyzerGUIApp()
{
}

BOOL CStaticHeapAnalyzerGUIApp::InitInstance()
{
    CWinApp::InitInstance();

    CStaticHeapAnalyzerGUIDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    return FALSE;
}
