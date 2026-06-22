#include "AllocatorAnalyzer.h"

#include <regex>

void AllocatorAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    std::regex opNewRegex("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(\\([^\\)]*\\)\\s*)?::operator\\s+new\\s*\\(");

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::smatch match;
        if (!std::regex_search(tokens[i].text, match, opNewRegex)) {
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
            if (tokens[j].text.find("new (" + var + ")") != std::string::npos) {
                placementInit = true;
            }
            if (tokens[j].text.find(var + "->") != std::string::npos ||
                tokens[j].text.find("*" + var) != std::string::npos) {
                usedDirectly = true;
            }
        }

        if (usedDirectly && !placementInit) {
            Finding finding;
            finding.severity = Severity::HIGH;
            finding.file = file;
            finding.line = tokens[i].line;
            finding.rule = "ALLOC-001";
            finding.message = "::operator new 직접 호출 후 placement new 초기화 없이 사용";
            findings.push_back(finding);
        }
    }
}
