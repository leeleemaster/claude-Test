#include "ScopeRuleAnalyzer.h"

#include <map>
#include <regex>
#include <sstream>
#include <vector>

namespace {

enum Kind { K_NEW, K_NEW_ARR, K_MALLOC, K_UNKNOWN };
enum FreeMode { F_DELETE, F_DELETE_ARR, F_FREE };

struct Rec {
    Kind kind;
    int line;       // 할당(또는 마지막 대입) 줄
    bool deleted;
};

std::string IntToStr(int v) {
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

class ScopeTracker {
public:
    ScopeTracker(const std::string& file, std::vector<Finding>& findings)
        : m_file(file), m_findings(findings) {
        m_frames.push_back(std::map<std::string, Rec>());  // 전역 프레임
    }

    void PushFrame() { m_frames.push_back(std::map<std::string, Rec>()); }

    void PopFrame() {
        if (m_frames.size() > 1) {
            m_frames.pop_back();
        }
    }

    void RecordAlloc(const std::string& var, Kind kind, int line) {
        Rec r;
        r.kind = kind;
        r.line = line;
        r.deleted = false;
        m_frames.back()[var] = r;
    }

    // 일반 대입: 기존 변수면 deleted 해제 + 종류 미상, 없으면 현재 프레임에 등록
    void RecordAssign(const std::string& var, int line) {
        Rec* r = FindMutable(var);
        if (r != 0) {
            r->kind = K_UNKNOWN;
            r->line = line;
            r->deleted = false;
        } else {
            Rec nr;
            nr.kind = K_UNKNOWN;
            nr.line = line;
            nr.deleted = false;
            m_frames.back()[var] = nr;
        }
    }

    void RecordFree(const std::string& var, FreeMode mode, int line, const std::string& codeLine) {
        Rec* r = FindMutable(var);
        if (r == 0) {
            return;  // 다른 함수/스코프에서 할당된 변수 — 추적 불가
        }

        if (r->deleted) {
            Emit(Severity::HIGH, line, "R9-double-free",
                 "(할당: " + IntToStr(r->line) + "번 줄) 변수 '" + var + "' 가 재할당 없이 2회 해제됨 — free list 가 손상될 수 있음",
                 "해제 후 NULL 대입, 또는 중복 해제 제거", codeLine);
            r->deleted = true;
            return;
        }

        if (mode == F_DELETE) {
            if (r->kind == K_NEW_ARR) {
                Emit(Severity::HIGH, line, "R2-new-delete-mismatch",
                     "(할당: " + IntToStr(r->line) + "번 줄) new[] 로 할당했는데 delete 로 해제 — 배열 헤더/소멸자 처리가 어긋나 힙이 손상",
                     "delete[] " + var + "; 로 해제", codeLine);
            } else if (r->kind == K_MALLOC) {
                Emit(Severity::HIGH, line, "R3-new-free-mix",
                     "(할당: " + IntToStr(r->line) + "번 줄) malloc 계열로 할당했는데 delete 로 해제 — 할당자 관리 구조가 달라 free list 파괴",
                     "free(" + var + "); 로 해제", codeLine);
            }
        } else if (mode == F_DELETE_ARR) {
            if (r->kind == K_NEW) {
                Emit(Severity::HIGH, line, "R2-new-delete-mismatch",
                     "(할당: " + IntToStr(r->line) + "번 줄) new 로 할당했는데 delete[] 로 해제 — 배열 처리가 어긋나 힙이 손상",
                     "delete " + var + "; 로 해제", codeLine);
            } else if (r->kind == K_MALLOC) {
                Emit(Severity::HIGH, line, "R3-new-free-mix",
                     "(할당: " + IntToStr(r->line) + "번 줄) malloc 계열로 할당했는데 delete[] 로 해제 — 할당자 관리 구조가 달라 free list 파괴",
                     "free(" + var + "); 로 해제", codeLine);
            }
        } else {  // F_FREE
            if (r->kind == K_NEW || r->kind == K_NEW_ARR) {
                Emit(Severity::HIGH, line, "R3-new-free-mix",
                     "(할당: " + IntToStr(r->line) + "번 줄) new 로 할당했는데 free() 로 해제 — 두 할당자의 내부 관리 구조가 달라 free list 파괴",
                     "delete" + std::string(r->kind == K_NEW_ARR ? "[] " : " ") + var + "; 로 해제", codeLine);
            }
        }

        r->deleted = true;
    }

private:
    Rec* FindMutable(const std::string& var) {
        for (size_t i = m_frames.size(); i-- > 0;) {
            std::map<std::string, Rec>::iterator it = m_frames[i].find(var);
            if (it != m_frames[i].end()) {
                return &it->second;
            }
        }
        return 0;
    }

    void Emit(Severity sev, int line, const std::string& rule,
              const std::string& msg, const std::string& fix, const std::string& code) {
        Finding f;
        f.severity = sev;
        f.file = m_file;
        f.line = line;
        f.rule = rule;
        f.message = msg;
        f.fix = fix;
        f.code = code;
        m_findings.push_back(f);
    }

    std::string m_file;
    std::vector<Finding>& m_findings;
    std::vector<std::map<std::string, Rec> > m_frames;
};

// 하나의 문(statement)을 분류하여 이벤트 처리
void ProcessStatement(const std::string& stmt, int line, const std::string& codeLine, ScopeTracker& tracker) {
    static const std::regex arrNewRe("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*new\\b[^;]*\\[");
    static const std::regex scalarNewRe("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*new\\s+[A-Za-z_]");
    static const std::regex mallocRe("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(?:\\([^)]*\\)\\s*)?(?:malloc|calloc|realloc|_aligned_malloc|_recalloc)\\s*\\(");
    static const std::regex delArrRe("\\bdelete\\s*\\[\\s*\\]\\s*([A-Za-z_][A-Za-z0-9_]*)");
    static const std::regex delScRe("\\bdelete\\s+([A-Za-z_][A-Za-z0-9_]*)");
    static const std::regex freeRe("\\b(?:free|_aligned_free)\\s*\\(\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*\\)");
    static const std::regex assignRe("([A-Za-z_][A-Za-z0-9_]*)\\s*=(?!=)");

    std::smatch m;

    if (std::regex_search(stmt, m, arrNewRe)) {
        tracker.RecordAlloc(m[1].str(), K_NEW_ARR, line);
        return;
    }
    if (std::regex_search(stmt, m, scalarNewRe)) {
        tracker.RecordAlloc(m[1].str(), K_NEW, line);
        return;
    }
    if (std::regex_search(stmt, m, mallocRe)) {
        tracker.RecordAlloc(m[1].str(), K_MALLOC, line);
        return;
    }
    if (std::regex_search(stmt, m, delArrRe)) {
        tracker.RecordFree(m[1].str(), F_DELETE_ARR, line, codeLine);
        return;
    }
    if (std::regex_search(stmt, m, delScRe)) {
        const std::string var = m[1].str();
        if (var != "this") {
            tracker.RecordFree(var, F_DELETE, line, codeLine);
        }
        return;
    }
    if (std::regex_search(stmt, m, freeRe)) {
        tracker.RecordFree(m[1].str(), F_FREE, line, codeLine);
        return;
    }
    if (std::regex_search(stmt, m, assignRe)) {
        const std::string var = m[1].str();
        if (var != "this") {
            tracker.RecordAssign(var, line);
        }
        return;
    }
}

}  // namespace

void ScopeRuleAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    ScopeTracker tracker(file, findings);

    std::string stmt;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const TokenLine& t = tokens[i];
        const std::string& s = t.clean;

        for (size_t k = 0; k < s.size(); ++k) {
            const char c = s[k];
            if (c == '{') {
                ProcessStatement(stmt, t.line, t.text, tracker);
                stmt.clear();
                tracker.PushFrame();
            } else if (c == '}') {
                ProcessStatement(stmt, t.line, t.text, tracker);
                stmt.clear();
                tracker.PopFrame();
            } else if (c == ';') {
                ProcessStatement(stmt, t.line, t.text, tracker);
                stmt.clear();
            } else {
                stmt += c;
            }
        }
        stmt += ' ';  // 줄바꿈을 토큰 구분 공백으로
    }
}
