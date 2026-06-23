#include "ThreadSafetyAnalyzer.h"

#include <regex>
#include <vector>

void ThreadSafetyAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    std::regex staticPtrRegex("\\bstatic\\s+[A-Za-z_][A-Za-z0-9_:<>]*\\s*\\*\\s*([A-Za-z_][A-Za-z0-9_]*)");
    std::vector<std::string> staticPointers;

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::smatch match;
        if (std::regex_search(tokens[i].clean, match, staticPtrRegex)) {
            staticPointers.push_back(match[1].str());
        }
    }

    for (size_t p = 0; p < staticPointers.size(); ++p) {
        const std::string& name = staticPointers[p];
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i].clean.find(name + " = new") == std::string::npos &&
                tokens[i].clean.find("!" + name) == std::string::npos) {
                continue;
            }

            bool hasLock = false;
            size_t begin = (i > 3) ? i - 3 : 0;
            size_t end = (i + 3 < tokens.size()) ? i + 3 : tokens.size() - 1;
            for (size_t j = begin; j <= end; ++j) {
                if (tokens[j].clean.find("lock_guard") != std::string::npos ||
                    tokens[j].clean.find("mutex") != std::string::npos ||
                    tokens[j].clean.find("CriticalSection") != std::string::npos ||
                    tokens[j].clean.find("EnterCriticalSection") != std::string::npos) {
                    hasLock = true;
                    break;
                }
            }

            if (!hasLock) {
                Finding finding;
                finding.severity = Severity::MEDIUM;
                finding.file = file;
                finding.line = tokens[i].line;
                finding.rule = "THREAD-001";
                finding.message = "static/global 포인터 '" + name + "' 접근(지연 초기화 등) 시 Lock 미사용 — 경합으로 이중 할당/해제 발생 가능";
                finding.fix = "lock_guard/CriticalSection 으로 보호하거나 std::call_once 사용";
                finding.code = tokens[i].text;
                findings.push_back(finding);
                break;
            }
        }
    }
}
