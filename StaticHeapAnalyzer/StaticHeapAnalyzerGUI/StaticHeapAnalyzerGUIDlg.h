#pragma once
#include "stdafx.h"
#include "resource.h"
#include <vector>
#include <string>

struct FindingItem {
    CString severity;
    CString file;
    int     line;
    CString rule;
    CString message;
    CString code;
    CString fix;
};

class CStaticHeapAnalyzerGUIDlg : public CDialogEx
{
public:
    explicit CStaticHeapAnalyzerGUIDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_MAIN_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnBnClickedBtnBrowse();
    afx_msg void OnBnClickedBtnAnalyze();
    afx_msg void OnNMDblclkListResults(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMCustomdrawListResults(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMRClickListResults(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()

private:
    CEdit       m_editPath;
    CComboBox   m_comboReport;
    CEdit       m_editRules;
    CButton     m_checkRecursive;
    CListCtrl   m_listResults;

    std::vector<FindingItem> m_findings;

    void        SetStatus(const CString& msg);
    bool        RunAnalyzer(const CString& src, const CString& fmt,
                            const CString& rules, bool recursive);
    bool        ParseJsonReport(const CString& jsonPath);
    void        PopulateList();
    void        ShowDetail(int idx);
    CString     GetAnalyzerExePath() const;
    COLORREF    SeverityColor(const CString& sev) const;
    void        ResizeControls(int cx, int cy);
};
