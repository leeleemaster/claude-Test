#include "stdafx.h"
#include "StaticHeapAnalyzerGUIDlg.h"

BEGIN_MESSAGE_MAP(CStaticHeapAnalyzerGUIDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE,  &CStaticHeapAnalyzerGUIDlg::OnBnClickedBtnBrowse)
    ON_BN_CLICKED(IDC_BTN_ANALYZE, &CStaticHeapAnalyzerGUIDlg::OnBnClickedBtnAnalyze)
    ON_NOTIFY(NM_DBLCLK,    IDC_LIST_RESULTS, &CStaticHeapAnalyzerGUIDlg::OnNMDblclkListResults)
    ON_NOTIFY(NM_CUSTOMDRAW,IDC_LIST_RESULTS, &CStaticHeapAnalyzerGUIDlg::OnNMCustomdrawListResults)
    ON_NOTIFY(NM_RCLICK,    IDC_LIST_RESULTS, &CStaticHeapAnalyzerGUIDlg::OnNMRClickListResults)
    ON_WM_SIZE()
END_MESSAGE_MAP()

CStaticHeapAnalyzerGUIDlg::CStaticHeapAnalyzerGUIDlg(CWnd* pParent)
    : CDialogEx(IDD_MAIN_DIALOG, pParent)
{
}

void CStaticHeapAnalyzerGUIDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_PATH,        m_editPath);
    DDX_Control(pDX, IDC_COMBO_REPORT,     m_comboReport);
    DDX_Control(pDX, IDC_EDIT_RULES,       m_editRules);
    DDX_Control(pDX, IDC_CHECK_RECURSIVE,  m_checkRecursive);
    DDX_Control(pDX, IDC_LIST_RESULTS,     m_listResults);
}

BOOL CStaticHeapAnalyzerGUIDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Korean labels (override RC placeholders)
    SetWindowText(_T("StaticHeapAnalyzer - 힙 메모리 오류 분석기"));
    SetDlgItemText(IDC_GRP_SRC,          _T("소스 경로"));
    SetDlgItemText(IDC_BTN_BROWSE,       _T("찾아보기..."));
    SetDlgItemText(IDC_GRP_OPT,          _T("분석 옵션"));
    SetDlgItemText(IDC_LBL_REPORT,       _T("보고서:"));
    SetDlgItemText(IDC_LBL_RULES,        _T("규칙:"));
    SetDlgItemText(IDC_CHECK_RECURSIVE,  _T("하위 폴더 포함"));
    SetDlgItemText(IDC_BTN_ANALYZE,      _T("▶  분석 시작"));
    SetDlgItemText(IDC_GRP_RES,          _T("분석 결과"));
    SetDlgItemText(IDC_STATIC_STATUS,    _T("준비"));

    // Edit placeholder hint
    m_editRules.SetCueBanner(_T("전체 (예: R1,R2,R9)"), TRUE);
    m_editPath.SetCueBanner(_T("분석할 파일 또는 폴더 경로"), TRUE);

    // Report format combo
    m_comboReport.AddString(_T("console"));
    m_comboReport.AddString(_T("json"));
    m_comboReport.AddString(_T("html"));
    m_comboReport.SetCurSel(0);

    // List columns  (width adjusted in ResizeControls on first WM_SIZE)
    m_listResults.InsertColumn(0, _T("심각도"), LVCFMT_LEFT,  70);
    m_listResults.InsertColumn(1, _T("파일"),   LVCFMT_LEFT, 220);
    m_listResults.InsertColumn(2, _T("줄"),     LVCFMT_RIGHT, 44);
    m_listResults.InsertColumn(3, _T("규칙"),   LVCFMT_LEFT, 130);
    m_listResults.InsertColumn(4, _T("설명"),   LVCFMT_LEFT, 400);
    m_listResults.SetExtendedStyle(
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);

    // Allow resizing
    ModifyStyle(0, WS_SIZEBOX | WS_MAXIMIZEBOX);

    return TRUE;
}

// ─── Browse ─────────────────────────────────────────────────────────────────

void CStaticHeapAnalyzerGUIDlg::OnBnClickedBtnBrowse()
{
    BROWSEINFO bi  = {};
    bi.hwndOwner   = GetSafeHwnd();
    bi.lpszTitle   = _T("분석할 소스 디렉터리를 선택하세요");
    bi.ulFlags     = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl) {
        TCHAR path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path))
            m_editPath.SetWindowText(path);
        CoTaskMemFree(pidl);
    }
}

// ─── Analyze ────────────────────────────────────────────────────────────────

void CStaticHeapAnalyzerGUIDlg::OnBnClickedBtnAnalyze()
{
    CString srcPath;
    m_editPath.GetWindowText(srcPath);
    srcPath.Trim();
    if (srcPath.IsEmpty()) {
        MessageBox(_T("분석할 소스 경로를 입력하거나 [찾아보기] 버튼으로 선택하세요."),
                   _T("경로 없음"), MB_OK | MB_ICONWARNING);
        return;
    }

    int sel = m_comboReport.GetCurSel();
    CString fmt;
    m_comboReport.GetLBText(sel < 0 ? 0 : sel, fmt);

    CString rules;
    m_editRules.GetWindowText(rules);
    rules.Trim();

    bool recursive = (m_checkRecursive.GetCheck() == BST_CHECKED);

    // Disable UI
    GetDlgItem(IDC_BTN_ANALYZE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_BROWSE)->EnableWindow(FALSE);
    SetStatus(_T("분석 중..."));
    m_listResults.DeleteAllItems();
    m_findings.clear();

    bool ok = RunAnalyzer(srcPath, fmt, rules, recursive);

    GetDlgItem(IDC_BTN_ANALYZE)->EnableWindow(TRUE);
    GetDlgItem(IDC_BTN_BROWSE)->EnableWindow(TRUE);

    if (ok) {
        PopulateList();

        int cr = 0, hi = 0, me = 0, lo = 0, info = 0;
        for (auto& f : m_findings) {
            if      (f.severity == _T("CRITICAL")) cr++;
            else if (f.severity == _T("HIGH"))     hi++;
            else if (f.severity == _T("MEDIUM"))   me++;
            else if (f.severity == _T("LOW"))      lo++;
            else if (f.severity == _T("INFO"))     info++;
        }
        CString status;
        status.Format(
            _T("분석 완료  |  총 %d 건  |  CRITICAL: %d  HIGH: %d  MEDIUM: %d  LOW: %d  INFO: %d"),
            (int)m_findings.size(), cr, hi, me, lo, info);
        SetStatus(status);
    }
    // On failure RunAnalyzer already showed a MessageBox
}

void CStaticHeapAnalyzerGUIDlg::SetStatus(const CString& msg)
{
    SetDlgItemText(IDC_STATIC_STATUS, msg);
    UpdateWindow();
}

// ─── Analyzer subprocess ─────────────────────────────────────────────────────

CString CStaticHeapAnalyzerGUIDlg::GetAnalyzerExePath() const
{
    TCHAR buf[MAX_PATH];
    GetModuleFileName(NULL, buf, MAX_PATH);
    CString dir(buf);
    int cut = dir.ReverseFind(_T('\\'));
    if (cut >= 0) dir = dir.Left(cut + 1);
    return dir + _T("StaticHeapAnalyzer.exe");
}

bool CStaticHeapAnalyzerGUIDlg::RunAnalyzer(const CString& src, const CString& fmt,
                                              const CString& rules, bool recursive)
{
    CString exe = GetAnalyzerExePath();
    if (GetFileAttributes(exe) == INVALID_FILE_ATTRIBUTES) {
        CString msg;
        msg.Format(
            _T("분석기 실행 파일을 찾을 수 없습니다:\n%s\n\n")
            _T("StaticHeapAnalyzer.exe 를 이 프로그램과 같은 폴더에 놓으세요."),
            (LPCTSTR)exe);
        MessageBox(msg, _T("분석기 없음"), MB_OK | MB_ICONERROR);
        return false;
    }

    // JSON output goes to temp dir
    TCHAR tmpDir[MAX_PATH];
    GetTempPath(MAX_PATH, tmpDir);
    CString jsonOut = CString(tmpDir) + _T("StaticHeapAnalyzerReport.json");
    DeleteFile(jsonOut);

    // Build command line
    CString cmd;
    cmd.Format(_T("\"%s\" --src \"%s\" --report json"), (LPCTSTR)exe, (LPCTSTR)src);
    if (!rules.IsEmpty())
        cmd += _T(" --rules ") + rules;
    if (recursive)
        cmd += _T(" --recursive");

    STARTUPINFO si = {};
    si.cb           = sizeof(si);
    si.dwFlags      = STARTF_USESHOWWINDOW;
    si.wShowWindow  = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    // Run in temp dir so output file lands there
    BOOL created = CreateProcess(
        NULL, cmd.GetBuffer(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, tmpDir, &si, &pi);
    cmd.ReleaseBuffer();

    if (!created) {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(_T("분석기 실행 실패 (오류 %lu).\n\n%s"), err, (LPCTSTR)exe);
        MessageBox(msg, _T("실행 오류"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Wait max 2 min
    DWORD wait = WaitForSingleObject(pi.hProcess, 120000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (wait == WAIT_TIMEOUT) {
        MessageBox(_T("분석 시간 초과 (2분).\n파일 수를 줄이거나 --recursive 옵션을 해제하세요."),
                   _T("시간 초과"), MB_OK | MB_ICONWARNING);
        return false;
    }

    return ParseJsonReport(jsonOut);
}

// ─── JSON parser ─────────────────────────────────────────────────────────────

// UTF-8 std::string → CString (UTF-16) without ATL dependency
static CString Utf8ToCString(const std::string& s)
{
    if (s.empty()) return CString();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return CString(ws.c_str());
}

static std::string ExtractStr(const std::string& obj, const char* key)
{
    std::string k = std::string("\"") + key + "\":\"";
    size_t pos = obj.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    std::string result;
    while (pos < obj.size() && obj[pos] != '"') {
        if (obj[pos] == '\\' && pos + 1 < obj.size()) {
            switch (obj[pos + 1]) {
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            case 'n':  result += '\n'; break;
            case 'r':                  break;
            case 't':  result += '\t'; break;
            default:   result += obj[pos + 1];
            }
            pos += 2;
        } else {
            result += obj[pos++];
        }
    }
    return result;
}

static int ExtractInt(const std::string& obj, const char* key)
{
    std::string k = std::string("\"") + key + "\":";
    size_t pos = obj.find(k);
    if (pos == std::string::npos) return 0;
    pos += k.size();
    while (pos < obj.size() && !isdigit((unsigned char)obj[pos])) pos++;
    return atoi(obj.c_str() + pos);
}

bool CStaticHeapAnalyzerGUIDlg::ParseJsonReport(const CString& jsonPath)
{
    std::ifstream ifs(static_cast<LPCWSTR>(jsonPath));
    if (!ifs) {
        MessageBox(_T("JSON 보고서 파일을 읽을 수 없습니다.\n분석 결과가 없거나 경로가 잘못되었습니다."),
                   _T("파일 오류"), MB_OK | MB_ICONERROR);
        return false;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string json = oss.str();

    m_findings.clear();

    size_t arr = json.find("\"findings\":");
    if (arr == std::string::npos) return true;
    arr = json.find('[', arr);
    if (arr == std::string::npos) return true;

    size_t pos = arr + 1;
    while (pos < json.size()) {
        while (pos < json.size() && isspace((unsigned char)json[pos])) pos++;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '{') { pos++; continue; }

        // Find matching '}'
        int depth = 0;
        size_t end = pos;
        while (end < json.size()) {
            char c = json[end];
            if (c == '{') depth++;
            else if (c == '}') { if (--depth == 0) { end++; break; } }
            else if (c == '"') {
                end++;
                while (end < json.size() && json[end] != '"') {
                    if (json[end] == '\\') end++;
                    end++;
                }
            }
            end++;
        }

        std::string obj = json.substr(pos, end - pos);

        FindingItem fi;
        fi.severity = Utf8ToCString(ExtractStr(obj, "severity"));
        fi.file     = Utf8ToCString(ExtractStr(obj, "file"    ));
        fi.rule     = Utf8ToCString(ExtractStr(obj, "rule"    ));
        fi.message  = Utf8ToCString(ExtractStr(obj, "message" ));
        fi.code     = Utf8ToCString(ExtractStr(obj, "code"    ));
        fi.fix      = Utf8ToCString(ExtractStr(obj, "fix"     ));
        fi.line     = ExtractInt(obj, "line");
        m_findings.push_back(fi);

        pos = end;
    }
    return true;
}

// ─── List control ────────────────────────────────────────────────────────────

void CStaticHeapAnalyzerGUIDlg::PopulateList()
{
    m_listResults.DeleteAllItems();
    for (int i = 0; i < (int)m_findings.size(); ++i) {
        const FindingItem& f = m_findings[i];
        int idx = m_listResults.InsertItem(i, f.severity);
        m_listResults.SetItemText(idx, 1, f.file);
        CString ln; ln.Format(_T("%d"), f.line);
        m_listResults.SetItemText(idx, 2, ln);
        m_listResults.SetItemText(idx, 3, f.rule);
        m_listResults.SetItemText(idx, 4, f.message);
    }
}

COLORREF CStaticHeapAnalyzerGUIDlg::SeverityColor(const CString& sev) const
{
    if (sev == _T("CRITICAL")) return RGB(255, 180, 180);
    if (sev == _T("HIGH"))     return RGB(255, 220, 160);
    if (sev == _T("MEDIUM"))   return RGB(255, 255, 190);
    if (sev == _T("LOW"))      return RGB(220, 240, 220);
    return RGB(255, 255, 255);
}

void CStaticHeapAnalyzerGUIDlg::OnNMCustomdrawListResults(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVCUSTOMDRAW* p = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    if (p->nmcd.dwDrawStage == CDDS_PREPAINT) {
        *pResult = CDRF_NOTIFYITEMDRAW;
    } else if (p->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        int idx = (int)p->nmcd.dwItemSpec;
        if (idx >= 0 && idx < (int)m_findings.size())
            p->clrTextBk = SeverityColor(m_findings[idx].severity);
        *pResult = CDRF_DODEFAULT;
    }
}

// ─── Detail view ─────────────────────────────────────────────────────────────

void CStaticHeapAnalyzerGUIDlg::ShowDetail(int idx)
{
    if (idx < 0 || idx >= (int)m_findings.size()) return;
    const FindingItem& f = m_findings[idx];

    CString msg;
    msg.Format(
        _T("파일:   %s\n")
        _T("줄:     %d\n")
        _T("규칙:   %s\n")
        _T("심각도: %s\n\n")
        _T("코드:\n  %s\n\n")
        _T("원인:\n  %s\n\n")
        _T("수정 방법:\n  %s"),
        (LPCTSTR)f.file, f.line,
        (LPCTSTR)f.rule, (LPCTSTR)f.severity,
        (LPCTSTR)f.code,
        (LPCTSTR)f.message,
        (LPCTSTR)f.fix);
    MessageBox(msg, _T("발견 항목 상세"), MB_OK | MB_ICONINFORMATION);
}

void CStaticHeapAnalyzerGUIDlg::OnNMDblclkListResults(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMITEMACTIVATE* p = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
    ShowDetail(p->iItem);
    *pResult = 0;
}

// ─── Context menu ─────────────────────────────────────────────────────────────

void CStaticHeapAnalyzerGUIDlg::OnNMRClickListResults(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    int sel = -1;
    POSITION pos = m_listResults.GetFirstSelectedItemPosition();
    if (pos) sel = m_listResults.GetNextSelectedItem(pos);

    CMenu menu;
    menu.CreatePopupMenu();
    UINT flags = MF_STRING | (sel < 0 ? MF_GRAYED : 0);
    menu.AppendMenu(flags,          101, _T("소스 파일 열기"));
    menu.AppendMenu(flags,          102, _T("상세 정보 보기"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING,      103, _T("모두 선택 (Ctrl+A)"));

    POINT pt; GetCursorPos(&pt);
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, this);

    switch (cmd) {
    case 101:
        if (sel >= 0 && sel < (int)m_findings.size())
            ShellExecute(NULL, _T("open"), m_findings[sel].file, NULL, NULL, SW_SHOW);
        break;
    case 102:
        ShowDetail(sel);
        break;
    case 103:
        for (int i = 0; i < m_listResults.GetItemCount(); ++i)
            m_listResults.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
        break;
    }
    *pResult = 0;
}

// ─── Resize ──────────────────────────────────────────────────────────────────

void CStaticHeapAnalyzerGUIDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (nType == SIZE_MINIMIZED) return;
    ResizeControls(cx, cy);
}

void CStaticHeapAnalyzerGUIDlg::ResizeControls(int cx, int cy)
{
    if (!IsWindow(m_listResults.GetSafeHwnd())) return;

    const int M  = 7;   // margin
    const int BW = 55;  // browse button width
    const int RH = 12;  // status height

    // Source path group
    if (CWnd* g = GetDlgItem(IDC_GRP_SRC))
        g->SetWindowPos(NULL, M, 5, cx - 2*M, 24, SWP_NOZORDER);
    m_editPath.SetWindowPos(NULL, M+6, 14, cx - 2*M - BW - 12, 13, SWP_NOZORDER);
    if (CWnd* b = GetDlgItem(IDC_BTN_BROWSE))
        b->SetWindowPos(NULL, cx - M - BW, 13, BW, 15, SWP_NOZORDER);

    // Options group (fixed height)
    if (CWnd* g = GetDlgItem(IDC_GRP_OPT))
        g->SetWindowPos(NULL, M, 34, cx - 2*M, 30, SWP_NOZORDER);
    // Analyze button docked to the right
    if (CWnd* b = GetDlgItem(IDC_BTN_ANALYZE))
        b->SetWindowPos(NULL, cx - M - 86, 43, 86, 17, SWP_NOZORDER);

    // Results list – fills remaining space
    int listTop  = 79;
    int listH    = cy - listTop - RH - 2*M;
    if (listH < 20) listH = 20;

    if (CWnd* g = GetDlgItem(IDC_GRP_RES))
        g->SetWindowPos(NULL, M, 69, cx - 2*M, cy - 69 - RH - M, SWP_NOZORDER);
    m_listResults.SetWindowPos(NULL, M+6, listTop, cx - 2*M - 12, listH, SWP_NOZORDER);

    // Status at bottom
    if (CWnd* s = GetDlgItem(IDC_STATIC_STATUS))
        s->SetWindowPos(NULL, M, cy - RH - M + 2, cx - 2*M, RH, SWP_NOZORDER);

    Invalidate();
}
