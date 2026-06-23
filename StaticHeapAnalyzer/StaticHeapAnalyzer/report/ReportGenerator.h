#ifndef REPORT_GENERATOR_H
#define REPORT_GENERATOR_H

#include "../RuleEngine.h"

#include <string>
#include <vector>

class ReportGenerator {
public:
    static void PrintConsole(const std::vector<Finding>& findings);
    static bool WriteHtml(const std::vector<Finding>& findings, const std::string& path, std::string& error);
    static bool WriteJson(const std::vector<Finding>& findings, const std::string& path, std::string& error);
};

#endif
