// StaticHeapAnalyzerGUI - Win32 API frontend for StaticHeapAnalyzer CLI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// ─── Control IDs ─────────────────────────────────────────────────────────────
#define IDC_EDIT_PATH       1001
#define IDC_BTN_BROWSE      1002
#define IDC_COMBO_REPORT    1003
#define IDC_EDIT_RULES      1004
#define IDC_CHECK_RECURSIVE 1005
#define IDC_BTN_ANALYZE     1006
#define IDC_LIST_RESULTS    1007
#define IDC_STATIC_STATUS   1008

// ─── Finding ─────────────────────────────────────────────────────────────────
struct Finding {
    std::wstring severity;
    std::wstring rule;
    std::wstring file;
    int          line = 0;
    std::wstring message;
};

static std::vector<Finding> g_findings;

// ─── Helpers ─────────────────────────────────────────────────────────────────
static std::wstring Utf8W(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string WtoUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring TrimW(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return {};
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

// ─── JSON mini-parser ─────────────────────────────────────────────────────────
static std::string JsonStr(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return {};
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return {};
    p = json.find('"', p + 1);
    if (p == std::string::npos) return {};
    size_t q = json.find('"', p + 1);
    while (q != std::string::npos && json[q - 1] == '\\') q = json.find('"', q + 1);
    if (q == std::string::npos) return {};
    std::string v = json.substr(p + 1, q - p - 1);
    std::string unesc;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] != '\\' || i + 1 >= v.size()) { unesc += v[i]; continue; }
        char c2 = v[++i];
        if      (c2 == '"' || c2 == '\\' || c2 == '/') unesc += c2;
        else if (c2 == 'n') unesc += '\n';
        else if (c2 == 't') unesc += '\t';
        else { unesc += '\\'; unesc += c2; }
    }
    return unesc;
}

static int JsonInt(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return 0;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return 0;
    while (p < json.size() && (json[p] == ':' || json[p] == ' ')) ++p;
    return std::atoi(json.c_str() + p);
}

static std::vector<Finding> ParseJson(const std::wstring& path) {
    std::vector<Finding> out;
    std::ifstream f(path.c_str());
    if (!f) return out;
    std::string raw((std::istreambuf_iterator<char>(f)), {});

    // find "findings": [ ... ]
    size_t arr = raw.find("\"findings\"");
    if (arr == std::string::npos) arr = 0;
    size_t start = raw.find('[', arr);
    if (start == std::string::npos) return out;
    size_t end = raw.rfind(']');
    if (end == std::string::npos || end <= start) return out;

    size_t pos = start + 1;
    while (pos < end) {
        size_t ob = raw.find('{', pos);
        if (ob == std::string::npos || ob >= end) break;
        // find matching }
        int depth = 1;
        size_t cb = ob + 1;
        while (cb < raw.size() && depth > 0) {
            if (raw[cb] == '{') ++depth;
            else if (raw[cb] == '}') --depth;
            ++cb;
        }
        if (depth != 0) break;
        std::string obj = raw.substr(ob, cb - ob);
        Finding f2;
        f2.severity = Utf8W(JsonStr(obj, "severity"));
        f2.rule     = Utf8W(JsonStr(obj, "rule"));
        f2.file     = Utf8W(JsonStr(obj, "file"));
        f2.line     = JsonInt(obj, "line");
        f2.message  = Utf8W(JsonStr(obj, "message"));
        out.push_back(f2);
        pos = cb;
    }
    return out;
}

// ─── Severity color ───────────────────────────────────────────────────────────
static COLORREF SeverityColor(const std::wstring& sev) {
    if (sev == L"CRITICAL") return RGB(220,  50,  50);
    if (sev == L"HIGH")     return RGB(220, 120,  30);
    if (sev == L"MEDIUM")   return RGB(180, 150,   0);
    if (sev == L"LOW")      return RGB( 60, 140,  60);
    return RGB(80, 80, 80);
}

// ─── Global window handles ────────────────────────────────────────────────────
static HWND g_hWnd       = nullptr;
static HWND g_hPath      = nullptr;
static HWND g_hBrowse    = nullptr;
static HWND g_hCombo     = nullptr;
static HWND g_hRules     = nullptr;
static HWND g_hRecursive = nullptr;
static HWND g_hAnalyze   = nullptr;
static HWND g_hList      = nullptr;
static HWND g_hStatus    = nullptr;

// ─── Layout ───────────────────────────────────────────────────────────────────
static void DoLayout(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;
    int M = 10;   // margin
    int R = W - M;
    int y = M;
    int row = 26;
    int gap = 6;

    // Row 1: path edit + browse
    MoveWindow(g_hPath,   M,       y, R - M - 90, row, TRUE);
    MoveWindow(g_hBrowse, R - 85,  y, 85,         row, TRUE);
    y += row + gap;

    // Row 2: report label+combo  |  rules label+edit  |  recursive checkbox
    int col1w = 220, col2w = W - M*2 - col1w - 140 - gap*2;
    MoveWindow(g_hCombo,     M,                          y, col1w, row, TRUE);
    MoveWindow(g_hRules,     M + col1w + gap,            y, col2w, row, TRUE);
    MoveWindow(g_hRecursive, M + col1w + gap + col2w + gap, y, 130, row, TRUE);
    y += row + gap;

    // Row 3: analyze button (full width)
    MoveWindow(g_hAnalyze, M, y, R - M, 30, TRUE);
    y += 30 + gap;

    // ListView: rest of space minus status bar
    int statusH = 22;
    MoveWindow(g_hList,   M, y, R - M, H - y - gap - statusH - gap, TRUE);

    // Status bar
    MoveWindow(g_hStatus, M, H - statusH - gap, R - M, statusH, TRUE);

    // Resize list columns proportionally
    int lw = R - M - M;
    int col0 = 90, col1 = 110, col4 = lw - col0 - col1 - 60 - 60;
    if (col4 < 100) col4 = 100;
    ListView_SetColumnWidth(g_hList, 0, col0);
    ListView_SetColumnWidth(g_hList, 1, col1);
    ListView_SetColumnWidth(g_hList, 2, 60);
    ListView_SetColumnWidth(g_hList, 3, col4);
    ListView_SetColumnWidth(g_hList, 4, 60);
}

// ─── List helpers ─────────────────────────────────────────────────────────────
static void ListClear() {
    ListView_DeleteAllItems(g_hList);
}

static void ListAdd(const Finding& f, int idx) {
    LVITEM lvi = {};
    lvi.mask    = LVIF_TEXT;
    lvi.iItem   = idx;
    lvi.iSubItem = 0;
    lvi.pszText = const_cast<LPWSTR>(f.severity.c_str());
    ListView_InsertItem(g_hList, &lvi);

    ListView_SetItemText(g_hList, idx, 1, const_cast<LPWSTR>(f.rule.c_str()));

    // extract filename only
    std::wstring fname = f.file;
    size_t slash = fname.find_last_of(L"\\/");
    if (slash != std::wstring::npos) fname = fname.substr(slash + 1);
    ListView_SetItemText(g_hList, idx, 2, const_cast<LPWSTR>(fname.c_str()));

    std::wstring lineStr = std::to_wstring(f.line);
    ListView_SetItemText(g_hList, idx, 3, const_cast<LPWSTR>(f.message.c_str()));
    ListView_SetItemText(g_hList, idx, 4, const_cast<LPWSTR>(lineStr.c_str()));
}

// ─── Run CLI and return exit code ─────────────────────────────────────────────
static int RunCli(const std::wstring& cmdLine, std::wstring& outErr) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadErr, hWriteErr;
    CreatePipe(&hReadErr, &hWriteErr, &sa, 0);
    SetHandleInformation(hReadErr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = INVALID_HANDLE_VALUE;
    si.hStdOutput  = hWriteErr;
    si.hStdError   = hWriteErr;

    PROCESS_INFORMATION pi = {};
    std::wstring cmd = cmdLine;  // CreateProcess may modify buffer
    BOOL ok = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWriteErr);

    if (!ok) {
        CloseHandle(hReadErr);
        outErr = L"CreateProcess failed";
        return -1;
    }

    // Read stderr/stdout
    std::string buf;
    char tmp[4096];
    DWORD read;
    while (ReadFile(hReadErr, tmp, sizeof(tmp), &read, nullptr) && read > 0)
        buf.append(tmp, read);
    CloseHandle(hReadErr);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    outErr = Utf8W(buf);
    return (int)exitCode;
}

// ─── Find CLI exe next to GUI exe ────────────────────────────────────────────
static std::wstring FindCliExe() {
    wchar_t selfPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
    std::wstring dir(selfPath);
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
    return dir + L"StaticHeapAnalyzer.exe";
}

// ─── Analyze ──────────────────────────────────────────────────────────────────
static void DoAnalyze() {
    wchar_t pathBuf[MAX_PATH] = {};
    GetWindowTextW(g_hPath, pathBuf, MAX_PATH);
    std::wstring srcPath = TrimW(pathBuf);
    if (srcPath.empty()) {
        MessageBoxW(g_hWnd, L"분석할 경로를 선택하세요.", L"경고", MB_ICONWARNING);
        return;
    }

    // report type
    int sel = ComboBox_GetCurSel(g_hCombo);
    std::wstring reportType = (sel == 1) ? L"html" : (sel == 2) ? L"json" : L"json";

    // rules
    wchar_t rulesBuf[512] = {};
    GetWindowTextW(g_hRules, rulesBuf, 512);
    std::wstring rules = TrimW(rulesBuf);

    // recursive
    bool recursive = (Button_GetCheck(g_hRecursive) == BST_CHECKED);

    // temp JSON output
    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring jsonOut = std::wstring(tempDir) + L"StaticHeapAnalyzerReport.json";

    std::wstring cli = FindCliExe();

    // Build command line
    std::wstring cmd = L"\"" + cli + L"\"";
    cmd += L" --src \"" + srcPath + L"\"";
    cmd += L" --report json";
    if (!rules.empty()) cmd += L" --rules \"" + rules + L"\"";
    if (recursive)      cmd += L" --recursive";
    // override output path via working dir trick is not supported by CLI;
    // CLI always writes to CWD. Run CLI with temp dir as CWD.

    // Build command and run from tempDir so JSON lands there
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadErr, hWriteErr;
    CreatePipe(&hReadErr, &hWriteErr, &sa, 0);
    SetHandleInformation(hReadErr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb      = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = INVALID_HANDLE_VALUE;
    si.hStdOutput = hWriteErr;
    si.hStdError  = hWriteErr;

    PROCESS_INFORMATION pi = {};
    std::wstring cmdCopy = cmd;

    SetWindowTextW(g_hStatus, L"분석 중...");
    EnableWindow(g_hAnalyze, FALSE);

    BOOL ok = CreateProcessW(nullptr, &cmdCopy[0], nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr,
                             tempDir,   // working directory = %TEMP%
                             &si, &pi);
    CloseHandle(hWriteErr);

    if (!ok) {
        CloseHandle(hReadErr);
        EnableWindow(g_hAnalyze, TRUE);
        MessageBoxW(g_hWnd,
            (L"StaticHeapAnalyzer.exe 를 찾을 수 없습니다.\n경로: " + cli).c_str(),
            L"오류", MB_ICONERROR);
        SetWindowTextW(g_hStatus, L"실패");
        return;
    }

    std::string buf;
    char tmp[4096];
    DWORD read;
    while (ReadFile(hReadErr, tmp, sizeof(tmp), &read, nullptr) && read > 0)
        buf.append(tmp, read);
    CloseHandle(hReadErr);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    EnableWindow(g_hAnalyze, TRUE);

    if (exitCode == 1) {
        std::wstring msg = L"CLI 오류:\n" + Utf8W(buf);
        MessageBoxW(g_hWnd, msg.c_str(), L"분석 실패", MB_ICONERROR);
        SetWindowTextW(g_hStatus, L"분석 실패");
        return;
    }

    g_findings = ParseJson(jsonOut);
    ListClear();
    for (int i = 0; i < (int)g_findings.size(); ++i)
        ListAdd(g_findings[i], i);

    int cnt = (int)g_findings.size();
    std::wstring status = L"발견: " + std::to_wstring(cnt) + L"건";
    if (exitCode == 2) status += L"  [HIGH/CRITICAL 포함 — 종료코드 2]";
    SetWindowTextW(g_hStatus, status.c_str());
}

// ─── WndProc ──────────────────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        auto MakeCtrl = [&](LPCWSTR cls, LPCWSTR txt, DWORD style, int id) -> HWND {
            HWND h = CreateWindowExW(0, cls, txt,
                WS_CHILD | WS_VISIBLE | style,
                0, 0, 10, 10, hWnd, (HMENU)(UINT_PTR)id,
                GetModuleHandleW(nullptr), nullptr);
            SendMessageW(h, WM_SETFONT, (WPARAM)hFont, TRUE);
            return h;
        };

        g_hPath   = MakeCtrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, IDC_EDIT_PATH);
        g_hBrowse = MakeCtrl(L"BUTTON", L"폴더 선택", BS_PUSHBUTTON, IDC_BTN_BROWSE);

        g_hCombo  = MakeCtrl(L"COMBOBOX", L"",
                       CBS_DROPDOWNLIST | WS_VSCROLL, IDC_COMBO_REPORT);
        ComboBox_AddString(g_hCombo, L"콘솔(JSON 표시)");
        ComboBox_AddString(g_hCombo, L"HTML 리포트");
        ComboBox_AddString(g_hCombo, L"JSON 리포트");
        ComboBox_SetCurSel(g_hCombo, 0);

        g_hRules     = MakeCtrl(L"EDIT", L"",
                           WS_BORDER | ES_AUTOHSCROLL, IDC_EDIT_RULES);
        SendMessageW(g_hRules, EM_SETCUEBANNER, 0, (LPARAM)L"규칙 필터 (예: ALLOC-001,SCOPE-002)");

        g_hRecursive = MakeCtrl(L"BUTTON", L"하위 폴더 포함",
                           BS_AUTOCHECKBOX, IDC_CHECK_RECURSIVE);

        g_hAnalyze   = MakeCtrl(L"BUTTON", L"분석 시작",
                           BS_DEFPUSHBUTTON, IDC_BTN_ANALYZE);

        // ListView
        g_hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 10, 10, hWnd, (HMENU)(UINT_PTR)IDC_LIST_RESULTS,
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_hList, WM_SETFONT, (WPARAM)hFont, TRUE);
        ListView_SetExtendedListViewStyle(g_hList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        auto AddCol = [&](int i, LPCWSTR hdr, int w) {
            LVCOLUMNW col = {};
            col.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            col.cx      = w;
            col.pszText = const_cast<LPWSTR>(hdr);
            col.iSubItem = i;
            ListView_InsertColumn(g_hList, i, &col);
        };
        AddCol(0, L"심각도",  90);
        AddCol(1, L"규칙",   110);
        AddCol(2, L"파일",    60);
        AddCol(3, L"메시지", 400);
        AddCol(4, L"줄",      60);

        g_hStatus = MakeCtrl(L"STATIC", L"준비", SS_SUNKEN, IDC_STATIC_STATUS);

        // Init common controls
        INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icex);

        return 0;
    }

    case WM_SIZE:
        DoLayout(hWnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_BROWSE) {
            BROWSEINFOW bi = {};
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"분석할 폴더를 선택하세요";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t dir[MAX_PATH] = {};
                SHGetPathFromIDListW(pidl, dir);
                CoTaskMemFree(pidl);
                SetWindowTextW(g_hPath, dir);
            }
        } else if (id == IDC_BTN_ANALYZE) {
            DoAnalyze();
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* hdr = (NMHDR*)lParam;
        if (hdr->idFrom == IDC_LIST_RESULTS) {
            if (hdr->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lParam;
                switch (cd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    int idx = (int)cd->nmcd.dwItemSpec;
                    if (idx >= 0 && idx < (int)g_findings.size()) {
                        cd->clrText = SeverityColor(g_findings[idx].severity);
                    }
                    return CDRF_NEWFONT;
                }
                }
                return CDRF_DODEFAULT;
            }
            if (hdr->code == NM_DBLCLK) {
                NMITEMACTIVATE* ia = (NMITEMACTIVATE*)lParam;
                int idx = ia->iItem;
                if (idx >= 0 && idx < (int)g_findings.size()) {
                    const Finding& f = g_findings[idx];
                    std::wstring detail =
                        L"심각도: " + f.severity + L"\n" +
                        L"규칙:   " + f.rule     + L"\n" +
                        L"파일:   " + f.file     + L"\n" +
                        L"줄:     " + std::to_wstring(f.line) + L"\n\n" +
                        L"메시지:\n" + f.message;
                    MessageBoxW(hWnd, detail.c_str(), L"상세 정보", MB_ICONINFORMATION);
                }
            }
        }
        return CDRF_DODEFAULT;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lParam;
        mm->ptMinTrackSize = { 700, 500 };
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ─── WinMain ──────────────────────────────────────────────────────────────────
int WINAPI WinMainImpl(HINSTANCE hInst) {
    INITCOMMONCONTROLSEX icex = { sizeof(icex),
        ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);
    CoInitialize(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SHA_GUI";
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(0, L"SHA_GUI",
        L"Static Heap Analyzer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 680,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(g_hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    CoUninitialize();
    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    return WinMainImpl(hInst);
}
