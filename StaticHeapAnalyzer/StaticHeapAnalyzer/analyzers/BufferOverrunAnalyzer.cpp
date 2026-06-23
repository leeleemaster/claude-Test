#include "BufferOverrunAnalyzer.h"

#include <regex>

void BufferOverrunAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    std::regex loopRegex("\\b(for|while)\\s*\\(");
    std::regex allocRegex("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*new\\s+char\\s*\\[");

    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!std::regex_search(tokens[i].text, loopRegex)) {
            continue;
        }

        std::string allocatedVar;
        size_t end = i + 15;
        if (end > tokens.size()) {
            end = tokens.size();
        }

        for (size_t j = i; j < end; ++j) {
            std::smatch m;
            if (allocatedVar.empty() && std::regex_search(tokens[j].text, m, allocRegex)) {
                allocatedVar = m[1].str();
            }
            if (allocatedVar.empty()) {
                continue;
            }

            if (tokens[j].text.find("strcpy(" + allocatedVar + ",") != std::string::npos ||
                tokens[j].text.find("memcpy(" + allocatedVar + ",") != std::string::npos ||
                tokens[j].text.find("sprintf(" + allocatedVar + ",") != std::string::npos) {
                Finding finding;
                finding.severity = Severity::HIGH;
                finding.file = file;
                finding.line = tokens[j].line;
                finding.rule = "HEAP-002";
                finding.message = "반복 할당 버퍼에 경계 검사 없는 복사 함수 사용";
                findings.push_back(finding);
                break;
            }
        }
    }
}
