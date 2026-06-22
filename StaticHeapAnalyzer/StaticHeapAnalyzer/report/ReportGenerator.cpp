#include "ReportGenerator.h"

#include "Severity.h"

#include <fstream>
#include <iostream>

namespace {
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
    for (size_t i = 0; i < findings.size(); ++i) {
        const Finding& f = findings[i];
        std::cout << "[" << SeverityToString(f.severity) << "] "
                  << f.file << ":" << f.line << "  "
                  << f.rule << "  " << f.message << std::endl;
    }
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
    out << "<tr><th>Severity</th><th>File</th><th>Line</th><th>Rule</th><th>Message</th></tr>";

    for (size_t i = 0; i < findings.size(); ++i) {
        const Finding& f = findings[i];
        out << "<tr style=\"background:" << SeverityToHtmlColor(f.severity) << ";\">";
        out << "<td>" << SeverityToString(f.severity) << "</td>";
        out << "<td>" << EscapeHtml(f.file) << "</td>";
        out << "<td>" << f.line << "</td>";
        out << "<td>" << EscapeHtml(f.rule) << "</td>";
        out << "<td>" << EscapeHtml(f.message) << "</td>";
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
        out << "    \"file\": \"" << f.file << "\",\n";
        out << "    \"line\": " << f.line << ",\n";
        out << "    \"rule\": \"" << f.rule << "\",\n";
        out << "    \"message\": \"" << f.message << "\"\n";
        out << "  }";
        if (i + 1 < findings.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "]\n";

    return true;
}
