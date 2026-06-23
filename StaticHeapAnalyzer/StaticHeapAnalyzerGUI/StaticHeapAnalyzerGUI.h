#pragma once
#include "stdafx.h"

class CStaticHeapAnalyzerGUIApp : public CWinApp
{
public:
    CStaticHeapAnalyzerGUIApp();
    virtual BOOL InitInstance();

    DECLARE_MESSAGE_MAP()
};

extern CStaticHeapAnalyzerGUIApp theApp;
