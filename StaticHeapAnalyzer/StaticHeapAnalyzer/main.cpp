#include "RuleEngine.h"
#include "TokenScanner.h"
#include "report/ReportGenerator.h"

#include <algorithm>
#include <experimental/filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::experimental::filesystem;

namespace {
bool IsSourceFile(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".cpp" || ext == ".cc" || ext == ".c" || ext == ".h" || ext == ".hpp";
}

void CollectFiles(const fs::path& src, bool recursive, std::vector<std::string>& files) {
    if (fs::is_regular_file(src)) {
        if (IsSourceFile(src)) {
            files.push_back(src.string());
        }
        return;
    }

    if (!fs::is_directory(src)) {
        return;
    }

    if (recursive) {
        for (fs::recursive_directory_iterator it(src), end; it != end; ++it) {
            if (fs::is_regular_file(*it) && IsSourceFile(it->path())) {
                files.push_back(it->path().string());
            }
        }
    } else {
        for (fs::directory_iterator it(src), end; it != end; ++it) {
            if (fs::is_regular_file(*it) && IsSourceFile(it->path())) {
                files.push_back(it->path().string());
            }
        }
    }
}

bool FindingLess(const Finding& a, const Finding& b) {
    int ra = SeverityRank(a.severity);
    int rb = SeverityRank(b.severity);
    if (ra != rb) return ra < rb;
    if (a.file != b.file) return a.file < b.file;
    if (a.line != b.line) return a.line < b.line;
    return a.rule < b.rule;
}

// 심각도/경로/줄번호 정렬 후 (file,line,rule) 기준 중복 제거 (설계 render)
void SortAndDedup(std::vector<Finding>& findings) {
    std::sort(findings.begin(), findings.end(), FindingLess);
    std::vector<Finding> unique;
    for (size_t i = 0; i < findings.size(); ++i) {
        const Finding& f = findings[i];
        if (!unique.empty()) {
            const Finding& p = unique.back();
            if (p.file == f.file && p.line == f.line && p.rule == f.rule) {
                continue;
            }
        }
        unique.push_back(f);
    }
    findings.swap(unique);
}

bool HasHighSeverity(const std::vector<Finding>& findings) {
    for (size_t i = 0; i < findings.size(); ++i) {
        if (findings[i].severity == Severity::CRITICAL || findings[i].severity == Severity::HIGH) {
            return true;
        }
    }
    return false;
}

std::set<std::string> ParseRules(const std::string& rulesArg) {
    std::set<std::string> rules;
    std::string token;
    for (size_t i = 0; i < rulesArg.size(); ++i) {
        if (rulesArg[i] == ',') {
            if (!token.empty()) {
                rules.insert(token);
                token.clear();
            }
        } else {
            token += rulesArg[i];
        }
    }
    if (!token.empty()) {
        rules.insert(token);
    }
    return rules;
}
}

int main(int argc, char* argv[]) {
    std::string src;
    std::string reportType = "console";
    std::set<std::string> rules;
    bool recursive = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--src" && i + 1 < argc) {
            src = argv[++i];
        } else if (arg == "--report" && i + 1 < argc) {
            reportType = argv[++i];
        } else if (arg == "--rules" && i + 1 < argc) {
            rules = ParseRules(argv[++i]);
        } else if (arg == "--recursive") {
            recursive = true;
        }
    }

    if (src.empty()) {
        std::cerr << "Usage: StaticHeapAnalyzer.exe --src <file-or-dir> [--report console|html|json] [--rules R1,R2,...,ALLOC-001,...] [--recursive]" << std::endl;
        return 1;
    }

    std::vector<std::string> files;
    CollectFiles(fs::path(src), recursive, files);
    if (files.empty()) {
        std::cerr << "No source files found." << std::endl;
        return 1;
    }

    TokenScanner scanner;
    RuleEngine engine;
    engine.SetEnabledRules(rules);

    std::vector<Finding> findings;
    for (size_t i = 0; i < files.size(); ++i) {
        std::vector<TokenLine> tokens;
        std::string error;
        if (!scanner.ScanFile(files[i], tokens, error)) {
            std::cerr << error << std::endl;
            continue;
        }

        std::vector<Finding> fileFindings = engine.AnalyzeFile(files[i], tokens);
        findings.insert(findings.end(), fileFindings.begin(), fileFindings.end());
    }

    SortAndDedup(findings);

    if (reportType == "html") {
        std::string error;
        if (!ReportGenerator::WriteHtml(findings, "StaticHeapAnalyzerReport.html", error)) {
            std::cerr << error << std::endl;
            return 1;
        }
        std::cout << "HTML report written: StaticHeapAnalyzerReport.html" << std::endl;
    } else if (reportType == "json") {
        std::string error;
        if (!ReportGenerator::WriteJson(findings, "StaticHeapAnalyzerReport.json", error)) {
            std::cerr << error << std::endl;
            return 1;
        }
        std::cout << "JSON report written: StaticHeapAnalyzerReport.json" << std::endl;
    } else {
        ReportGenerator::PrintConsole(findings);
    }

    // FR-7: HIGH(이상) 발견 시 종료 코드 2 (빌드 파이프라인 통합용)
    return HasHighSeverity(findings) ? 2 : 0;
}
