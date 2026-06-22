#include "StackHeapAnalyzer.h"

#include <map>
#include <regex>

void StackHeapAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    std::regex ptrToStackRegex("[A-Za-z_][A-Za-z0-9_:<>]*\\s*\\*\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*&([A-Za-z_][A-Za-z0-9_]*)\\s*;");
    std::regex deleteRegex("\\bdelete\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*;");

    std::map<std::string, std::string> ptrToStack;

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::smatch match;
        if (std::regex_search(tokens[i].text, match, ptrToStackRegex)) {
            ptrToStack[match[1].str()] = match[2].str();
            continue;
        }

        if (std::regex_search(tokens[i].text, match, deleteRegex)) {
            std::string ptr = match[1].str();
            if (ptrToStack.find(ptr) != ptrToStack.end()) {
                Finding finding;
                finding.severity = Severity::CRITICAL;
                finding.file = file;
                finding.line = tokens[i].line;
                finding.rule = "STACK-001";
                finding.message = "스택 객체 주소를 가진 포인터 '" + ptr + "' 에 delete 사용";
                findings.push_back(finding);
            }
        }
    }
}
