#include "LineRuleAnalyzer.h"

#include <regex>

namespace {

Finding MakeFinding(Severity sev, const std::string& file, int line,
                    const std::string& rule, const std::string& msg,
                    const std::string& fix, const std::string& code) {
    Finding f;
    f.severity = sev;
    f.file = file;
    f.line = line;
    f.rule = rule;
    f.message = msg;
    f.fix = fix;
    f.code = code;
    return f;
}

// R1: 경계 검사 없는 복사/포맷 함수
void CheckOverrun(const std::string& file, const TokenLine& t, std::vector<Finding>& findings) {
    static const std::regex re(
        "\\b(strcpy|strcat|sprintf|gets|wcscpy|wcscat|lstrcpy|lstrcat|"
        "wsprintf|_tcscpy|_tcscat|_stprintf|strncpy)\\s*\\(");
    std::smatch m;
    if (std::regex_search(t.clean, m, re)) {
        const std::string fn = m[1].str();
        findings.push_back(MakeFinding(
            Severity::HIGH, file, t.line, "R1-overrun",
            "경계 검사 없는 복사/포맷 함수 '" + fn + "' 호출 — 대상 버퍼를 넘겨 쓰면 인접 힙 블록 메타데이터가 손상될 수 있음",
            "길이 제한 버전(strncpy_s / StringCchCopy / _snprintf 등) 사용 및 대상 버퍼 크기 검증",
            t.text));
    }
}

// R4: memset/memcpy 대상과 sizeof 인자가 동일 식별자
void CheckSizeofPtr(const std::string& file, const TokenLine& t, std::vector<Finding>& findings) {
    static const std::regex re(
        "\\b(memset|memcpy)\\s*\\(\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*,"
        "[^;]*sizeof\\s*\\(\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*\\)");
    std::smatch m;
    if (std::regex_search(t.clean, m, re) && m[2].str() == m[3].str()) {
        const std::string v = m[2].str();
        findings.push_back(MakeFinding(
            Severity::MEDIUM, file, t.line, "R4-sizeof-ptr",
            "대상과 sizeof 인자가 동일 식별자 '" + v + "' — 그 변수가 포인터면 sizeof 가 포인터 크기(4/8B)가 되어 잘못된 크기로 동작 (배열이면 정상)",
            "버퍼 실제 크기(원소수 * sizeof(원소형) 또는 정의된 길이 상수) 사용",
            t.text));
    }
}

// R5: memset 길이 인자가 리터럴 0
void CheckMemsetZero(const std::string& file, const TokenLine& t, std::vector<Finding>& findings) {
    static const std::regex re(
        "\\bmemset\\s*\\(\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*,[^,;]*,\\s*0\\s*\\)");
    std::smatch m;
    if (std::regex_search(t.clean, m, re)) {
        findings.push_back(MakeFinding(
            Severity::MEDIUM, file, t.line, "R5-memset-zero",
            "memset 의 길이 인자가 리터럴 0 — 인자 순서(대상,값,길이) 혼동 의심 (실제로 아무것도 지워지지 않음)",
            "memset(dst, 0, size) 형태로 값/길이 인자 위치 점검",
            t.text));
    }
}

}  // namespace

void LineRuleAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    static const std::regex forLeRe(
        "\\bfor\\s*\\([^;]*;[^;]*?\\b([A-Za-z_][A-Za-z0-9_]*)\\s*<=\\s*[^;]*;");
    static const std::regex getBufRe("(\\.|->)\\s*GetBuffer\\s*\\(");
    static const std::regex releaseRe("\\bReleaseBuffer\\s*\\(");
    static const std::regex fnRe("\\b([A-Za-z_][A-Za-z0-9_:~]*)\\s*\\([^;{]*\\)\\s*\\{");

    // 함수 스코프 추적용 상태 (R7)
    bool inFunction = false;
    int funcDepth = 0;
    std::vector<int> getBufferLines;
    bool releaseSeen = false;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const TokenLine& t = tokens[i];

        // --- 단순 라인 규칙 ---
        CheckOverrun(file, t, findings);
        CheckSizeofPtr(file, t, findings);
        CheckMemsetZero(file, t, findings);

        // --- R6: off-by-one ---
        std::smatch fm;
        if (std::regex_search(t.clean, fm, forLeRe)) {
            const std::string counter = fm[1].str();
            std::regex idxRe("[A-Za-z_][A-Za-z0-9_]*\\s*\\[\\s*" + counter + "\\s*\\]");
            size_t end = i + 8;
            if (end > tokens.size()) {
                end = tokens.size();
            }
            for (size_t j = i; j < end; ++j) {
                if (std::regex_search(tokens[j].clean, idxRe)) {
                    findings.push_back(MakeFinding(
                        Severity::MEDIUM, file, tokens[j].line, "R6-off-by-one",
                        "for 루프 조건 'i <= N' 과 동일 카운터 '" + counter + "' 로 배열 인덱싱 — 마지막 한 칸을 넘겨 써서 경계 침범 가능",
                        "루프 조건을 'i < N' 으로 수정하거나 인덱스 상한을 재확인",
                        tokens[j].text));
                    break;
                }
            }
        }

        // --- R7: GetBuffer / ReleaseBuffer 짝 추적 (함수 스코프 단위) ---
        std::smatch fnm;
        if (!inFunction && std::regex_search(t.clean, fnm, fnRe)) {
            const std::string name = fnm[1].str();
            if (name != "if" && name != "for" && name != "while" && name != "switch") {
                inFunction = true;
                funcDepth = 0;
                getBufferLines.clear();
                releaseSeen = false;
            }
        }

        if (inFunction) {
            if (std::regex_search(t.clean, getBufRe)) {
                getBufferLines.push_back(t.line);
            }
            if (std::regex_search(t.clean, releaseRe)) {
                releaseSeen = true;
            }

            for (size_t k = 0; k < t.clean.size(); ++k) {
                if (t.clean[k] == '{') {
                    ++funcDepth;
                } else if (t.clean[k] == '}') {
                    --funcDepth;
                    if (funcDepth <= 0) {
                        // 함수 종료 → 판정
                        if (!releaseSeen) {
                            for (size_t g = 0; g < getBufferLines.size(); ++g) {
                                findings.push_back(MakeFinding(
                                    Severity::MEDIUM, file, getBufferLines[g], "R7-getbuffer",
                                    "CString::GetBuffer() 호출에 짝이 되는 ReleaseBuffer() 가 같은 함수에 없음 — 요청 길이 초과 기록·해제 누락 시 CString 내부 힙 손상",
                                    "기록 후 ReleaseBuffer() 호출 (필요 길이를 GetBuffer(n) 인자로 충분히 확보)",
                                    ""));
                            }
                        }
                        inFunction = false;
                        funcDepth = 0;
                        getBufferLines.clear();
                        releaseSeen = false;
                        break;
                    }
                }
            }
        }
    }
}
