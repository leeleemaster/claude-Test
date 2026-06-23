#include "CtorInitAnalyzer.h"

#include <map>
#include <regex>

void CtorInitAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings) {
    std::regex classRegex("\\bclass\\s+([A-Za-z_][A-Za-z0-9_]*)");
    std::regex ptrMemberRegex("[A-Za-z_][A-Za-z0-9_:<>]*\\s*\\*\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*;");

    bool inClass = false;
    std::string className;
    std::map<std::string, int> ptrMembers;
    std::map<std::string, bool> initialized;
    std::map<std::string, int> usedLine;
    std::map<std::string, std::string> usedText;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& line = tokens[i].clean;
        std::smatch match;

        if (!inClass && std::regex_search(line, match, classRegex)) {
            inClass = true;
            className = match[1].str();
            ptrMembers.clear();
            initialized.clear();
            usedLine.clear();
            continue;
        }

        if (!inClass) {
            continue;
        }

        if (std::regex_search(line, match, ptrMemberRegex)) {
            std::string member = match[1].str();
            ptrMembers[member] = tokens[i].line;
            initialized[member] = false;
        }

        if (line.find(className + "(") != std::string::npos) {
            for (std::map<std::string, bool>::iterator it = initialized.begin(); it != initialized.end(); ++it) {
                if (line.find(it->first + "(") != std::string::npos || line.find(it->first + " =") != std::string::npos) {
                    it->second = true;
                }
            }
        }

        for (std::map<std::string, int>::const_iterator it = ptrMembers.begin(); it != ptrMembers.end(); ++it) {
            if (line.find(it->first + "->") != std::string::npos) {
                usedLine[it->first] = tokens[i].line;
                usedText[it->first] = tokens[i].text;
            }
            if (line.find(it->first + " =") != std::string::npos) {
                initialized[it->first] = true;
            }
        }

        if (line.find("};") != std::string::npos) {
            for (std::map<std::string, int>::const_iterator it = ptrMembers.begin(); it != ptrMembers.end(); ++it) {
                const std::string& member = it->first;
                if (!initialized[member] && usedLine.find(member) != usedLine.end()) {
                    Finding finding;
                    finding.severity = Severity::HIGH;
                    finding.file = file;
                    finding.line = usedLine[member];
                    finding.rule = "CTOR-001";
                    finding.message = "포인터 멤버 '" + member + "' 를 생성자에서 초기화하지 않은 채 사용 — 미초기화 포인터 역참조로 임의 메모리/힙 손상";
                    finding.fix = "생성자 초기화 리스트 또는 본문에서 '" + member + "' 를 NULL/유효 객체로 초기화";
                    finding.code = usedText.count(member) ? usedText[member] : std::string();
                    findings.push_back(finding);
                }
            }
            inClass = false;
            className.clear();
        }
    }
}
