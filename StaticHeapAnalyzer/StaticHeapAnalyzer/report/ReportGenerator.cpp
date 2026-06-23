#include "ReportGenerator.h"

#include "Severity.h"

#include <fstream>
#include <iostream>

namespace {
std::string EscapeJson(const std::string& input) {
    std::string out;
    for (size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string EscapeHtml(const std::string& input) {
    std::string out;
    for (size_t i = 0; i < input.size(); ++i) {
        switch (input[i]) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += input[i]; break;
        }
    }
    return out;
}
}

void ReportGenerator::PrintConsole(const std::vector<Finding>& findings) {
    int high = 0, medium = 0, info = 0, other = 0;

    for (size_t i = 0; i < findings.size(); ++i) {
        const Finding& f = findings[i];
        std::cout << "[" << SeverityToString(f.severity) << "] "
                  << f.file << ":" << f.line << "  (" << f.rule << ")" << std::endl;
        if (!f.code.empty()) {
            std::cout << "    > " << f.code << std::endl;
        }
        if (!f.message.empty()) {
            std::cout << "    원인: " << f.message << std::endl;
        }
        if (!f.fix.empty()) {
            std::cout << "    수정: " << f.fix << std::endl;
        }
        std::cout << std::endl;

        switch (f.severity) {
        case Severity::CRITICAL:
        case Severity::HIGH: ++high; break;
        case Severity::MEDIUM: ++medium; break;
        case Severity::INFO: ++info; break;
        default: ++other; break;
        }
    }

    std::cout << "요약: 총 " << findings.size() << "건 "
              << "(HIGH/CRITICAL " << high
              << ", MEDIUM " << medium
              << ", INFO " << info;
    if (other > 0) {
        std::cout << ", 기타 " << other;
    }
    std::cout << ")" << std::endl;
}

bool ReportGenerator::WriteHtml(const std::vector<Finding>& findings, const std::string& path, std::string& error) {
    std::ofstream out(path.c_str());
    if (!out.is_open()) {
        error = "cannot write html report: " + path;
        return false;
    }

    out << "<html><head><meta charset=\"utf-8\"><title>StaticHeapAnalyzer Report</title></head><body>";
    out << "<h2>StaticHeapAnalyzer Findings</h2>";
    out << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"6\">";
    out << "<tr><th>Severity</th><th>File</th><th>Line</th><th>Rule</th><th>Code</th><th>원인</th><th>수정</th></tr>";

    for (size_t i = 0; i < findings.size(); ++i) {
        const Finding& f = findings[i];
        out << "<tr style=\"background:" << SeverityToHtmlColor(f.severity) << ";\">";
        out << "<td>" << SeverityToString(f.severity) << "</td>";
        out << "<td>" << EscapeHtml(f.file) << "</td>";
        out << "<td>" << f.line << "</td>";
        out << "<td>" << EscapeHtml(f.rule) << "</td>";
        out << "<td><code>" << EscapeHtml(f.code) << "</code></td>";
        out << "<td>" << EscapeHtml(f.message) << "</td>";
        out << "<td>" << EscapeHtml(f.fix) << "</td>";
        out << "</tr>";
    }

    out << "</table></body></html>";
    return true;
}

bool ReportGenerator::WriteJson(const std::vector<Finding>& findings, const std::string& path, std::string& error) {
    std::ofstream out(path.c_str());
    if (!out.is_open()) {
        error = "cannot write json report: " + path;
        return false;
    }

    out << "[\n";
    for (size_t i = 0; i < findings.size(); ++i) {
        const Finding& f = findings[i];
        out << "  {\n";
        out << "    \"severity\": \"" << SeverityToString(f.severity) << "\",\n";
        out << "    \"file\": \"" << EscapeJson(f.file) << "\",\n";
        out << "    \"line\": " << f.line << ",\n";
        out << "    \"rule\": \"" << EscapeJson(f.rule) << "\",\n";
        out << "    \"code\": \"" << EscapeJson(f.code) << "\",\n";
        out << "    \"message\": \"" << EscapeJson(f.message) << "\",\n";
        out << "    \"fix\": \"" << EscapeJson(f.fix) << "\"\n";
        out << "  }";
        if (i + 1 < findings.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "]\n";

    return true;
}
