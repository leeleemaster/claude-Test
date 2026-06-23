// StaticHeapAnalyzerGUI - Win32 API frontend for StaticHeapAnalyzer CLI
#define WINVER       0x0601
#define _WIN32_WINNT 0x0601
#define _WIN32_IE    0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <commdlg.h>

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

#include "resource.h"

// ─── Control IDs ─────────────────────────────────────────────────────────────
#define IDC_EDIT_PATH       1001
#define IDC_BTN_BROWSE      1002
#define IDC_COMBO_REPORT    1003
#define IDC_EDIT_RULES      1004
#define IDC_CHECK_RECURSIVE 1005
#define IDC_BTN_ANALYZE     1006
#define IDC_LIST_RESULTS    1007
#define IDC_STATIC_STATUS   1008
#define IDC_BTN_SAVE        1009

// ─── Finding ─────────────────────────────────────────────────────────────────
struct Finding {
    std::wstring severity;
    std::wstring rule;
    std::wstring file;
    int          line = 0;
    std::wstring message;
};

static std::vector<Finding> g_findings;

// ─── String helpers ───────────────────────────────────────────────────────────
static std::wstring Utf8W(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::wstring TrimW(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return {};
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

// ─── JSON mini-parser ────────────────────────────────────────────────────────
static std::string JsonStr(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return {};
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return {};
    p = json.find('"', p + 1);
    if (p == std::string::npos) return {};
    size_t q = p + 1;
    while (q < json.size()) {
        if (json[q] == '"' && json[q-1] != '\\') break;
        ++q;
    }
    if (q >= json.size()) return {};
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

    size_t arr   = raw.find('[');
    size_t end   = raw.rfind(']');
    if (arr == std::string::npos || end == std::string::npos || end <= arr) return out;

    size_t pos = arr + 1;
    while (pos < end) {
        size_t ob = raw.find('{', pos);
        if (ob == std::string::npos || ob >= end) break;
        int depth = 1;
        size_t cb = ob + 1;
        while (cb < raw.size() && depth > 0) {
            if      (raw[cb] == '{') ++depth;
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
    if (sev == L"MEDIUM")   return RGB(160, 130,   0);
    if (sev == L"LOW")      return RGB( 40, 130,  40);
    return RGB(80, 80, 80);
}

// ─── Heap impact mapping ──────────────────────────────────────────────────────
struct HeapImpact {
    std::wstring category;
    std::wstring explanation;
};

static HeapImpact GetHeapImpact(const std::wstring& rule) {
    auto startsWith = [&](const wchar_t* prefix) {
        size_t n = wcslen(prefix);
        return rule.size() >= n && rule.compare(0, n, prefix) == 0;
    };
    if (startsWith(L"R1"))
        return { L"직접적 (DIRECT)",
                 L"Potential out-of-bounds write/copy can directly corrupt adjacent memory or heap-managed buffers." };
    if (startsWith(L"R2"))
        return { L"직접적 (DIRECT)",
                 L"Mismatched new/delete or new[]/delete[] can corrupt allocator-managed heap state." };
    if (startsWith(L"R3"))
        return { L"직접적 (DIRECT)",
                 L"Mixing new/delete with malloc/free can directly break heap management." };
    if (startsWith(L"R4"))
        return { L"가능성 있음 (POSSIBLE)",
                 L"sizeof(pointer) misuse may under-initialize/copy memory and contribute to later heap misuse." };
    if (startsWith(L"R5"))
        return { L"간접적 (INDIRECT)",
                 L"Zero-length memset is more of a logic/init defect than direct heap corruption by itself." };
    if (startsWith(L"R6"))
        return { L"가능성 있음 (POSSIBLE)",
                 L"Off-by-one indexing can become an out-of-bounds write/read depending on actual access." };
    if (startsWith(L"R7"))
        return { L"가능성 있음 (POSSIBLE)",
                 L"Missing ReleaseBuffer() can violate buffer ownership/contract and lead to later memory problems." };
    if (startsWith(L"R9"))
        return { L"직접적 (DIRECT)",
                 L"Double free is a classic direct heap corruption pattern." };
    if (startsWith(L"ALLOC-001"))
        return { L"가능성 있음 (POSSIBLE)",
                 L"Using raw allocated storage without proper object initialization can trigger invalid memory behavior." };
    if (startsWith(L"CTOR-001"))
        return { L"가능성 있음 (POSSIBLE)",
                 L"Uninitialized pointer member use may dereference invalid memory and can lead to heap corruption." };
    if (startsWith(L"STACK-001"))
        return { L"직접적 (DIRECT)",
                 L"Deleting a stack address is invalid deallocation and can directly corrupt heap internals." };
    if (startsWith(L"THREAD-001"))
        return { L"가능성 있음 (POSSIBLE)",
                 L"Races around shared pointer initialization/access can lead to double free, leaks, or corrupted memory state." };
    return { L"검토 필요 (REVIEW)",
             L"This finding may affect memory safety depending on context and should be reviewed manually." };
}


static HWND g_hWnd       = nullptr;
static HWND g_hPath      = nullptr;
static HWND g_hBrowse    = nullptr;
static HWND g_hRecursive = nullptr;
static HWND g_hAnalyze   = nullptr;
static HWND g_hSave      = nullptr;
static HWND g_hList      = nullptr;
static HWND g_hStatus    = nullptr;

// ─── Layout ──────────────────────────────────────────────────────────────────
static void DoLayout(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;
    if (W <= 0 || H <= 0) return;

    const int M   = 10;
    const int R   = W - M;
    const int ROW = 24;
    const int GAP = 6;
    int y = M;

    // Row 1: path + browse
    if (g_hPath)   MoveWindow(g_hPath,   M,       y, R - M - 90, ROW, TRUE);
    if (g_hBrowse) MoveWindow(g_hBrowse, R - 85,  y, 85,         ROW, TRUE);
    y += ROW + GAP;

    // Row 2: recursive checkbox (right-aligned)
    int checkW = 130;
    if (g_hRecursive) MoveWindow(g_hRecursive, R - checkW, y, checkW, ROW, TRUE);
    y += ROW + GAP;

    // Row 3: analyze + save buttons (side-by-side)
    int availW = R - M - GAP;
    int btnW   = (availW - GAP) / 2;
    if (g_hAnalyze) MoveWindow(g_hAnalyze, M,              y, btnW, 28, TRUE);
    if (g_hSave)    MoveWindow(g_hSave,    M + btnW + GAP, y, availW - btnW, 28, TRUE);
    y += 28 + GAP;

    // Status bar
    const int SH = 20;
    // List view: rest of height minus status
    int listH = H - y - GAP - SH - GAP;
    if (listH < 10) listH = 10;
    if (g_hList)   MoveWindow(g_hList,   M, y,          R - M, listH, TRUE);
    if (g_hStatus) MoveWindow(g_hStatus, M, H - SH - GAP, R - M, SH,    TRUE);

    // Resize list columns
    if (g_hList) {
        int lw = R - M - M;
        int c0 = 80, c1 = 100, c4 = 55;
        int c3 = lw - c0 - c1 - c4 - 55;
        if (c3 < 80) c3 = 80;
        ListView_SetColumnWidth(g_hList, 0, c0);
        ListView_SetColumnWidth(g_hList, 1, c1);
        ListView_SetColumnWidth(g_hList, 2, 55);
        ListView_SetColumnWidth(g_hList, 3, c3);
        ListView_SetColumnWidth(g_hList, 4, c4);
    }
}

// ─── List helpers ─────────────────────────────────────────────────────────────
static void ListClear() {
    if (g_hList) ListView_DeleteAllItems(g_hList);
}

static void ListAdd(const Finding& f, int idx) {
    if (!g_hList) return;

    LVITEMW lvi = {};
    lvi.mask     = LVIF_TEXT;
    lvi.iItem    = idx;
    lvi.iSubItem = 0;
    lvi.pszText  = const_cast<LPWSTR>(f.severity.c_str());
    ListView_InsertItem(g_hList, &lvi);

    ListView_SetItemText(g_hList, idx, 1, const_cast<LPWSTR>(f.rule.c_str()));

    std::wstring fname = f.file;
    size_t slash = fname.find_last_of(L"\\/");
    if (slash != std::wstring::npos) fname = fname.substr(slash + 1);
    ListView_SetItemText(g_hList, idx, 2, const_cast<LPWSTR>(fname.c_str()));

    ListView_SetItemText(g_hList, idx, 3, const_cast<LPWSTR>(f.message.c_str()));

    std::wstring lineStr = std::to_wstring(f.line);
    ListView_SetItemText(g_hList, idx, 4, const_cast<LPWSTR>(lineStr.c_str()));
}

// ─── Extract embedded CLI exe to %TEMP% ──────────────────────────────────────
static std::wstring GetCliExe() {
    // 1) same directory as GUI exe
    wchar_t selfPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
    std::wstring dir(selfPath);
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
    std::wstring sameDir = dir + L"StaticHeapAnalyzer.exe";
    if (GetFileAttributesW(sameDir.c_str()) != INVALID_FILE_ATTRIBUTES)
        return sameDir;

    // 2) extract from embedded resource
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_CLI_EXE), L"RCDATA");
    if (!hRes) return {};
    HGLOBAL hGlob = LoadResource(nullptr, hRes);
    if (!hGlob) return {};
    void*  pData = LockResource(hGlob);
    DWORD  size  = SizeofResource(nullptr, hRes);
    if (!pData || size == 0) return {};

    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring exePath = std::wstring(tempDir) + L"StaticHeapAnalyzer.exe";

    HANDLE hFile = CreateFileW(exePath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return {};
    DWORD written = 0;
    WriteFile(hFile, pData, size, &written, nullptr);
    CloseHandle(hFile);
    return (written == size) ? exePath : std::wstring{};
}

// ─── Analyze ─────────────────────────────────────────────────────────────────
static void DoAnalyze() {
    wchar_t pathBuf[MAX_PATH] = {};
    GetWindowTextW(g_hPath, pathBuf, MAX_PATH);
    std::wstring srcPath = TrimW(pathBuf);
    if (srcPath.empty()) {
        MessageBoxW(g_hWnd, L"분석할 경로를 선택하세요.", L"경고", MB_ICONWARNING);
        return;
    }

    bool recursive = (g_hRecursive && Button_GetCheck(g_hRecursive) == BST_CHECKED);

    // temp JSON output in %TEMP%
    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring jsonOut = std::wstring(tempDir) + L"StaticHeapAnalyzerReport.json";

    // Delete old report
    DeleteFileW(jsonOut.c_str());

    std::wstring cli = GetCliExe();
    if (cli.empty()) {
        MessageBoxW(g_hWnd, L"StaticHeapAnalyzer.exe 를 찾거나 추출할 수 없습니다.",
                    L"오류", MB_ICONERROR);
        return;
    }
    std::wstring cmd = L"\"" + cli + L"\" --src \"" + srcPath + L"\" --report json";
    if (recursive) cmd += L" --recursive";

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        MessageBoxW(g_hWnd, L"파이프 생성 실패", L"오류", MB_ICONERROR);
        return;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = INVALID_HANDLE_VALUE;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    PROCESS_INFORMATION pi = {};

    if (g_hStatus) SetWindowTextW(g_hStatus, L"분석 중...");
    if (g_hAnalyze) EnableWindow(g_hAnalyze, FALSE);

    BOOL ok = CreateProcessW(nullptr, &cmd[0],
                             nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr,
                             tempDir, &si, &pi);
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        if (g_hAnalyze) EnableWindow(g_hAnalyze, TRUE);
        MessageBoxW(g_hWnd, L"분석기 실행 실패", L"오류", MB_ICONERROR);
        if (g_hStatus) SetWindowTextW(g_hStatus, L"실패");
        return;
    }

    // drain output
    std::string buf;
    char tmp[4096];
    DWORD rd;
    while (ReadFile(hRead, tmp, sizeof(tmp), &rd, nullptr) && rd > 0)
        buf.append(tmp, rd);
    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (g_hAnalyze) EnableWindow(g_hAnalyze, TRUE);

    if (exitCode == 1) {
        std::wstring msg = L"CLI 오류:\n" + Utf8W(buf);
        MessageBoxW(g_hWnd, msg.c_str(), L"분석 실패", MB_ICONERROR);
        if (g_hStatus) SetWindowTextW(g_hStatus, L"분석 실패");
        return;
    }

    g_findings = ParseJson(jsonOut);
    ListClear();
    for (int i = 0; i < (int)g_findings.size(); ++i)
        ListAdd(g_findings[i], i);

    std::wstring status = L"발견: " + std::to_wstring((int)g_findings.size()) + L"건";
    if (exitCode == 2) status += L"  [HIGH/CRITICAL 포함]";
    if (g_hStatus) SetWindowTextW(g_hStatus, status.c_str());
}

// ─── Save / Export ────────────────────────────────────────────────────────────
static void DoSave() {
    if (g_findings.empty()) {
        MessageBoxW(g_hWnd,
                    L"저장할 분석 결과가 없습니다.\n먼저 분석을 실행하세요.",
                    L"알림", MB_ICONINFORMATION);
        return;
    }

    // Open standard Save File dialog
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn          = {};
    ofn.lStructSize            = sizeof(ofn);
    ofn.hwndOwner              = g_hWnd;
    ofn.lpstrFilter            = L"텍스트 파일 (*.txt)\0*.txt\0모든 파일 (*.*)\0*.*\0";
    ofn.lpstrFile              = filePath;
    ofn.nMaxFile               = MAX_PATH;
    ofn.lpstrDefExt            = L"txt";
    ofn.lpstrTitle             = L"분석 결과 저장";
    ofn.Flags                  = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn))
        return; // user cancelled

    // Analysis target path
    wchar_t pathBuf[MAX_PATH] = {};
    if (g_hPath) GetWindowTextW(g_hPath, pathBuf, MAX_PATH);
    std::wstring srcPath = TrimW(pathBuf);

    // Current timestamp (no extra CRT headers needed)
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    auto padToTwo = [](int v) -> std::wstring {
        std::wstring s = std::to_wstring(v);
        if (s.size() < 2) s = L"0" + s;
        return s;
    };
    std::wstring timestamp =
        std::to_wstring(st.wYear) + L"-" + padToTwo(st.wMonth) + L"-" + padToTwo(st.wDay) +
        L" " + padToTwo(st.wHour) + L":" + padToTwo(st.wMinute) + L":" + padToTwo(st.wSecond);

    // Build report
    std::wstring rep;
    rep += L"==========================================================\n";
    rep += L"  Static Heap Analyzer - 분석 결과 보고서\n";
    rep += L"==========================================================\n";
    rep += L"분석 대상   : " + (srcPath.empty() ? L"(알 수 없음)" : srcPath) + L"\n";
    rep += L"내보낸 시각 : " + timestamp + L"\n";
    rep += L"총 발견 건수: " + std::to_wstring((int)g_findings.size()) + L"건\n";
    rep += L"==========================================================\n\n";

    for (int i = 0; i < (int)g_findings.size(); ++i) {
        const Finding& f = g_findings[i];
        HeapImpact hi    = GetHeapImpact(f.rule);
        rep += L"[" + std::to_wstring(i + 1) + L"]\n";
        rep += L"심각도       : " + f.severity + L"\n";
        rep += L"규칙         : " + f.rule + L"\n";
        rep += L"파일         : " + f.file + L"\n";
        rep += L"줄 번호      : " + std::to_wstring(f.line) + L"\n";
        rep += L"메시지       : " + f.message + L"\n";
        rep += L"힙 영향      : " + hi.category + L"\n";
        rep += L"힙 영향 설명 : " + hi.explanation + L"\n";
        rep += L"----------------------------------------------------------\n";
    }

    // Convert wide string to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, rep.c_str(), -1,
                                      nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) {
        MessageBoxW(g_hWnd, L"텍스트 변환 실패", L"오류", MB_ICONERROR);
        return;
    }
    std::string utf8(utf8Len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, rep.c_str(), -1,
                        &utf8[0], utf8Len, nullptr, nullptr);

    // Write file with UTF-8 BOM
    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hWnd, L"파일을 열 수 없습니다.", L"저장 오류", MB_ICONERROR);
        return;
    }
    const char bom[] = "\xEF\xBB\xBF";
    DWORD bomBytesWritten    = 0;
    BOOL  bomWriteOk         = WriteFile(hFile, bom, 3, &bomBytesWritten, nullptr);
    DWORD contentBytesWritten = 0;
    BOOL  contentWriteOk     = WriteFile(hFile, utf8.c_str(), (DWORD)utf8.size(), &contentBytesWritten, nullptr);
    CloseHandle(hFile);

    if (!bomWriteOk || !contentWriteOk || contentBytesWritten != (DWORD)utf8.size()) {
        MessageBoxW(g_hWnd, L"파일 쓰기에 실패했습니다.", L"저장 오류", MB_ICONERROR);
        return;
    }

    if (g_hStatus)
        SetWindowTextW(g_hStatus, (L"저장 완료: " + std::wstring(filePath)).c_str());
    MessageBoxW(g_hWnd,
                (L"결과를 저장했습니다:\n" + std::wstring(filePath)).c_str(),
                L"저장 완료", MB_ICONINFORMATION);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        auto Make = [&](LPCWSTR cls, LPCWSTR txt, DWORD style, int id) -> HWND {
            HWND h = CreateWindowExW(0, cls, txt,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                0, 0, 10, 10, hWnd, (HMENU)(UINT_PTR)id, hInst, nullptr);
            if (h) SendMessageW(h, WM_SETFONT, (WPARAM)hFont, FALSE);
            return h;
        };

        g_hPath      = Make(L"EDIT",   L"", ES_AUTOHSCROLL | WS_BORDER, IDC_EDIT_PATH);
        g_hBrowse    = Make(L"BUTTON", L"폴더 선택", BS_PUSHBUTTON,      IDC_BTN_BROWSE);
        g_hRecursive = Make(L"BUTTON", L"하위 폴더 포함", BS_AUTOCHECKBOX, IDC_CHECK_RECURSIVE);
        g_hAnalyze   = Make(L"BUTTON", L"분석 시작", BS_DEFPUSHBUTTON,   IDC_BTN_ANALYZE);
        g_hSave      = Make(L"BUTTON", L"저장",      BS_PUSHBUTTON,       IDC_BTN_SAVE);

        // ListView
        g_hList = CreateWindowExW(WS_EX_CLIENTEDGE,
                                   WC_LISTVIEW, L"",
                                   WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
                                   0, 0, 10, 10,
                                   hWnd, (HMENU)(UINT_PTR)IDC_LIST_RESULTS, hInst, nullptr);
        if (g_hList) {
            SendMessageW(g_hList, WM_SETFONT, (WPARAM)hFont, FALSE);
            ListView_SetExtendedListViewStyle(g_hList,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            struct { LPCWSTR hdr; int w; } cols[] = {
                { L"심각도", 80  },
                { L"규칙",   100 },
                { L"파일",   55  },
                { L"메시지", 400 },
                { L"줄",     55  },
            };
            for (int i = 0; i < 5; ++i) {
                LVCOLUMN col = {};
                col.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
                col.cx       = cols[i].w;
                col.pszText  = const_cast<LPWSTR>(cols[i].hdr);
                col.iSubItem = i;
                ListView_InsertColumn(g_hList, i, &col);
            }
        }

        g_hStatus = CreateWindowExW(0, L"STATIC", L"준비",
                                     WS_CHILD | WS_VISIBLE | SS_SUNKEN,
                                     0, 0, 10, 10,
                                     hWnd, (HMENU)(UINT_PTR)IDC_STATIC_STATUS, hInst, nullptr);
        if (g_hStatus) SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)hFont, FALSE);

        return 0;
    }

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            DoLayout(hWnd);
        return 0;

    case WM_COMMAND: {
        int id  = LOWORD(wParam);
        int evt = HIWORD(wParam);
        if (id == IDC_BTN_BROWSE && evt == BN_CLICKED) {
            BROWSEINFOW bi = {};
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"분석할 폴더를 선택하세요";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t dir[MAX_PATH] = {};
                SHGetPathFromIDListW(pidl, dir);
                CoTaskMemFree(pidl);
                if (g_hPath) SetWindowTextW(g_hPath, dir);
            }
        } else if (id == IDC_BTN_ANALYZE && evt == BN_CLICKED) {
            DoAnalyze();
        } else if (id == IDC_BTN_SAVE && evt == BN_CLICKED) {
            DoSave();
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (!hdr) return 0;

        if (hdr->idFrom == IDC_LIST_RESULTS) {
            if (hdr->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    int idx = (int)cd->nmcd.dwItemSpec;
                    if (idx >= 0 && idx < (int)g_findings.size())
                        cd->clrText = SeverityColor(g_findings[idx].severity);
                    return CDRF_NEWFONT;
                }
                return CDRF_DODEFAULT;
            }
            if (hdr->code == NM_DBLCLK) {
                NMITEMACTIVATE* ia = reinterpret_cast<NMITEMACTIVATE*>(lParam);
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
                return 0;
            }
        }
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = reinterpret_cast<MINMAXINFO*>(lParam);
        mm->ptMinTrackSize = { 640, 480 };
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ─── WinMain ─────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // Initialize common controls (ListView, etc.)
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC  = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SHA_GUI_V2";
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"윈도우 클래스 등록 실패", L"오류", MB_ICONERROR);
        return 1;
    }

    g_hWnd = CreateWindowExW(0, L"SHA_GUI_V2",
                              L"Static Heap Analyzer",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 960, 680,
                              nullptr, nullptr, hInst, nullptr);
    if (!g_hWnd) {
        MessageBoxW(nullptr, L"윈도우 생성 실패", L"오류", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(g_hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    CoUninitialize();
    return (int)msg.wParam;
}
