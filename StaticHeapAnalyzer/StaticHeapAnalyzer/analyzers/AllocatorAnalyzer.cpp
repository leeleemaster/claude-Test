#include "AllocatorAnalyzer.h"

#include <regex>

void AllocatorAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    std::regex opNewRegex("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(\\([^\\)]*\\)\\s*)?::operator\\s+new\\s*\\(");

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::smatch match;
        if (!std::regex_search(tokens[i].clean, match, opNewRegex)) {
            continue;
        }

        std::string var = match[1].str();
        bool placementInit = false;
        bool usedDirectly = false;

        size_t end = i + 6;
        if (end > tokens.size()) {
            end = tokens.size();
        }

        for (size_t j = i; j < end; ++j) {
            if (tokens[j].clean.find("new (" + var + ")") != std::string::npos) {
                placementInit = true;
            }
            if (tokens[j].clean.find(var + "->") != std::string::npos ||
                tokens[j].clean.find("*" + var) != std::string::npos) {
                usedDirectly = true;
            }
        }

        if (usedDirectly && !placementInit) {
            Finding finding;
            finding.severity = Severity::HIGH;
            finding.file = file;
            finding.line = tokens[i].line;
            finding.rule = "ALLOC-001";
            finding.message = "::operator new 직접 호출 후 placement new 초기화 없이 사용 — 생성자 미실행 객체 접근으로 힙/객체 상태 손상";
            finding.fix = "::operator new 결과에 placement new(T(...))로 생성자 호출 후 사용";
            finding.code = tokens[i].text;
            findings.push_back(finding);
        }
    }
}
